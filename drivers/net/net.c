#include <stdint.h>
#include "net.h"
#include "portio.h"
#include "mm.h"
#include "serial.h"
#include "util.h"

#define PCI_CONFIG_ADDR 0x0CF8
#define PCI_CONFIG_DATA 0x0CFC

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

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

#define RTL_RCR_AAP   0x00000001
#define RTL_RCR_APM   0x00000002
#define RTL_RCR_AM    0x00000004
#define RTL_RCR_AB    0x00000008
#define RTL_RCR_WRAP  0x00000080

#define RTL_ISR_ROK   0x0001
#define RTL_ISR_RER   0x0002
#define RTL_ISR_TOK   0x0004
#define RTL_ISR_TER   0x0008

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

static uint16_t rtl_io = 0;
static uint8_t rtl_mac[6];
static uint8_t rtl_ip[4] = {10,0,2,15};
static uint8_t rtl_mask[4] = {255,255,255,0};
static uint8_t rtl_gateway[4] = {10,0,2,2};
static uint16_t rtl_rx_offset = 0;
static uint8_t rtl_ready = 0;
static int rtl_tx_slot = 0;
static uint8_t *rtl_rx_buffer = 0;
static uint8_t *rtl_tx_buffers[4];
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
static uint8_t dhcp_offer_dns[4];

static uint16_t bswap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static uint32_t bswap32(uint32_t v) {
    return ((v & 0x000000FFU) << 24) |
           ((v & 0x0000FF00U) << 8)  |
           ((v & 0x00FF0000U) >> 8)  |
           ((v & 0xFF000000U) >> 24);
}

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000U |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000U |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
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

static int ip_equal(const uint8_t a[4], const uint8_t b[4]) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

static void ip_copy(uint8_t dst[4], const uint8_t src[4]) {
    dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2]; dst[3]=src[3];
}

static int ip_same_subnet(const uint8_t a[4], const uint8_t b[4]) {
    return ((a[0] & rtl_mask[0]) == (b[0] & rtl_mask[0])) &&
           ((a[1] & rtl_mask[1]) == (b[1] & rtl_mask[1])) &&
           ((a[2] & rtl_mask[2]) == (b[2] & rtl_mask[2])) &&
           ((a[3] & rtl_mask[3]) == (b[3] & rtl_mask[3]));
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
    int idx = 0;
    int val = 0;
    int saw = 0;
    while (*text) {
        if (*text >= '0' && *text <= '9') {
            val = (val * 10) + (*text - '0');
            if (val > 255) return 0;
            saw = 1;
        } else if (*text == '.') {
            if (!saw || idx >= 3) return 0;
            out[idx++] = (uint8_t)val;
            val = 0;
            saw = 0;
        } else {
            return 0;
        }
        text++;
    }
    if (!saw || idx != 3) return 0;
    out[3] = (uint8_t)val;
    return 1;
}

static void rtl_send_raw(const void *frame, int len) {
    uint8_t *tx = rtl_tx_buffers[rtl_tx_slot];
    if (!rtl_ready || !tx || len <= 0 || len > TX_BUF_SIZE) return;
    memcpy(tx, frame, (uint32_t)len);
    outl(rtl_io + RTL_TSAD0 + (rtl_tx_slot * 4), (uint32_t)(uintptr_t)tx);
    outl(rtl_io + RTL_TSD0 + (rtl_tx_slot * 4), (uint32_t)len);
    rtl_tx_slot = (rtl_tx_slot + 1) & 3;
}

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
    memcpy(eth->src, rtl_mac, 6);
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
    rtl_send_raw(frame, total_len < 60 ? 60 : total_len);
}

static void send_arp_request(const uint8_t target_ip[4]) {
    uint8_t frame[64];
    ETH_HDR *eth = (ETH_HDR*)frame;
    ARP_PKT *arp = (ARP_PKT*)(frame + sizeof(ETH_HDR));
    for (int i = 0; i < 6; i++) eth->dst[i] = 0xFF;
    memcpy(eth->src, rtl_mac, 6);
    eth->type = bswap16(0x0806);
    arp->htype = bswap16(1);
    arp->ptype = bswap16(0x0800);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = bswap16(1);
    memcpy(arp->sha, rtl_mac, 6);
    memcpy(arp->spa, rtl_ip, 4);
    memset(arp->tha, 0, 6);
    memcpy(arp->tpa, target_ip, 4);
    memset(frame + sizeof(ETH_HDR) + sizeof(ARP_PKT), 0, 64 - sizeof(ETH_HDR) - sizeof(ARP_PKT));
    rtl_send_raw(frame, 64);
}

static void send_icmp_echo(const uint8_t target_ip[4], const uint8_t dst_mac[6], uint16_t seq) {
    uint8_t frame[sizeof(ETH_HDR) + sizeof(IPV4_HDR) + sizeof(ICMP_ECHO_PKT)];
    ETH_HDR *eth = (ETH_HDR*)frame;
    IPV4_HDR *ip = (IPV4_HDR*)(frame + sizeof(ETH_HDR));
    ICMP_ECHO_PKT *icmp = (ICMP_ECHO_PKT*)(frame + sizeof(ETH_HDR) + sizeof(IPV4_HDR));
    int frame_len = sizeof(frame);
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, rtl_mac, 6);
    eth->type = bswap16(0x0800);

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = bswap16(sizeof(IPV4_HDR) + sizeof(ICMP_ECHO_PKT));
    ip->id = bswap16(seq);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = 1;
    ip->checksum = 0;
    memcpy(ip->src, rtl_ip, 4);
    memcpy(ip->dst, target_ip, 4);
    ip->checksum = net_checksum(ip, sizeof(IPV4_HDR));

    icmp->type = 8;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->identifier = bswap16(ping_identifier);
    icmp->sequence = bswap16(seq);
    for (int i = 0; i < (int)sizeof(icmp->payload); i++) icmp->payload[i] = (uint8_t)('A' + (i % 26));
    icmp->checksum = net_checksum(icmp, sizeof(ICMP_ECHO_PKT));

    rtl_send_raw(frame, frame_len < 60 ? 60 : frame_len);
}

static void dhcp_reset_offer_state(void) {
    dhcp_offer_valid = 0;
    dhcp_ack_valid = 0;
    memset(dhcp_offer_ip, 0, 4);
    memset(dhcp_server_ip, 0, 4);
    memset(dhcp_offer_mask, 0, 4);
    memset(dhcp_offer_gateway, 0, 4);
    memset(dhcp_offer_dns, 0, 4);
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
    memcpy(pkt.chaddr, rtl_mac, 6);
    pkt.options[0] = 99;
    pkt.options[1] = 130;
    pkt.options[2] = 83;
    pkt.options[3] = 99;
    opt = pkt.options + 4;

    *opt++ = 53; *opt++ = 1; *opt++ = msg_type;
    *opt++ = 61; *opt++ = 7; *opt++ = 1; memcpy(opt, rtl_mac, 6); opt += 6;
    *opt++ = 55; *opt++ = 3; *opt++ = 1; *opt++ = 3; *opt++ = 6;
    if (requested_ip) {
        *opt++ = 50; *opt++ = 4; memcpy(opt, requested_ip, 4); opt += 4;
    }
    if (server_ip) {
        *opt++ = 54; *opt++ = 4; memcpy(opt, server_ip, 4); opt += 4;
    }
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
        if (len < (int)(sizeof(ETH_HDR) + ihl + sizeof(UDP_HDR) + sizeof(DHCP_PKT) - 312)) return;
        udp = (const UDP_HDR*)(frame + sizeof(ETH_HDR) + ihl);
        if (bswap16(udp->src_port) != 67 || bswap16(udp->dst_port) != 68) return;
        udp_len = bswap16(udp->length);
        if (udp_len < (int)(sizeof(UDP_HDR) + sizeof(DHCP_PKT) - 312)) return;
        dhcp = (const DHCP_PKT*)(frame + sizeof(ETH_HDR) + ihl + sizeof(UDP_HDR));
        if (dhcp->op != 2 || dhcp->htype != 1 || dhcp->hlen != 6) return;
        if (bswap32(dhcp->xid) != dhcp_xid) return;
        if (dhcp->options[0] != 99 || dhcp->options[1] != 130 ||
            dhcp->options[2] != 83 || dhcp->options[3] != 99) return;

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
            else if (code == 6 && olen >= 4) memcpy(dhcp_offer_dns, opt, 4);
            else if (code == 54 && olen >= 4) memcpy(dhcp_server_ip, opt, 4);
            opt += olen;
        }

        if (msg_type == 2) {
            memcpy(dhcp_offer_ip, dhcp->yiaddr, 4);
            dhcp_offer_valid = 1;
        } else if (msg_type == 5) {
            memcpy(dhcp_offer_ip, dhcp->yiaddr, 4);
            dhcp_ack_valid = 1;
        }
    }
}

void NetPoll(void) {
    if (!rtl_ready) return;
    while (!(inb(rtl_io + RTL_CR) & 0x01)) {
        uint16_t *hdr = (uint16_t*)(rtl_rx_buffer + rtl_rx_offset);
        uint16_t status = hdr[0];
        uint16_t packet_len = hdr[1];
        uint8_t *packet;
        if (!(status & 0x0001) || packet_len < 60 || packet_len > 1600) {
            outw(rtl_io + RTL_CAPR, (uint16_t)((rtl_rx_offset - 0x10) & 0xFFFF));
            break;
        }
        packet = rtl_rx_buffer + rtl_rx_offset + 4;
        if (((ETH_HDR*)packet)->type == bswap16(0x0806)) handle_arp(packet, packet_len);
        else if (((ETH_HDR*)packet)->type == bswap16(0x0800)) handle_ipv4(packet, packet_len);

        rtl_rx_offset = (uint16_t)(((rtl_rx_offset + packet_len + 4 + 3) & ~3) % RX_BUF_SIZE);
        outw(rtl_io + RTL_CAPR, (uint16_t)((rtl_rx_offset - 0x10) & 0xFFFF));
        outw(rtl_io + RTL_ISR, RTL_ISR_ROK | RTL_ISR_RER | RTL_ISR_TOK | RTL_ISR_TER);
    }
}

int NetIsReady(void) {
    return rtl_ready;
}

void NetInit(void) {
    uint8_t found = 0;
    uint8_t bus_found = 0, slot_found = 0, func_found = 0;
    if (rtl_ready) return;

    for (uint16_t bus = 0; bus < 256 && !found; bus++) {
        for (uint8_t slot = 0; slot < 32 && !found; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read32((uint8_t)bus, slot, func, 0x00);
                if (id == 0xFFFFFFFFU) {
                    if (func == 0) break;
                    continue;
                }
                if ((id & 0xFFFFU) == RTL8139_VENDOR_ID &&
                    ((id >> 16) & 0xFFFFU) == RTL8139_DEVICE_ID) {
                    found = 1;
                    bus_found = (uint8_t)bus;
                    slot_found = slot;
                    func_found = func;
                    break;
                }
            }
        }
    }

    if (!found) {
        SerialPutString("[NET] RTL8139 not found\r\n");
        return;
    }

    {
        uint32_t bar0 = pci_read32(bus_found, slot_found, func_found, 0x10);
        uint32_t cmd = pci_read32(bus_found, slot_found, func_found, 0x04);
        rtl_io = (uint16_t)(bar0 & 0xFFFFFFFCU);
        pci_write32(bus_found, slot_found, func_found, 0x04, cmd | 0x00000005U);
    }

    rtl_rx_buffer = (uint8_t*)kmalloc(RX_BUF_SIZE + 16 + 1500);
    for (int i = 0; i < 4; i++) rtl_tx_buffers[i] = (uint8_t*)kmalloc(TX_BUF_SIZE);
    if (!rtl_rx_buffer || !rtl_tx_buffers[0] || !rtl_tx_buffers[1] || !rtl_tx_buffers[2] || !rtl_tx_buffers[3]) {
        SerialPutString("[NET] Buffer allocation failed\r\n");
        return;
    }

    outb(rtl_io + RTL_CONFIG1, 0x00);
    outb(rtl_io + RTL_CR, RTL_CMD_RESET);
    for (volatile int i = 0; i < 1000000 && (inb(rtl_io + RTL_CR) & RTL_CMD_RESET); i++);

    for (int i = 0; i < 6; i++) rtl_mac[i] = inb(rtl_io + RTL_IDR0 + i);

    outl(rtl_io + RTL_RBSTART, (uint32_t)(uintptr_t)rtl_rx_buffer);
    outw(rtl_io + RTL_IMR, 0x0000);
    outl(rtl_io + RTL_RCR, RTL_RCR_APM | RTL_RCR_AB | RTL_RCR_WRAP);
    outl(rtl_io + RTL_TCR, 0x03000700);
    outl(rtl_io + RTL_MPC, 0);
    rtl_rx_offset = 0;
    outw(rtl_io + RTL_CAPR, 0);
    outw(rtl_io + RTL_ISR, 0xFFFF);
    outb(rtl_io + RTL_CR, RTL_CMD_RE | RTL_CMD_TE);

    rtl_ready = 1;
    SerialPutString("[NET] RTL8139 ready MAC=");
    for (int i = 0; i < 6; i++) {
        SerialPrintHex(rtl_mac[i]);
        if (i != 5) SerialPutString(":");
    }
    SerialPutString("\r\n");

    dhcp_reset_offer_state();
    send_dhcp_packet(1, 0, 0);
    for (volatile int i = 0; i < 30000000 && !dhcp_offer_valid; i++) {
        if ((i & 0x7FF) == 0) NetPoll();
    }

    if (dhcp_offer_valid) {
        send_dhcp_packet(3, dhcp_offer_ip, dhcp_server_ip);
        for (volatile int i = 0; i < 30000000 && !dhcp_ack_valid; i++) {
            if ((i & 0x7FF) == 0) NetPoll();
        }
    }

    if (dhcp_ack_valid) {
        ip_copy(rtl_ip, dhcp_offer_ip);
        if (dhcp_offer_mask[0] || dhcp_offer_mask[1] || dhcp_offer_mask[2] || dhcp_offer_mask[3]) {
            ip_copy(rtl_mask, dhcp_offer_mask);
        }
        if (dhcp_offer_gateway[0] || dhcp_offer_gateway[1] || dhcp_offer_gateway[2] || dhcp_offer_gateway[3]) {
            ip_copy(rtl_gateway, dhcp_offer_gateway);
        }
        SerialPutString("[NET] DHCP lease acquired IP=");
        {
            char ipbuf[32];
            format_ip(rtl_ip, ipbuf);
            SerialPutString(ipbuf);
            SerialPutString(" GW=");
            format_ip(rtl_gateway, ipbuf);
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

    if (!rtl_ready) {
        strcpy(out_text, "Network unavailable");
        return -1;
    }
    if (!parse_ip(ip_text, target_ip)) {
        strcpy(out_text, "Usage: PING a.b.c.d");
        return -2;
    }

    if (ip_same_subnet(rtl_ip, target_ip)) ip_copy(next_hop, target_ip);
    else ip_copy(next_hop, rtl_gateway);

    arp_valid = 0;
    send_arp_request(next_hop);
    for (volatile int i = 0; i < 4000000 && !arp_valid; i++) {
        if ((i & 0x7FF) == 0) NetPoll();
    }
    if (!arp_valid || !ip_equal(arp_ip, next_hop)) {
        strcpy(out_text, "ARP timeout");
        return -3;
    }

    memcpy(target_mac, arp_mac, 6);
    ping_got_reply = 0;
    send_icmp_echo(target_ip, target_mac, ping_sequence);
    for (volatile int i = 0; i < 12000000 && !ping_got_reply; i++) {
        if ((i & 0x7FF) == 0) NetPoll();
    }

    if (!ping_got_reply) {
        strcpy(out_text, "Request timed out");
        ping_sequence++;
        return -4;
    }

    strcpy(out_text, "Reply from ");
    format_ip(ping_reply_ip, ipbuf);
    strcat(out_text, ipbuf);
    strcat(out_text, ": bytes=32");
    ping_sequence++;
    return 0;
}
