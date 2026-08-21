#include <stdint.h>
#include "net.h"
#include "io/port.h"
#include "io/pci.h"
#include "mm/mm.h"
#include "serial.h"
#include "core/util.h"


#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

#define PCNET_VENDOR_ID   0x1022
#define PCNET_DEVICE_ID   0x2000

#define E1000_VENDOR_ID   0x8086
#define E1000_DEVICE_82540EM 0x100E
#define E1000_DEVICE_82545EM 0x100F
#define E1000_DEVICE_82543GC 0x1004
#define E1000_DEVICE_82541PI 0x107C

#define RX_BUF_SIZE   8192
#define TX_BUF_SIZE   2048

typedef struct __attribute__((packed)) {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} ETH_HDR;

typedef struct __attribute__((packed)) {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
} ARP_PKT;

typedef struct __attribute__((packed)) {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t src[4];
    uint8_t dst[4];
} IPV4_HDR;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
    uint8_t payload[32];
} ICMP_ECHO_PKT;

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} UDP_HDR;

typedef struct __attribute__((packed)) {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint8_t options[312];
} DHCP_PKT;

typedef struct {
    uint8_t bus, slot, func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t command;
    uint8_t irq;
} PCI_DEVICE_INFO;

typedef struct {
    int (*init)(const PCI_DEVICE_INFO *pci);
    void (*poll)(void);
    void (*send_raw)(const void *frame, int len);
    int (*is_ready)(void);
    const char *name;
} NIC_OPS;

static uint8_t nic_mac[6];
static uint8_t nic_ip[4] = {10,0,2,15};
static uint8_t nic_mask[4] = {255,255,255,0};
static uint8_t nic_gateway[4] = {10,0,2,2};
static int nic_ready = 0;
static const NIC_OPS *nic_ops = 0;

static uint8_t arp_ip[4];
static uint8_t arp_mac[6];
static int arp_valid = 0;
static uint16_t ping_sequence = 1;
static uint16_t ping_identifier = 0x4E54;
static int ping_got_reply = 0;
static uint8_t ping_reply_ip[4];
static uint32_t dhcp_xid = 0x4E544450U;
static int dhcp_offer_valid = 0;
static int dhcp_ack_valid = 0;
static uint8_t dhcp_offer_ip[4];
static uint8_t dhcp_server_ip[4];
static uint8_t dhcp_offer_mask[4];
static uint8_t dhcp_offer_gateway[4];

static uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static uint32_t bswap32(uint32_t v) {
    return ((v & 0x000000FFU) << 24) |
           ((v & 0x0000FF00U) << 8)  |
           ((v & 0x00FF0000U) >> 8)  |
           ((v & 0xFF000000U) >> 24);
}

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return PciConfigRead32(bus, slot, func, offset);
}

static uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset);
    return (uint16_t)((v >> ((offset & 2) * 8)) & 0xFFFFU);
}

static uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset);
    return (uint8_t)((v >> ((offset & 3) * 8)) & 0xFFU);
}

static void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    PciConfigWrite32(bus, slot, func, offset, value);
}

static void pci_enable_busmaster(const PCI_DEVICE_INFO *pci) {
    pci_write32(pci->bus, pci->slot, pci->func, 0x04, pci->command | 0x00000007U);
}

static int pci_find_first(PCI_DEVICE_INFO *out, uint16_t vendor, uint16_t device) {
    /* The firmware-visible NICs supported here are on the root bus in QEMU,
       VirtualBox, and VMware.  Avoid spending seconds probing every possible
       PCI bus during the single-threaded boot path. */
    for (uint16_t bus = 0; bus < 1; bus++) {
        /* QEMU's emulated NICs use slot 3; probing that slot first keeps
           startup responsive on the very slow port-I/O path. */
        for (uint8_t pass = 0; pass < 2; pass++) {
            uint8_t slot = pass ? 0 : 3;
            if (pass && slot == 3) continue;
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read32((uint8_t)bus, slot, func, 0x00);
                if (id == 0xFFFFFFFFU) {
                    if (func == 0) break;
                    continue;
                }
                if ((id & 0xFFFFU) == vendor &&
                    ((id >> 16) & 0xFFFFU) == device) {
                    out->bus = (uint8_t)bus;
                    out->slot = slot;
                    out->func = func;
                    out->vendor_id = vendor;
                    out->device_id = device;
                    out->bar0 = pci_read32(out->bus, out->slot, out->func, 0x10);
                    out->bar1 = pci_read32(out->bus, out->slot, out->func, 0x14);
                    out->command = pci_read32(out->bus, out->slot, out->func, 0x04);
                    out->irq = pci_read8(out->bus, out->slot, out->func, 0x3C);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static uint16_t net_checksum(const void *data, int len) {
    const uint8_t *p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint16_t)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len) sum += ((uint16_t)p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static void ip_copy(uint8_t dst[4], const uint8_t src[4]) {
    dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2]; dst[3]=src[3];
}

static int ip_equal(const uint8_t a[4], const uint8_t b[4]) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

static int ip_same_subnet(const uint8_t a[4], const uint8_t b[4]) {
    return ((a[0] & nic_mask[0]) == (b[0] & nic_mask[0])) &&
           ((a[1] & nic_mask[1]) == (b[1] & nic_mask[1])) &&
           ((a[2] & nic_mask[2]) == (b[2] & nic_mask[2])) &&
           ((a[3] & nic_mask[3]) == (b[3] & nic_mask[3]));
}

static void format_ip(const uint8_t ip[4], char *out) {
    char num[8];
    out[0] = 0;
    itoa(ip[0], num, 10); strcat(out, num); strcat(out, ".");
    itoa(ip[1], num, 10); strcat(out, num); strcat(out, ".");
    itoa(ip[2], num, 10); strcat(out, num); strcat(out, ".");
    itoa(ip[3], num, 10); strcat(out, num);
}

static int parse_ip(const char *text, uint8_t out[4]) {
    int idx = 0, val = 0, saw = 0;
    while (*text) {
        if (*text >= '0' && *text <= '9') {
            val = val * 10 + (*text - '0');
            if (val > 255) return 0;
            saw = 1;
        } else if (*text == '.') {
            if (!saw || idx >= 3) return 0;
            out[idx++] = (uint8_t)val;
            val = 0;
            saw = 0;
        } else return 0;
        text++;
    }
    if (!saw || idx != 3) return 0;
    out[3] = (uint8_t)val;
    return 1;
}

static void nic_send_raw(const void *frame, int len) {
    if (nic_ops && nic_ops->send_raw) nic_ops->send_raw(frame, len);
}

/* ---------- RTL8139 ---------- */
#define RTL_IDR0      0x00
#define RTL_RBSTART   0x30
#define RTL_CR        0x37
#define RTL_CAPR      0x38
#define RTL_CBR       0x3A
#define RTL_IMR       0x3C
#define RTL_ISR       0x3E
#define RTL_TCR       0x40
#define RTL_RCR       0x44
#define RTL_CONFIG1   0x52
#define RTL_MPC       0x4C
#define RTL_TSAD0     0x20
#define RTL_TSD0      0x10
#define RTL_CMD_RESET 0x10
#define RTL_CMD_RE    0x08
#define RTL_CMD_TE    0x04
#define RTL_RCR_APM   0x00000002
#define RTL_RCR_AB    0x00000008
#define RTL_RCR_WRAP  0x00000080
#define RTL_ISR_ALL   0xFFFF

static struct {
    uint16_t io;
    uint16_t rx_offset;
    int tx_slot;
    uint8_t *rx_buffer;
    uint8_t *tx_buffers[4];
    int ready;
} rtl;

static int rtl_init(const PCI_DEVICE_INFO *pci) {
    rtl.io = (uint16_t)(pci->bar0 & 0xFFFFFFFCU);
    pci_enable_busmaster(pci);
    rtl.rx_buffer = (uint8_t*)kmalloc(RX_BUF_SIZE + 16 + 1500);
    for (int i = 0; i < 4; i++) rtl.tx_buffers[i] = (uint8_t*)kmalloc(TX_BUF_SIZE);
    if (!rtl.rx_buffer || !rtl.tx_buffers[0] || !rtl.tx_buffers[1] || !rtl.tx_buffers[2] || !rtl.tx_buffers[3]) return 0;
    outb(rtl.io + RTL_CONFIG1, 0x00);
    outb(rtl.io + RTL_CR, RTL_CMD_RESET);
    for (volatile int i = 0; i < 1000000 && (inb(rtl.io + RTL_CR) & RTL_CMD_RESET); i++);
    for (int i = 0; i < 6; i++) nic_mac[i] = inb(rtl.io + RTL_IDR0 + i);
    outl(rtl.io + RTL_RBSTART, (uint32_t)(uintptr_t)rtl.rx_buffer);
    outw(rtl.io + RTL_IMR, 0);
    outl(rtl.io + RTL_RCR, RTL_RCR_APM | RTL_RCR_AB | RTL_RCR_WRAP);
    outl(rtl.io + RTL_TCR, 0x03000700);
    outl(rtl.io + RTL_MPC, 0);
    rtl.rx_offset = 0;
    outw(rtl.io + RTL_CAPR, 0);
    outw(rtl.io + RTL_ISR, RTL_ISR_ALL);
    outb(rtl.io + RTL_CR, RTL_CMD_RE | RTL_CMD_TE);
    rtl.ready = 1;
    return 1;
}

static void rtl_send(const void *frame, int len) {
    uint8_t *tx = rtl.tx_buffers[rtl.tx_slot];
    if (!rtl.ready || !tx || len <= 0 || len > TX_BUF_SIZE) return;
    memcpy(tx, frame, (uint32_t)len);
    outl(rtl.io + RTL_TSAD0 + (rtl.tx_slot * 4), (uint32_t)(uintptr_t)tx);
    outl(rtl.io + RTL_TSD0 + (rtl.tx_slot * 4), (uint32_t)len);
    rtl.tx_slot = (rtl.tx_slot + 1) & 3;
}

static void net_handle_frame(const uint8_t *packet, int packet_len);

static void rtl_poll(void) {
    if (!rtl.ready) return;
    while (!(inb(rtl.io + RTL_CR) & 0x01)) {
        uint16_t *hdr = (uint16_t*)(rtl.rx_buffer + rtl.rx_offset);
        uint16_t status = hdr[0];
        uint16_t packet_len = hdr[1];
        uint8_t *packet;
        if (!(status & 0x0001) || packet_len < 60 || packet_len > 1600) {
            outw(rtl.io + RTL_CAPR, (uint16_t)((rtl.rx_offset - 0x10) & 0xFFFF));
            break;
        }
        packet = rtl.rx_buffer + rtl.rx_offset + 4;
        net_handle_frame(packet, packet_len);
        rtl.rx_offset = (uint16_t)(((rtl.rx_offset + packet_len + 4 + 3) & ~3) % RX_BUF_SIZE);
        outw(rtl.io + RTL_CAPR, (uint16_t)((rtl.rx_offset - 0x10) & 0xFFFF));
        outw(rtl.io + RTL_ISR, RTL_ISR_ALL);
    }
}

static int rtl_is_ready(void) { return rtl.ready; }
static const NIC_OPS rtl_ops = { rtl_init, rtl_poll, rtl_send, rtl_is_ready, "RTL8139" };

/* ---------- PCnet ---------- */
#define PCNET_RDP   0x10
#define PCNET_RAP   0x12
#define PCNET_RESET 0x14
#define PCNET_BDP   0x16

#define PCNET_CSR0_INIT 0x0001
#define PCNET_CSR0_STRT 0x0002
#define PCNET_CSR0_STOP 0x0004
#define PCNET_CSR0_TDMD 0x0008
#define PCNET_CSR0_IDON 0x0100
#define PCNET_CSR0_INTR 0x8000

typedef struct __attribute__((packed)) {
    uint16_t mode;
    uint8_t rlen_tlen;
    uint8_t reserved;
    uint8_t phys_addr[6];
    uint16_t reserved2;
    uint32_t filter[2];
    uint32_t rx_ring;
    uint32_t tx_ring;
} PCNET_INIT_BLOCK;

typedef struct __attribute__((packed)) {
    uint32_t base;
    int16_t length;
    uint16_t status;
    uint32_t misc;
    uint32_t reserved;
} PCNET_DESC;

static struct {
    uint16_t io;
    PCNET_INIT_BLOCK *init_block;
    PCNET_DESC *rx_ring;
    PCNET_DESC *tx_ring;
    uint8_t *rx_buffers[8];
    uint8_t *tx_buffers[8];
    int rx_index;
    int tx_index;
    int ready;
} pcnet;

static void pcnet_rap(uint16_t reg) { outw(pcnet.io + PCNET_RAP, reg); }
static uint16_t pcnet_csr_read(uint16_t reg) { pcnet_rap(reg); return inw(pcnet.io + PCNET_RDP); }
static void pcnet_csr_write(uint16_t reg, uint16_t val) { pcnet_rap(reg); outw(pcnet.io + PCNET_RDP, val); }
static uint16_t pcnet_bcr_read(uint16_t reg) { pcnet_rap(reg); return inw(pcnet.io + PCNET_BDP); }
static void pcnet_bcr_write(uint16_t reg, uint16_t val) { pcnet_rap(reg); outw(pcnet.io + PCNET_BDP, val); }

static int pcnet_init(const PCI_DEVICE_INFO *pci) {
    pcnet.io = (uint16_t)(pci->bar0 & 0xFFFFFFFCU);
    pci_enable_busmaster(pci);
    (void)inw(pcnet.io + PCNET_RESET);
    (void)inw(pcnet.io + PCNET_RESET);
    pcnet_bcr_write(20, 2);
    pcnet_csr_write(0, PCNET_CSR0_STOP);

    pcnet.init_block = (PCNET_INIT_BLOCK*)kmalloc(sizeof(PCNET_INIT_BLOCK));
    pcnet.rx_ring = (PCNET_DESC*)kmalloc(sizeof(PCNET_DESC) * 8);
    pcnet.tx_ring = (PCNET_DESC*)kmalloc(sizeof(PCNET_DESC) * 8);
    if (!pcnet.init_block || !pcnet.rx_ring || !pcnet.tx_ring) return 0;
    memset(pcnet.init_block, 0, sizeof(PCNET_INIT_BLOCK));
    memset(pcnet.rx_ring, 0, sizeof(PCNET_DESC) * 8);
    memset(pcnet.tx_ring, 0, sizeof(PCNET_DESC) * 8);

    for (int i = 0; i < 6; i++) nic_mac[i] = inb(pcnet.io + i);
    memcpy(pcnet.init_block->phys_addr, nic_mac, 6);
    pcnet.init_block->rlen_tlen = (3 << 4) | 3;
    pcnet.init_block->rx_ring = (uint32_t)(uintptr_t)pcnet.rx_ring;
    pcnet.init_block->tx_ring = (uint32_t)(uintptr_t)pcnet.tx_ring;

    for (int i = 0; i < 8; i++) {
        pcnet.rx_buffers[i] = (uint8_t*)kmalloc(TX_BUF_SIZE);
        pcnet.tx_buffers[i] = (uint8_t*)kmalloc(TX_BUF_SIZE);
        if (!pcnet.rx_buffers[i] || !pcnet.tx_buffers[i]) return 0;
        pcnet.rx_ring[i].base = (uint32_t)(uintptr_t)pcnet.rx_buffers[i];
        pcnet.rx_ring[i].length = (int16_t)(-(int)TX_BUF_SIZE);
        pcnet.rx_ring[i].status = 0x8000;
        pcnet.tx_ring[i].base = (uint32_t)(uintptr_t)pcnet.tx_buffers[i];
        pcnet.tx_ring[i].length = (int16_t)(-(int)TX_BUF_SIZE);
        pcnet.tx_ring[i].status = 0;
    }

    pcnet_csr_write(1, (uint16_t)((uint32_t)(uintptr_t)pcnet.init_block & 0xFFFF));
    pcnet_csr_write(2, (uint16_t)(((uint32_t)(uintptr_t)pcnet.init_block >> 16) & 0xFFFF));
    pcnet_csr_write(3, 0);
    pcnet_csr_write(0, PCNET_CSR0_INIT);
    for (volatile int i = 0; i < 1000000 && !(pcnet_csr_read(0) & PCNET_CSR0_IDON); i++);
    pcnet_csr_write(0, PCNET_CSR0_IDON | PCNET_CSR0_STRT);

    pcnet.rx_index = 0;
    pcnet.tx_index = 0;
    pcnet.ready = 1;
    return 1;
}

static void pcnet_send(const void *frame, int len) {
    int idx = pcnet.tx_index & 7;
    if (!pcnet.ready || len <= 0 || len > TX_BUF_SIZE) return;
    if (pcnet.tx_ring[idx].status & 0x8000) return;
    memcpy(pcnet.tx_buffers[idx], frame, (uint32_t)len);
    pcnet.tx_ring[idx].length = (int16_t)(-len);
    pcnet.tx_ring[idx].misc = 0;
    pcnet.tx_ring[idx].status = 0x8300;
    pcnet.tx_index = (pcnet.tx_index + 1) & 7;
    pcnet_csr_write(0, PCNET_CSR0_TDMD | PCNET_CSR0_STRT);
}

static void pcnet_poll(void) {
    if (!pcnet.ready) return;
    for (int i = 0; i < 8; i++) {
        PCNET_DESC *d = &pcnet.rx_ring[pcnet.rx_index & 7];
        if (d->status & 0x8000) break;
        if ((d->status & 0x0300) == 0x0300) {
            int packet_len = (int)(d->misc & 0x0FFF);
            if (packet_len > 0 && packet_len <= TX_BUF_SIZE) {
                net_handle_frame(pcnet.rx_buffers[pcnet.rx_index & 7], packet_len);
            }
        }
        d->length = (int16_t)(-(int)TX_BUF_SIZE);
        d->status = 0x8000;
        pcnet.rx_index = (pcnet.rx_index + 1) & 7;
    }
    pcnet_csr_write(0, pcnet_csr_read(0));
}

static int pcnet_is_ready(void) { return pcnet.ready; }
static const NIC_OPS pcnet_ops = { pcnet_init, pcnet_poll, pcnet_send, pcnet_is_ready, "PCnet" };

/* ---------- E1000 ---------- */
#define E1000_CTRL   0x0000
#define E1000_STATUS 0x0008
#define E1000_EERD   0x0014
#define E1000_ICR    0x00C0
#define E1000_IMS    0x00D0
#define E1000_RCTL   0x0100
#define E1000_TCTL   0x0400
#define E1000_TIPG   0x0410
#define E1000_RDBAL  0x2800
#define E1000_RDLEN  0x2808
#define E1000_RDH    0x2810
#define E1000_RDT    0x2818
#define E1000_TDBAL  0x3800
#define E1000_TDLEN  0x3808
#define E1000_TDH    0x3810
#define E1000_TDT    0x3818
#define E1000_RAL    0x5400
#define E1000_RAH    0x5404

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} E1000_RX_DESC;

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} E1000_TX_DESC;

static struct {
    volatile uint8_t *mmio;
    E1000_RX_DESC *rx_ring;
    E1000_TX_DESC *tx_ring;
    uint8_t *rx_buffers[8];
    uint8_t *tx_buffers[8];
    uint32_t rx_tail;
    uint32_t tx_tail;
    int ready;
} e1000;

static uint32_t e1000_r32(uint32_t reg) { return *(volatile uint32_t*)(e1000.mmio + reg); }
static void e1000_w32(uint32_t reg, uint32_t v) { *(volatile uint32_t*)(e1000.mmio + reg) = v; }

static int e1000_read_eeprom_word(uint8_t index, uint16_t *data) {
    e1000_w32(E1000_EERD, 1 | ((uint32_t)index << 8));
    for (volatile int i = 0; i < 100000; i++) {
        uint32_t v = e1000_r32(E1000_EERD);
        if (v & (1 << 4)) {
            *data = (uint16_t)(v >> 16);
            return 1;
        }
    }
    return 0;
}

static int e1000_init(const PCI_DEVICE_INFO *pci) {
    e1000.mmio = (volatile uint8_t*)(uintptr_t)(pci->bar0 & ~0x0FU);
    pci_enable_busmaster(pci);
    e1000.rx_ring = (E1000_RX_DESC*)kmalloc(sizeof(E1000_RX_DESC) * 8);
    e1000.tx_ring = (E1000_TX_DESC*)kmalloc(sizeof(E1000_TX_DESC) * 8);
    if (!e1000.rx_ring || !e1000.tx_ring) return 0;
    memset((void*)e1000.rx_ring, 0, sizeof(E1000_RX_DESC) * 8);
    memset((void*)e1000.tx_ring, 0, sizeof(E1000_TX_DESC) * 8);
    for (int i = 0; i < 8; i++) {
        e1000.rx_buffers[i] = (uint8_t*)kmalloc(TX_BUF_SIZE);
        e1000.tx_buffers[i] = (uint8_t*)kmalloc(TX_BUF_SIZE);
        if (!e1000.rx_buffers[i] || !e1000.tx_buffers[i]) return 0;
        e1000.rx_ring[i].addr = (uint32_t)(uintptr_t)e1000.rx_buffers[i];
        e1000.tx_ring[i].addr = (uint32_t)(uintptr_t)e1000.tx_buffers[i];
        e1000.tx_ring[i].status = 0x1;
    }

    {
        uint16_t w0, w1, w2;
        if (e1000_read_eeprom_word(0, &w0) &&
            e1000_read_eeprom_word(1, &w1) &&
            e1000_read_eeprom_word(2, &w2)) {
            nic_mac[0] = w0 & 0xFF; nic_mac[1] = w0 >> 8;
            nic_mac[2] = w1 & 0xFF; nic_mac[3] = w1 >> 8;
            nic_mac[4] = w2 & 0xFF; nic_mac[5] = w2 >> 8;
        } else {
            uint32_t ral = e1000_r32(E1000_RAL);
            uint32_t rah = e1000_r32(E1000_RAH);
            nic_mac[0] = ral & 0xFF;
            nic_mac[1] = (ral >> 8) & 0xFF;
            nic_mac[2] = (ral >> 16) & 0xFF;
            nic_mac[3] = (ral >> 24) & 0xFF;
            nic_mac[4] = rah & 0xFF;
            nic_mac[5] = (rah >> 8) & 0xFF;
        }
    }

    e1000_w32(E1000_RDBAL, (uint32_t)(uintptr_t)e1000.rx_ring);
    e1000_w32(E1000_RDLEN, sizeof(E1000_RX_DESC) * 8);
    e1000_w32(E1000_RDH, 0);
    e1000_w32(E1000_RDT, 7);
    e1000_w32(E1000_RCTL, 0x00000002 | 0x00008000 | 0x04000000);

    e1000_w32(E1000_TDBAL, (uint32_t)(uintptr_t)e1000.tx_ring);
    e1000_w32(E1000_TDLEN, sizeof(E1000_TX_DESC) * 8);
    e1000_w32(E1000_TDH, 0);
    e1000_w32(E1000_TDT, 0);
    e1000_w32(E1000_TCTL, 0x00000002 | 0x00000008 | (0x0F << 4) | (0x40 << 12));
    e1000_w32(E1000_TIPG, 0x0060200A);
    e1000_w32(E1000_IMS, 0);
    e1000_w32(E1000_ICR, 0xFFFFFFFFU);

    e1000.rx_tail = 7;
    e1000.tx_tail = 0;
    e1000.ready = 1;
    return 1;
}

static void e1000_send(const void *frame, int len) {
    uint32_t idx = e1000.tx_tail;
    if (!e1000.ready || len <= 0 || len > TX_BUF_SIZE) return;
    if (!(e1000.tx_ring[idx].status & 0x1)) return;
    memcpy(e1000.tx_buffers[idx], frame, (uint32_t)len);
    e1000.tx_ring[idx].length = (uint16_t)len;
    e1000.tx_ring[idx].cmd = 0x0B;
    e1000.tx_ring[idx].status = 0;
    e1000.tx_tail = (e1000.tx_tail + 1) & 7;
    e1000_w32(E1000_TDT, e1000.tx_tail);
}

static void e1000_poll(void) {
    if (!e1000.ready) return;
    while (e1000.rx_ring[(e1000.rx_tail + 1) & 7].status & 0x1) {
        uint32_t idx = (e1000.rx_tail + 1) & 7;
        if (e1000.rx_ring[idx].length > 0) {
            net_handle_frame(e1000.rx_buffers[idx], e1000.rx_ring[idx].length);
        }
        e1000.rx_ring[idx].status = 0;
        e1000.rx_tail = idx;
        e1000_w32(E1000_RDT, e1000.rx_tail);
    }
}

static int e1000_is_ready(void) { return e1000.ready; }
static const NIC_OPS e1000_ops = { e1000_init, e1000_poll, e1000_send, e1000_is_ready, "E1000" };

/* ---------- Shared protocol ---------- */
static void send_udp_ipv4(const uint8_t src_ip[4], const uint8_t dst_ip[4], const uint8_t dst_mac[6],
                          uint16_t src_port, uint16_t dst_port,
                          const void *payload, int payload_len,
                          uint8_t ttl, uint16_t ip_id) {
    uint8_t frame[600];
    ETH_HDR *eth = (ETH_HDR*)frame;
    IPV4_HDR *ip = (IPV4_HDR*)(frame + sizeof(ETH_HDR));
    UDP_HDR *udp = (UDP_HDR*)(frame + sizeof(ETH_HDR) + sizeof(IPV4_HDR));
    int total_len = sizeof(ETH_HDR) + sizeof(IPV4_HDR) + sizeof(UDP_HDR) + payload_len;
    if (payload_len < 0 || total_len > (int)sizeof(frame)) return;
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, nic_mac, 6);
    eth->type = bswap16(0x0800);
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = bswap16(sizeof(IPV4_HDR) + sizeof(UDP_HDR) + payload_len);
    ip->id = bswap16(ip_id);
    ip->flags_frag = 0;
    ip->ttl = ttl;
    ip->protocol = 17;
    ip->checksum = 0;
    memcpy(ip->src, src_ip, 4);
    memcpy(ip->dst, dst_ip, 4);
    ip->checksum = net_checksum(ip, sizeof(IPV4_HDR));
    udp->src_port = bswap16(src_port);
    udp->dst_port = bswap16(dst_port);
    udp->length = bswap16(sizeof(UDP_HDR) + payload_len);
    udp->checksum = 0;
    memcpy(frame + sizeof(ETH_HDR) + sizeof(IPV4_HDR) + sizeof(UDP_HDR), payload, (uint32_t)payload_len);
    nic_send_raw(frame, total_len < 60 ? 60 : total_len);
}

static void send_arp_request(const uint8_t target_ip[4]) {
    uint8_t frame[64];
    ETH_HDR *eth = (ETH_HDR*)frame;
    ARP_PKT *arp = (ARP_PKT*)(frame + sizeof(ETH_HDR));
    for (int i = 0; i < 6; i++) eth->dst[i] = 0xFF;
    memcpy(eth->src, nic_mac, 6);
    eth->type = bswap16(0x0806);
    arp->htype = bswap16(1);
    arp->ptype = bswap16(0x0800);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = bswap16(1);
    memcpy(arp->sha, nic_mac, 6);
    memcpy(arp->spa, nic_ip, 4);
    memset(arp->tha, 0, 6);
    memcpy(arp->tpa, target_ip, 4);
    memset(frame + sizeof(ETH_HDR) + sizeof(ARP_PKT), 0, 64 - sizeof(ETH_HDR) - sizeof(ARP_PKT));
    nic_send_raw(frame, 64);
}

static void send_icmp_echo(const uint8_t target_ip[4], const uint8_t dst_mac[6], uint16_t seq) {
    uint8_t frame[sizeof(ETH_HDR) + sizeof(IPV4_HDR) + sizeof(ICMP_ECHO_PKT)];
    ETH_HDR *eth = (ETH_HDR*)frame;
    IPV4_HDR *ip = (IPV4_HDR*)(frame + sizeof(ETH_HDR));
    ICMP_ECHO_PKT *icmp = (ICMP_ECHO_PKT*)(frame + sizeof(ETH_HDR) + sizeof(IPV4_HDR));
    int frame_len = sizeof(frame);
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, nic_mac, 6);
    eth->type = bswap16(0x0800);
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = bswap16(sizeof(IPV4_HDR) + sizeof(ICMP_ECHO_PKT));
    ip->id = bswap16(seq);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = 1;
    ip->checksum = 0;
    memcpy(ip->src, nic_ip, 4);
    memcpy(ip->dst, target_ip, 4);
    ip->checksum = net_checksum(ip, sizeof(IPV4_HDR));
    icmp->type = 8;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->identifier = bswap16(ping_identifier);
    icmp->sequence = bswap16(seq);
    for (int i = 0; i < (int)sizeof(icmp->payload); i++) icmp->payload[i] = (uint8_t)('A' + (i % 26));
    icmp->checksum = net_checksum(icmp, sizeof(ICMP_ECHO_PKT));
    nic_send_raw(frame, frame_len < 60 ? 60 : frame_len);
}

static void dhcp_reset_offer_state(void) {
    dhcp_offer_valid = 0;
    dhcp_ack_valid = 0;
    memset(dhcp_offer_ip, 0, 4);
    memset(dhcp_server_ip, 0, 4);
    memset(dhcp_offer_mask, 0, 4);
    memset(dhcp_offer_gateway, 0, 4);
}

static void send_dhcp_packet(uint8_t msg_type, const uint8_t requested_ip[4], const uint8_t server_ip[4]) {
    DHCP_PKT pkt;
    uint8_t zero_ip[4] = {0,0,0,0};
    uint8_t broadcast_ip[4] = {255,255,255,255};
    uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    uint8_t *opt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.op = 1;
    pkt.htype = 1;
    pkt.hlen = 6;
    pkt.xid = bswap32(dhcp_xid);
    pkt.flags = bswap16(0x8000);
    memcpy(pkt.chaddr, nic_mac, 6);
    pkt.options[0] = 99; pkt.options[1] = 130; pkt.options[2] = 83; pkt.options[3] = 99;
    opt = pkt.options + 4;
    *opt++ = 53; *opt++ = 1; *opt++ = msg_type;
    *opt++ = 61; *opt++ = 7; *opt++ = 1; memcpy(opt, nic_mac, 6); opt += 6;
    *opt++ = 55; *opt++ = 3; *opt++ = 1; *opt++ = 3; *opt++ = 6;
    if (requested_ip) { *opt++ = 50; *opt++ = 4; memcpy(opt, requested_ip, 4); opt += 4; }
    if (server_ip)    { *opt++ = 54; *opt++ = 4; memcpy(opt, server_ip, 4); opt += 4; }
    *opt++ = 255;
    send_udp_ipv4(zero_ip, broadcast_ip, broadcast_mac, 68, 67, &pkt, sizeof(pkt), 64, (uint16_t)dhcp_xid);
}

static void handle_arp(const uint8_t *frame, int len) {
    const ARP_PKT *arp;
    if (len < (int)(sizeof(ETH_HDR) + sizeof(ARP_PKT))) return;
    arp = (const ARP_PKT*)(frame + sizeof(ETH_HDR));
    if (bswap16(arp->oper) == 2) {
        ip_copy(arp_ip, arp->spa);
        memcpy(arp_mac, arp->sha, 6);
        arp_valid = 1;
    }
}

static void handle_ipv4(const uint8_t *frame, int len) {
    const IPV4_HDR *ip;
    int ihl;
    if (len < (int)(sizeof(ETH_HDR) + sizeof(IPV4_HDR))) return;
    ip = (const IPV4_HDR*)(frame + sizeof(ETH_HDR));
    if ((ip->ver_ihl >> 4) != 4) return;
    ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ip->protocol == 1) {
        if (len < (int)(sizeof(ETH_HDR) + ihl + sizeof(ICMP_ECHO_PKT))) return;
        {
            const ICMP_ECHO_PKT *icmp = (const ICMP_ECHO_PKT*)(frame + sizeof(ETH_HDR) + ihl);
            if (icmp->type == 0 &&
                bswap16(icmp->identifier) == ping_identifier &&
                bswap16(icmp->sequence) == ping_sequence) {
                ping_got_reply = 1;
                ip_copy(ping_reply_ip, ip->src);
            }
        }
        return;
    }
    if (ip->protocol == 17) {
        const UDP_HDR *udp;
        const DHCP_PKT *dhcp;
        const uint8_t *opt;
        int udp_len;
        int msg_type = 0;
        if (len < (int)(sizeof(ETH_HDR) + ihl + sizeof(UDP_HDR) + 240)) return;
        udp = (const UDP_HDR*)(frame + sizeof(ETH_HDR) + ihl);
        if (bswap16(udp->src_port) != 67 || bswap16(udp->dst_port) != 68) return;
        udp_len = bswap16(udp->length);
        if (udp_len < (int)(sizeof(UDP_HDR) + 240)) return;
        dhcp = (const DHCP_PKT*)(frame + sizeof(ETH_HDR) + ihl + sizeof(UDP_HDR));
        if (dhcp->op != 2 || dhcp->htype != 1 || dhcp->hlen != 6) return;
        if (bswap32(dhcp->xid) != dhcp_xid) return;
        if (dhcp->options[0] != 99 || dhcp->options[1] != 130 || dhcp->options[2] != 83 || dhcp->options[3] != 99) return;
        opt = dhcp->options + 4;
        while (opt < ((const uint8_t*)dhcp) + udp_len - sizeof(UDP_HDR)) {
            uint8_t code = *opt++;
            uint8_t olen;
            if (code == 0) continue;
            if (code == 255) break;
            olen = *opt++;
            if (code == 53 && olen >= 1) msg_type = opt[0];
            else if (code == 1 && olen >= 4) memcpy(dhcp_offer_mask, opt, 4);
            else if (code == 3 && olen >= 4) memcpy(dhcp_offer_gateway, opt, 4);
            else if (code == 54 && olen >= 4) memcpy(dhcp_server_ip, opt, 4);
            opt += olen;
        }
        if (msg_type == 2) { memcpy(dhcp_offer_ip, dhcp->yiaddr, 4); dhcp_offer_valid = 1; }
        else if (msg_type == 5) { memcpy(dhcp_offer_ip, dhcp->yiaddr, 4); dhcp_ack_valid = 1; }
    }
}

static void net_handle_frame(const uint8_t *packet, int packet_len) {
    if (packet_len < (int)sizeof(ETH_HDR)) return;
    if (((const ETH_HDR*)packet)->type == bswap16(0x0806)) handle_arp(packet, packet_len);
    else if (((const ETH_HDR*)packet)->type == bswap16(0x0800)) handle_ipv4(packet, packet_len);
}

void NetPoll(void) {
    if (nic_ops && nic_ops->poll) nic_ops->poll();
}

int NetIsReady(void) {
    return nic_ready && nic_ops && nic_ops->is_ready && nic_ops->is_ready();
}

void NetInit(void) {
    PCI_DEVICE_INFO pci;
    const NIC_OPS *candidates[] = {
        &rtl_ops,
        &pcnet_ops,
        &e1000_ops
    };
    struct { uint16_t vendor, device; const NIC_OPS *ops; } probe[] = {
        { RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, &rtl_ops },
        { PCNET_VENDOR_ID, PCNET_DEVICE_ID, &pcnet_ops },
        { E1000_VENDOR_ID, E1000_DEVICE_82540EM, &e1000_ops },
        { E1000_VENDOR_ID, E1000_DEVICE_82545EM, &e1000_ops },
        { E1000_VENDOR_ID, E1000_DEVICE_82543GC, &e1000_ops },
        { E1000_VENDOR_ID, E1000_DEVICE_82541PI, &e1000_ops },
    };
    (void)candidates;

    nic_ready = 0;
    nic_ops = 0;

    SerialPutString("[NET] Probing PCI NICs\r\n");

    for (unsigned i = 0; i < sizeof(probe)/sizeof(probe[0]); i++) {
        if (pci_find_first(&pci, probe[i].vendor, probe[i].device) && probe[i].ops->init(&pci)) {
            nic_ops = probe[i].ops;
            nic_ready = 1;
            SerialPutString("[NET] NIC ready: ");
            SerialPutString(nic_ops->name);
            SerialPutString(" MAC=");
            for (int m = 0; m < 6; m++) {
                SerialPrintHex(nic_mac[m]);
                if (m != 5) SerialPutString(":");
            }
            SerialPutString("\r\n");
            break;
        }
    }

    if (!nic_ready) {
        SerialPutString("[NET] No supported NIC found\r\n");
        return;
    }

    dhcp_reset_offer_state();
    send_dhcp_packet(1, 0, 0);
    /* DHCP must not hold the boot/session-manager path hostage.  A failed
       lease is already handled by the static fallback below. */
    for (volatile int i = 0; i < 2000000 && !dhcp_offer_valid; i++) {
        if ((i & 0x7FF) == 0) NetPoll();
    }
    if (dhcp_offer_valid) {
        send_dhcp_packet(3, dhcp_offer_ip, dhcp_server_ip);
        for (volatile int i = 0; i < 2000000 && !dhcp_ack_valid; i++) {
            if ((i & 0x7FF) == 0) NetPoll();
        }
    }
    if (dhcp_ack_valid) {
        ip_copy(nic_ip, dhcp_offer_ip);
        if (dhcp_offer_mask[0] || dhcp_offer_mask[1] || dhcp_offer_mask[2] || dhcp_offer_mask[3]) ip_copy(nic_mask, dhcp_offer_mask);
        if (dhcp_offer_gateway[0] || dhcp_offer_gateway[1] || dhcp_offer_gateway[2] || dhcp_offer_gateway[3]) ip_copy(nic_gateway, dhcp_offer_gateway);
        SerialPutString("[NET] DHCP lease acquired IP=");
        {
            char ipbuf[32];
            format_ip(nic_ip, ipbuf);
            SerialPutString(ipbuf);
            SerialPutString(" GW=");
            format_ip(nic_gateway, ipbuf);
            SerialPutString(ipbuf);
            SerialPutString("\r\n");
        }
    } else {
        SerialPutString("[NET] DHCP failed, using static 10.0.2.15\r\n");
    }
}

int NetPing(const char *ip_text, char *out_text, int out_text_len) {
    uint8_t target_ip[4];
    uint8_t next_hop[4];
    uint8_t target_mac[6];
    char ipbuf[32];
    (void)out_text_len;
    if (!NetIsReady()) { strcpy(out_text, "Network unavailable"); return -1; }
    if (!parse_ip(ip_text, target_ip)) { strcpy(out_text, "Usage: PING a.b.c.d"); return -2; }
    if (ip_same_subnet(nic_ip, target_ip)) ip_copy(next_hop, target_ip);
    else ip_copy(next_hop, nic_gateway);
    arp_valid = 0;
    send_arp_request(next_hop);
    for (volatile int i = 0; i < 4000000 && !arp_valid; i++) {
        if ((i & 0x7FF) == 0) NetPoll();
    }
    if (!arp_valid || !ip_equal(arp_ip, next_hop)) { strcpy(out_text, "ARP timeout"); return -3; }
    memcpy(target_mac, arp_mac, 6);
    ping_got_reply = 0;
    send_icmp_echo(target_ip, target_mac, ping_sequence);
    for (volatile int i = 0; i < 12000000 && !ping_got_reply; i++) {
        if ((i & 0x7FF) == 0) NetPoll();
    }
    if (!ping_got_reply) { strcpy(out_text, "Request timed out"); ping_sequence++; return -4; }
    strcpy(out_text, "Reply from ");
    format_ip(ping_reply_ip, ipbuf);
    strcat(out_text, ipbuf);
    strcat(out_text, ": bytes=32");
    ping_sequence++;
    return 0;
}
