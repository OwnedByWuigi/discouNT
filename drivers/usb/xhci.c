#include "usb_internal.h"
#include "cpu.h"
#include "usb_msc.h"
#include "mm/vmm.h"
#include "core/util.h"
#include "serial.h"

#define XHCI_PAGE_SIZE 4096U
#define XHCI_MAX_DEVICES 8
#define XHCI_RING_TRBS 256
#define XHCI_CMD_RUN 1U
#define XHCI_CMD_RESET 2U
#define XHCI_STS_HALTED 1U
#define XHCI_STS_CNR 0x800U
#define XHCI_PORTSC 0x400U
#define XHCI_PORT_STRIDE 0x10U
#define XHCI_PORT_CCS 1U
#define XHCI_PORT_PED 2U
#define XHCI_PORT_RESET 0x10U
#define XHCI_PORT_CHANGES 0x00FE0000U
#define XHCI_TRB_CYCLE 1U
#define XHCI_TRB_IOC 0x20U
#define XHCI_TRB_IDT 0x40U
#define XHCI_TRB_TYPE(n) ((uint32_t)(n) << 10)
#define XHCI_COMPLETION_SUCCESS 1U
#define USB_DESC_DEVICE 1
#define USB_DESC_CONFIGURATION 2
#define USB_DESC_INTERFACE 4
#define USB_DESC_ENDPOINT 5
#define USB_CLASS_MASS_STORAGE 8

typedef struct __attribute__((packed)) {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} XHCI_TRB;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint32_t size;
    uint32_t reserved;
} XHCI_ERST_ENTRY;

typedef struct {
    XHCI_TRB *trbs;
    uint16_t enqueue;
    uint8_t cycle;
} XHCI_RING;

typedef struct _XHCI_STATE XHCI_STATE;
typedef struct {
    XHCI_STATE *host;
    uint8_t slot_id, port, speed, configured;
    uint8_t bulk_in, bulk_out, bulk_in_dci, bulk_out_dci;
    uint16_t bulk_in_packet, bulk_out_packet;
    void *output_context, *input_context;
    XHCI_RING ep0, in_ring, out_ring;
} XHCI_DEVICE;

struct _XHCI_STATE {
    USB_CONTROLLER *controller;
    uintptr_t operational, runtime, doorbells;
    uint8_t context_size, event_cycle;
    uint16_t event_dequeue;
    uint32_t max_slots;
    uint64_t *dcbaa;
    XHCI_RING command;
    XHCI_TRB *events;
    XHCI_ERST_ENTRY *erst;
    XHCI_DEVICE devices[XHCI_MAX_DEVICES];
    uint8_t device_count;
};

static uintptr_t physical(const void *p) { return VmmGetPhysicalAddress(p); }
static uint32_t physical_high(const void *p) {
    return (uint32_t)((uint64_t)physical(p) >> 32);
}
static void *dma_page(void) {
    void *page = VmmAllocatePages(1);
    if (page) memset(page, 0, XHCI_PAGE_SIZE);
    return page;
}

static int wait_bits(uintptr_t base, uint32_t offset, uint32_t mask, int set) {
    for (uint32_t timeout = 0; timeout < 2000000; ++timeout) {
        if (!!(UsbMmioRead32(base, offset) & mask) == !!set) return 1;
        CpuRelax();
    }
    return 0;
}

static void legacy_handoff(USB_CONTROLLER *controller, uint32_t hcc) {
    uint32_t offset = ((hcc >> 16) & 0xFFFFU) * 4;
    while (offset >= 0x40 && offset < 0x10000) {
        uint32_t value = UsbMmioRead32(controller->base, offset);
        if ((value & 0xFFU) == 1) {
            UsbMmioWrite32(controller->base, offset, value | (1U << 24));
            for (uint32_t timeout = 0; timeout < 100000; ++timeout)
                if (!(UsbMmioRead32(controller->base, offset) & (1U << 16))) break;
            UsbMmioWrite32(controller->base, offset + 4, 0);
            return;
        }
        uint32_t next = (value >> 8) & 0xFFU;
        if (!next) return;
        offset += next * 4;
    }
}

static int ring_allocate(XHCI_RING *ring) {
    ring->trbs = (XHCI_TRB *)dma_page();
    ring->enqueue = 0;
    ring->cycle = 1;
    if (!ring->trbs) return 0;
    ring->trbs[XHCI_RING_TRBS - 1].parameter = physical(ring->trbs);
    ring->trbs[XHCI_RING_TRBS - 1].control = XHCI_TRB_TYPE(6) | 2 | 1;
    return 1;
}

static XHCI_TRB *ring_push(XHCI_RING *ring, uint64_t parameter,
                           uint32_t status, uint32_t control) {
    XHCI_TRB *trb = &ring->trbs[ring->enqueue];
    trb->parameter = parameter;
    trb->status = status;
    trb->control = control | ring->cycle;
    CpuMemoryBarrier();
    if (++ring->enqueue == XHCI_RING_TRBS - 1) {
        ring->trbs[XHCI_RING_TRBS - 1].control = XHCI_TRB_TYPE(6) | 2 | ring->cycle;
        ring->enqueue = 0;
        ring->cycle ^= 1;
    }
    return trb;
}

static int wait_event(XHCI_STATE *state, const XHCI_TRB *submitted,
                      uint8_t event_type, uint8_t *slot_id) {
    for (uint32_t timeout = 0; timeout < 4000000; ++timeout) {
        XHCI_TRB *event = &state->events[state->event_dequeue];
        if ((event->control & 1) == state->event_cycle) {
            uint8_t type = (uint8_t)((event->control >> 10) & 0x3F);
            uint8_t completion = (uint8_t)(event->status >> 24);
            uint64_t wanted = submitted ? physical(submitted) : 0;
            int match = type == event_type && (!submitted || (event->parameter & ~0xFULL) == (wanted & ~0xFULL));
            if (slot_id) *slot_id = (uint8_t)(event->control >> 24);
            if (++state->event_dequeue == XHCI_RING_TRBS) {
                state->event_dequeue = 0;
                state->event_cycle ^= 1;
            }
            uintptr_t interrupter = state->runtime + 0x20;
            uint64_t dequeue = physical(&state->events[state->event_dequeue]) | (1ULL << 3);
            UsbMmioWrite32(interrupter, 0x18, (uint32_t)dequeue);
            UsbMmioWrite32(interrupter, 0x1C, (uint32_t)(dequeue >> 32));
            if (match)
                return completion == XHCI_COMPLETION_SUCCESS ||
                       (event_type == 32 && completion == 13); /* short packet */
        }
        CpuRelax();
    }
    SerialPutString("[xHCI] Event timeout\r\n");
    return 0;
}

static int command(XHCI_STATE *state, uint64_t parameter, uint32_t control,
                   uint8_t *slot_id) {
    XHCI_TRB *trb = ring_push(&state->command, parameter, 0, control);
    UsbMmioWrite32(state->doorbells, 0, 0);
    return wait_event(state, trb, 33, slot_id);
}

static uint32_t *input_control(XHCI_DEVICE *device) { return (uint32_t *)device->input_context; }
static uint32_t *input_slot(XHCI_DEVICE *device) { return (uint32_t *)((uint8_t *)device->input_context + device->host->context_size); }
static uint32_t *input_ep(XHCI_DEVICE *device, uint8_t dci) { return (uint32_t *)((uint8_t *)device->input_context + (uint32_t)(dci + 1) * device->host->context_size); }

static void configure_endpoint_context(XHCI_DEVICE *device, uint8_t dci,
                                       uint8_t endpoint_type, uint16_t packet,
                                       XHCI_RING *ring) {
    uint32_t *ep = input_ep(device, dci);
    ep[1] = (3U << 1) | ((uint32_t)endpoint_type << 3) | ((uint32_t)packet << 16);
    uint64_t dequeue = physical(ring->trbs) | 1;
    ep[2] = (uint32_t)dequeue;
    ep[3] = (uint32_t)(dequeue >> 32);
    ep[4] = packet;
}

static int control_transfer(XHCI_DEVICE *device, uint8_t request_type,
                            uint8_t request, uint16_t value, uint16_t index,
                            void *buffer, uint16_t length) {
    uint64_t setup = request_type | ((uint64_t)request << 8) |
                     ((uint64_t)value << 16) | ((uint64_t)index << 32) |
                     ((uint64_t)length << 48);
    uint32_t direction_in = request_type & 0x80;
    uint32_t setup_control = XHCI_TRB_TYPE(2) | XHCI_TRB_IDT |
                             (length ? (direction_in ? 3U : 2U) : 0U) << 16;
    ring_push(&device->ep0, setup, 8, setup_control);
    if (length) ring_push(&device->ep0, physical(buffer), length,
                          XHCI_TRB_TYPE(3) | (direction_in ? (1U << 16) : 0));
    XHCI_TRB *status = ring_push(&device->ep0, 0, 0, XHCI_TRB_TYPE(4) |
                                 XHCI_TRB_IOC | ((!length || !direction_in) ? (1U << 16) : 0));
    UsbMmioWrite32(device->host->doorbells, (uint32_t)device->slot_id * 4, 1);
    return wait_event(device->host, status, 32, 0);
}

static int bulk_transfer(void *context, uint8_t endpoint, void *buffer,
                         uint32_t length) {
    XHCI_DEVICE *device = (XHCI_DEVICE *)context;
    uint8_t dci = (endpoint & 0x80) ? device->bulk_in_dci : device->bulk_out_dci;
    XHCI_RING *ring = (endpoint & 0x80) ? &device->in_ring : &device->out_ring;
    XHCI_TRB *trb = ring_push(ring, physical(buffer), length,
                              XHCI_TRB_TYPE(1) | XHCI_TRB_IOC);
    UsbMmioWrite32(device->host->doorbells, (uint32_t)device->slot_id * 4, dci);
    return wait_event(device->host, trb, 32, 0);
}

static int configure_mass_storage(XHCI_DEVICE *device, uint8_t *config,
                                  uint16_t total_length) {
    uint8_t in = 0, out = 0, in_dci = 0, out_dci = 0;
    uint16_t in_packet = 0, out_packet = 0;
    int mass_interface = 0;
    for (uint16_t offset = 0; offset + 2 <= total_length;) {
        uint8_t length = config[offset], type = config[offset + 1];
        if (length < 2 || offset + length > total_length) break;
        if (type == USB_DESC_INTERFACE && length >= 9)
            mass_interface = config[offset + 5] == USB_CLASS_MASS_STORAGE &&
                             config[offset + 6] == 6 && config[offset + 7] == 0x50;
        else if (mass_interface && type == USB_DESC_ENDPOINT && length >= 7 &&
                 (config[offset + 3] & 3) == 2) {
            uint8_t address = config[offset + 2], number = address & 15;
            uint8_t dci = (uint8_t)(number * 2 + ((address & 0x80) ? 1 : 0));
            uint16_t packet = (uint16_t)(config[offset + 4] | config[offset + 5] << 8);
            if (address & 0x80) { in = address; in_dci = dci; in_packet = packet; }
            else { out = address; out_dci = dci; out_packet = packet; }
        }
        offset += length;
    }
    if (!in || !out) { SerialPutString("[xHCI] Bulk endpoints not found\r\n"); return 0; }
    if (!ring_allocate(&device->in_ring) || !ring_allocate(&device->out_ring)) {
        SerialPutString("[xHCI] Bulk ring allocation failed\r\n"); return 0;
    }
    device->bulk_in = in; device->bulk_out = out;
    device->bulk_in_dci = in_dci; device->bulk_out_dci = out_dci;
    device->bulk_in_packet = in_packet; device->bulk_out_packet = out_packet;
    uint8_t highest = in_dci > out_dci ? in_dci : out_dci;
    uint32_t *control = input_control(device), *slot = input_slot(device);
    memset(device->input_context, 0, XHCI_PAGE_SIZE);
    control[1] = 1U | (1U << in_dci) | (1U << out_dci);
    slot[0] = ((uint32_t)highest << 27) | ((uint32_t)device->speed << 20);
    slot[1] = (uint32_t)(device->port + 1) << 16;
    configure_endpoint_context(device, out_dci, 2, out_packet, &device->out_ring);
    configure_endpoint_context(device, in_dci, 6, in_packet, &device->in_ring);
    if (!command(device->host, physical(device->input_context),
                 XHCI_TRB_TYPE(12) | ((uint32_t)device->slot_id << 24), 0)) {
        SerialPutString("[xHCI] Configure Endpoint failed\r\n"); return 0;
    }
    if (!control_transfer(device, 0, 9, config[5], 0, 0, 0)) {
        SerialPutString("[xHCI] Set Configuration failed\r\n"); return 0;
    }
    device->configured = 1;
    if (!UsbMscAttach(device, bulk_transfer, in, out)) {
        SerialPutString("[xHCI] SCSI capacity probe failed\r\n"); return 0;
    }
    return 1;
}

static int enumerate_port(XHCI_STATE *state, uint8_t port) {
    uintptr_t port_reg = state->operational + XHCI_PORTSC + (uint32_t)port * XHCI_PORT_STRIDE;
    uint32_t portsc = UsbMmioRead32(port_reg, 0);
    if (!(portsc & XHCI_PORT_CCS) || state->device_count >= XHCI_MAX_DEVICES) return 0;
    UsbMmioWrite32(port_reg, 0, (portsc & ~XHCI_PORT_CHANGES) | XHCI_PORT_RESET);
    if (!wait_bits(port_reg, 0, XHCI_PORT_RESET, 0) || !wait_bits(port_reg, 0, XHCI_PORT_PED, 1)) {
        SerialPutString("[xHCI] Port reset failed\r\n"); return 0;
    }
    portsc = UsbMmioRead32(port_reg, 0);
    XHCI_DEVICE *device = &state->devices[state->device_count];
    memset(device, 0, sizeof(*device)); device->host = state; device->port = port;
    device->speed = (uint8_t)((portsc >> 10) & 15);
    if (!ring_allocate(&device->ep0)) return 0;
    device->input_context = dma_page(); device->output_context = dma_page();
    if (!device->input_context || !device->output_context) return 0;
    if (!command(state, 0, XHCI_TRB_TYPE(9), &device->slot_id) || !device->slot_id) {
        SerialPutString("[xHCI] Enable Slot failed\r\n"); return 0;
    }
    SerialPutString("[xHCI] Slot enabled\r\n");
    state->dcbaa[device->slot_id] = physical(device->output_context);
    uint32_t *control = input_control(device), *slot = input_slot(device);
    control[1] = 3; slot[0] = (1U << 27) | ((uint32_t)device->speed << 20);
    slot[1] = (uint32_t)(port + 1) << 16;
    uint16_t ep0_packet = device->speed >= 4 ? 512 : (device->speed == 3 ? 64 : 8);
    configure_endpoint_context(device, 1, 4, ep0_packet, &device->ep0);
    CpuMemoryBarrier();
    if (!command(state, physical(device->input_context), XHCI_TRB_TYPE(11) |
                 ((uint32_t)device->slot_id << 24), 0)) {
        SerialPutString("[xHCI] Address Device failed\r\n"); return 0;
    }
    SerialPutString("[xHCI] Device addressed\r\n");
    uint8_t descriptor[18]; memset(descriptor, 0, sizeof(descriptor));
    if (!control_transfer(device, 0x80, 6, USB_DESC_DEVICE << 8, 0,
                          descriptor, sizeof(descriptor))) { SerialPutString("[xHCI] Device descriptor failed\r\n"); return 0; }
    uint8_t header[9];
    if (!control_transfer(device, 0x80, 6, USB_DESC_CONFIGURATION << 8, 0,
                          header, sizeof(header))) { SerialPutString("[xHCI] Configuration header failed\r\n"); return 0; }
    uint16_t total = (uint16_t)(header[2] | header[3] << 8);
    if (total < 9 || total > XHCI_PAGE_SIZE) { SerialPutString("[xHCI] Invalid configuration length\r\n"); return 0; }
    uint8_t *config = (uint8_t *)dma_page(); if (!config) return 0;
    if (!control_transfer(device, 0x80, 6, USB_DESC_CONFIGURATION << 8, 0,
                          config, total)) { SerialPutString("[xHCI] Configuration descriptor failed\r\n"); return 0; }
    state->device_count++;
    if (configure_mass_storage(device, config, total))
        SerialPutString("[xHCI] USB mass-storage device configured\r\n");
    else SerialPutString("[xHCI] Mass-storage configuration failed\r\n");
    return 1;
}

static int initialize_rings(XHCI_STATE *state) {
    state->dcbaa = (uint64_t *)dma_page(); state->events = (XHCI_TRB *)dma_page();
    state->erst = (XHCI_ERST_ENTRY *)dma_page();
    if (!state->dcbaa || !state->events || !state->erst || !ring_allocate(&state->command)) return 0;
    state->event_cycle = 1; state->event_dequeue = 0;
    state->erst[0].address = physical(state->events); state->erst[0].size = XHCI_RING_TRBS;
    UsbMmioWrite32(state->operational, 0x18, (uint32_t)(physical(state->command.trbs) | 1));
    UsbMmioWrite32(state->operational, 0x1C, physical_high(state->command.trbs));
    UsbMmioWrite32(state->operational, 0x30, (uint32_t)physical(state->dcbaa));
    UsbMmioWrite32(state->operational, 0x34, physical_high(state->dcbaa));
    uintptr_t ir = state->runtime + 0x20;
    UsbMmioWrite32(ir, 8, 1); UsbMmioWrite32(ir, 0x10, (uint32_t)physical(state->erst));
    UsbMmioWrite32(ir, 0x14, physical_high(state->erst));
    UsbMmioWrite32(ir, 0x18, (uint32_t)physical(state->events));
    UsbMmioWrite32(ir, 0x1C, physical_high(state->events));
    return 1;
}

int XhciInitialize(USB_CONTROLLER *controller) {
    if (!controller->base) return 0;
    uint8_t length = *(volatile uint8_t *)controller->base;
    if (length < 0x20) return 0;
    uint32_t hcs = UsbMmioRead32(controller->base, 4);
    uint32_t hcc = UsbMmioRead32(controller->base, 0x10);
    legacy_handoff(controller, hcc);
    XHCI_STATE *state = (XHCI_STATE *)dma_page(); if (!state) return 0;
    state->controller = controller; state->operational = controller->base + length;
    state->runtime = controller->base + (UsbMmioRead32(controller->base, 0x18) & ~0x1FU);
    state->doorbells = controller->base + (UsbMmioRead32(controller->base, 0x14) & ~3U);
    state->context_size = (hcc & 4) ? 64 : 32;
    state->max_slots = hcs & 0xFF; if (state->max_slots > 32) state->max_slots = 32;
    controller->ports = (uint8_t)(hcs >> 24); controller->private_data = state;
    UsbMmioWrite32(state->operational, 0, UsbMmioRead32(state->operational, 0) & ~XHCI_CMD_RUN);
    if (!wait_bits(state->operational, 4, XHCI_STS_HALTED, 1)) return 0;
    UsbMmioWrite32(state->operational, 0, XHCI_CMD_RESET);
    if (!wait_bits(state->operational, 0, XHCI_CMD_RESET, 0) ||
        !wait_bits(state->operational, 4, XHCI_STS_CNR, 0)) return 0;
    if (!initialize_rings(state)) return 0;
    UsbMmioWrite32(state->operational, 0x38, state->max_slots);
    UsbMmioWrite32(state->operational, 0, XHCI_CMD_RUN);
    if (!wait_bits(state->operational, 4, XHCI_STS_HALTED, 0)) return 0;
    for (uint8_t port = 0; port < controller->ports; ++port) {
        uint32_t status = UsbMmioRead32(state->operational, XHCI_PORTSC + port * XHCI_PORT_STRIDE);
        if (status & XHCI_PORT_CCS) { UsbLogPort(controller, port, 1); enumerate_port(state, port); }
    }
    SerialPutString("[xHCI] Command/event rings active\r\n");
    return 1;
}

void XhciPoll(USB_CONTROLLER *controller) {
    XHCI_STATE *state = (XHCI_STATE *)controller->private_data;
    if (!state) return;
    for (uint8_t port = 0; port < controller->ports; ++port) {
        uint32_t offset = XHCI_PORTSC + (uint32_t)port * XHCI_PORT_STRIDE;
        uint32_t status = UsbMmioRead32(state->operational, offset);
        if (status & XHCI_PORT_CHANGES) {
            UsbLogPort(controller, port, status & XHCI_PORT_CCS);
            UsbMmioWrite32(state->operational, offset, status | XHCI_PORT_CHANGES);
            if (status & XHCI_PORT_CCS) enumerate_port(state, port);
        }
    }
}
