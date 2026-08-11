#include <stdint.h>
#include "usb.h"
#include "portio.h"
#include "serial.h"

#define PCI_ADDR 0x0CF8
#define PCI_DATA 0x0CFC
#define USB_CLASS 0x0C
#define USB_SUBCLASS 0x03

#define USB_UHCI 0x00
#define USB_OHCI 0x10
#define USB_EHCI 0x20

typedef enum {
    USB_HC_UHCI,
    USB_HC_OHCI,
    USB_HC_EHCI
} USB_HC_TYPE;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    USB_HC_TYPE type;
    uint32_t base;
    uint16_t io;
    uint8_t ports;
    uint32_t *ohci_hcca;
} USB_CONTROLLER;

#define USB_MAX_CONTROLLERS 8
static USB_CONTROLLER controllers[USB_MAX_CONTROLLERS];
static int controller_count;

static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg) {
    uint32_t address = 0x80000000U |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (reg & 0xFC);
    outl(PCI_ADDR, address);
    return inl(PCI_DATA);
}

static void pci_write(uint8_t bus, uint8_t slot, uint8_t func,
                      uint8_t reg, uint32_t value) {
    uint32_t address = 0x80000000U |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (reg & 0xFC);
    outl(PCI_ADDR, address);
    outl(PCI_DATA, value);
}

static void usb_wait(volatile uint32_t loops) {
    while (loops--) __asm__ volatile("pause");
}

static uint32_t mmio_read(uint32_t base, uint32_t offset) {
    return *(volatile uint32_t*)(uintptr_t)(base + offset);
}

static void mmio_write(uint32_t base, uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(uintptr_t)(base + offset) = value;
}

static const char *usb_type_name(USB_HC_TYPE type) {
    if (type == USB_HC_UHCI) return "UHCI";
    if (type == USB_HC_OHCI) return "OHCI";
    return "EHCI";
}

static void usb_log_port(USB_CONTROLLER *hc, int port, int connected) {
    SerialPutString("[USB] ");
    SerialPutString(usb_type_name(hc->type));
    SerialPutString(" port ");
    SerialPrintDec((uint32_t)(port + 1));
    SerialPutString(connected ? " connected\r\n" : " disconnected\r\n");
}

static int usb_init_uhci(USB_CONTROLLER *hc) {
    uint16_t status;
    uint16_t command;

    if (!(hc->io & 1)) return 0;
    hc->io &= 0xFFFCU;
    outw((uint16_t)(hc->io + 0x00), 0x0004); /* Global reset */
    usb_wait(100000);
    outw((uint16_t)(hc->io + 0x00), 0x0000);
    outw((uint16_t)(hc->io + 0x02), 0xFFFF);
    status = inw((uint16_t)(hc->io + 0x02));
    command = inw((uint16_t)(hc->io + 0x00));
    (void)status;
    (void)command;

    /* Read the two root ports and clear change bits.  Transfer queues will
     * be added once the USB bus/device layer is connected. */
    hc->ports = 2;
    for (int i = 0; i < 2; i++) {
        uint16_t port = inw((uint16_t)(hc->io + 0x10 + i * 2));
        if (port & 1) usb_log_port(hc, i, 1);
        outw((uint16_t)(hc->io + 0x10 + i * 2), port | 0x000A);
    }
    return 1;
}

static int usb_init_ohci(USB_CONTROLLER *hc) {
    uint32_t revision;
    uint32_t root_desc;
    uint32_t status;

    if (!hc->base) return 0;
    revision = mmio_read(hc->base, 0x00);
    if ((revision & 0xFF) == 0 || revision == 0xFFFFFFFFU) return 0;

    /* HcControl reset sequence. */
    mmio_write(hc->base, 0x04, 0);
    mmio_write(hc->base, 0x08, 1); /* HCR */
    usb_wait(100000);
    mmio_write(hc->base, 0x0C, 0xFFFFFFFFU);

    root_desc = mmio_read(hc->base, 0x48);
    hc->ports = (uint8_t)(root_desc & 0xFF);
    if (hc->ports > 15) hc->ports = 15;
    for (int i = 0; i < hc->ports; i++) {
        status = mmio_read(hc->base, 0x54 + (uint32_t)i * 4);
        if (status & 1) usb_log_port(hc, i, 1);
        mmio_write(hc->base, 0x54 + (uint32_t)i * 4, status | 0x00030000U);
    }
    /* Put the controller in the operational state with interrupts masked. */
    mmio_write(hc->base, 0x04, 0x80);
    return 1;
}

static int usb_init_ehci(USB_CONTROLLER *hc) {
    uint8_t cap_length;
    uint32_t op;
    uint32_t hcs;
    uint32_t command;

    if (!hc->base) return 0;
    cap_length = *(volatile uint8_t*)(uintptr_t)hc->base;
    op = hc->base + cap_length;
    hcs = mmio_read(hc->base, 4);
    hc->ports = (uint8_t)(hcs & 0x0F);
    if (hc->ports > 15) hc->ports = 15;

    command = mmio_read(op, 0x00);
    mmio_write(op, 0x00, command | 0x00000002U); /* HCRESET */
    usb_wait(200000);
    mmio_write(op, 0x04, 0xFFFFFFFFU);

    for (int i = 0; i < hc->ports; i++) {
        uint32_t port = mmio_read(op, 0x44 + (uint32_t)i * 4);
        if (port & 1) usb_log_port(hc, i, 1);
        /* Clear connect/status-change bits while preserving enable/power. */
        mmio_write(op, 0x44 + (uint32_t)i * 4, port | 0x00000002U | 0x00000004U);
    }

    /* Route ports to EHCI.  Do not start schedules until transfer rings are
     * installed; starting an empty controller is unsafe on real hardware. */
    mmio_write(op, 0x40, 1); /* CONFIGFLAG */
    return 1;
}

static int usb_init_controller(USB_CONTROLLER *hc) {
    if (hc->type == USB_HC_UHCI) return usb_init_uhci(hc);
    if (hc->type == USB_HC_OHCI) return usb_init_ohci(hc);
    return usb_init_ehci(hc);
}

void UsbInit(void) {
    controller_count = 0;
    for (uint16_t bus = 0; bus < 256 && controller_count < USB_MAX_CONTROLLERS; bus++) {
        for (uint8_t slot = 0; slot < 32 && controller_count < USB_MAX_CONTROLLERS; slot++) {
            for (uint8_t func = 0; func < 8 && controller_count < USB_MAX_CONTROLLERS; func++) {
                uint32_t id = pci_read((uint8_t)bus, slot, func, 0x00);
                uint32_t class_code;
                uint32_t bars;
                USB_CONTROLLER *hc;

                if (id == 0xFFFFFFFFU) {
                    if (func == 0) break;
                    continue;
                }
                class_code = pci_read((uint8_t)bus, slot, func, 0x08);
                if (((class_code >> 24) & 0xFF) != USB_CLASS ||
                    ((class_code >> 16) & 0xFF) != USB_SUBCLASS) continue;

                hc = &controllers[controller_count];
                bars = pci_read((uint8_t)bus, slot, func, 0x10);
                hc->bus = (uint8_t)bus;
                hc->slot = slot;
                hc->func = func;
                hc->base = 0;
                hc->io = 0;
                hc->ports = 0;
                hc->ohci_hcca = 0;
                pci_write((uint8_t)bus, slot, func, 0x04,
                          pci_read((uint8_t)bus, slot, func, 0x04) | 0x7U);

                switch ((class_code >> 8) & 0xFF) {
                    case USB_UHCI:
                        hc->type = USB_HC_UHCI;
                        hc->io = (uint16_t)(pci_read((uint8_t)bus, slot, func, 0x20) & 0xFFFFU);
                        break;
                    case USB_OHCI:
                        hc->type = USB_HC_OHCI;
                        hc->base = bars & 0xFFFFFFF0U;
                        break;
                    case USB_EHCI:
                        hc->type = USB_HC_EHCI;
                        hc->base = bars & 0xFFFFFFF0U;
                        break;
                    default:
                        continue;
                }

                SerialPutString("[USB] Found ");
                SerialPutString(usb_type_name(hc->type));
                SerialPutString(" controller at ");
                SerialPrintHex(hc->base ? hc->base : hc->io);
                SerialPutString("\r\n");
                if (usb_init_controller(hc)) controller_count++;
            }
        }
    }

    SerialPutString("[USB] ");
    SerialPrintDec((uint32_t)controller_count);
    SerialPutString(" USB 1/2 host controller(s) ready\r\n");
}

void UsbPoll(void) {
    /* Root-port change polling will be connected to hub enumeration here. */
}

int UsbIsReady(void) { return controller_count > 0; }
int UsbGetControllerCount(void) { return controller_count; }

int DriverEntry(void *context) {
    (void)context;
    UsbInit();
    return 1;
}
