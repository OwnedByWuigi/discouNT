#include <stdint.h>
#include "ahci.h"
#include "io/io.h"
#include "io/pci.h"
#include "mm/mm.h"
#include "mm/vmm.h"
#include "core/util.h"
#include "serial.h"
#include "cpu.h"

#define AHCI_MAX_DISKS 4
#define AHCI_TIMEOUT 1000000U
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35

typedef struct __attribute__((packed)) {
    uint8_t fis_type, pmport, command, featurel;
    uint8_t lba0, lba1, lba2, device;
    uint8_t lba3, lba4, lba5, featureh;
    uint8_t countl, counth, icc, control;
    uint8_t reserved[4];
} AHCI_FIS_REG_H2D;

typedef struct __attribute__((packed)) {
    uint32_t dba, dbau;
    uint32_t reserved;
    uint32_t dbc;
} AHCI_PRDT;

typedef struct __attribute__((packed)) {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    AHCI_PRDT prdt[1];
} AHCI_CMD_TABLE;

typedef struct __attribute__((packed)) {
    uint16_t flags, prdtl;
    uint32_t prdbc, ctba, ctbau;
    uint32_t reserved[4];
} AHCI_CMD_HEADER;

typedef struct {
    volatile uint8_t *port;
    uint32_t sectors;
    uint8_t *clb, *fb, *table;
} AHCI_DISK;

static IO_DRIVER_OBJECT *ahci_driver;
static AHCI_DISK disks[AHCI_MAX_DISKS];
static uint32_t disk_count;

static uint32_t reg32(volatile uint8_t *base, uint32_t offset) {
    return *(volatile uint32_t *)(base + offset);
}
static void reg32w(volatile uint8_t *base, uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(base + offset) = value;
}
static int wait_clear(volatile uint8_t *port, uint32_t mask) {
    for (uint32_t n = AHCI_TIMEOUT; n; --n) {
        if (!(reg32(port, 0x18) & mask)) return 1;
        CpuRelax();
    }
    return 0;
}

static int ahci_command(AHCI_DISK *disk, uint8_t command, uint64_t lba,
                        uint16_t count, void *buffer, int write) {
    AHCI_CMD_HEADER *header = (AHCI_CMD_HEADER *)disk->clb;
    AHCI_CMD_TABLE *table = (AHCI_CMD_TABLE *)disk->table;
    AHCI_FIS_REG_H2D *fis = (AHCI_FIS_REG_H2D *)table->cfis;
    uint64_t physical = VmmGetPhysicalAddress(buffer);
    if (!buffer || !physical || physical > 0xFFFFFFFFULL ||
        ((uint64_t)count * 512) > 0x400000U) return 0;
    if (!wait_clear(disk->port, 0x90)) return 0;
    memset(disk->clb, 0, 1024);
    memset(disk->table, 0, 256);
    header->flags = 5 | (write ? (1U << 6) : 0);
    header->prdtl = 1;
    /* The first two dwords are the command header; keep the extension layout
       local so this remains compatible with the AHCI 1.x register format. */
    header->ctba = (uint32_t)VmmGetPhysicalAddress(disk->table);
    memset(fis, 0, sizeof(*fis));
    fis->fis_type = 0x27;
    fis->command = command;
    fis->device = 1U << 6;
    fis->lba0 = (uint8_t)lba; fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16); fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32); fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = (uint8_t)count; fis->counth = (uint8_t)(count >> 8);
    table->prdt[0].dba = (uint32_t)physical;
    table->prdt[0].dbau = 0;
    table->prdt[0].dbc = (uint32_t)count * 512 - 1;
    reg32w(disk->port, 0x10, reg32(disk->port, 0x10) | 1);
    reg32w(disk->port, 0x38, 1);
    for (uint32_t n = AHCI_TIMEOUT; n; --n) {
        uint32_t is = reg32(disk->port, 0x10);
        if (!(reg32(disk->port, 0x38) & 1)) {
            reg32w(disk->port, 0x10, is);
            return !(is & 0x40000000U);
        }
        if (is & 0x40000000U) return 0;
        CpuRelax();
    }
    return 0;
}

static int ahci_port_init(volatile uint8_t *port, AHCI_DISK *disk) {
    uint16_t identify[256];
    disk->port = port;
    disk->clb = (uint8_t *)VmmAllocatePages(1);
    disk->fb = (uint8_t *)VmmAllocatePages(1);
    disk->table = (uint8_t *)VmmAllocatePages(1);
    if (!disk->clb || !disk->fb || !disk->table) return 0;
    memset(disk->clb, 0, 4096); memset(disk->fb, 0, 4096);
    memset(disk->table, 0, 4096);
    reg32w(port, 0x18, reg32(port, 0x18) & ~1U);
    if (!wait_clear(port, 0x8000)) return 0;
    reg32w(port, 0x00, (uint32_t)VmmGetPhysicalAddress(disk->clb));
    reg32w(port, 0x08, (uint32_t)VmmGetPhysicalAddress(disk->fb));
    reg32w(port, 0x10, 0xFFFFFFFFU);
    reg32w(port, 0x18, reg32(port, 0x18) | 0x10);
    if (!ahci_command(disk, ATA_CMD_IDENTIFY, 0, 1, identify, 0)) return 0;
    disk->sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
    if (identify[83] & (1U << 10)) {
        uint64_t total = (uint64_t)identify[100] | ((uint64_t)identify[101] << 16) |
            ((uint64_t)identify[102] << 32) | ((uint64_t)identify[103] << 48);
        disk->sectors = total > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (uint32_t)total;
    }
    return disk->sectors != 0;
}

static int ahci_dispatch(IO_DEVICE_OBJECT *device, IO_REQUEST *request) {
    AHCI_DISK *disk = (AHCI_DISK *)device->device_extension;
    uint64_t offset = request->parameters.read_write.offset;
    uint32_t count = request->length >> 9;
    if (!request->buffer || !request->length || (offset & 511) ||
        (request->length & 511) || (offset >> 9) > 0xFFFFFFFFULL ||
        count > 0xFFFF || (offset >> 9) + count > disk->sectors)
        return IO_STATUS_INVALID_PARAMETER;
    if (!ahci_command(disk, request->major_function == IO_MJ_WRITE ?
                      ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT,
                      offset >> 9, (uint16_t)count, request->buffer,
                      request->major_function == IO_MJ_WRITE))
        return IO_STATUS_DEVICE_ERROR;
    request->io_status.information = request->length;
    return IO_STATUS_SUCCESS;
}

static void scan_controller(uint8_t bus, uint8_t slot, uint8_t function) {
    uint32_t bar = PciConfigRead32(bus, slot, function, 0x24);
    uint64_t abar_phys = bar & ~0xFULL;
    volatile uint8_t *abar;
    uint32_t pi;
    if (!abar_phys || (bar & 1)) return;
    if ((bar & 6U) == 4U)
        abar_phys |= (uint64_t)PciConfigRead32(bus, slot, function, 0x28) << 32;
    abar = (volatile uint8_t *)VmmMapMmioRange(abar_phys, 0x1100);
    if (!abar) return;
    pi = reg32(abar, 0x0C);
    reg32w(abar, 0x04, reg32(abar, 0x04) | (1U << 31));
    for (uint8_t port = 0; port < 32 && disk_count < AHCI_MAX_DISKS; ++port) {
        volatile uint8_t *p;
        if (!(pi & (1U << port))) continue;
        p = abar + 0x100 + port * 0x80;
        if (reg32(p, 0x24) != 0x00000101U) continue;
        if (!ahci_port_init(p, &disks[disk_count])) continue;
        char name[16] = "SataDisk0";
        name[8] = (char)('0' + disk_count);
        IO_DEVICE_OBJECT *device = IoCreateDevice(ahci_driver, name, sizeof(AHCI_DISK));
        if (!device) continue;
        memcpy(device->device_extension, &disks[disk_count], sizeof(AHCI_DISK));
        SerialPutString("[AHCI] Attached "); SerialPutString(name); SerialPutString("\r\n");
        ++disk_count;
    }
}

int AhciInitialize(IO_DRIVER_OBJECT *driver) {
    if (!driver) return 0;
    ahci_driver = driver; disk_count = 0; memset(disks, 0, sizeof(disks));
    driver->major_function[IO_MJ_READ] = ahci_dispatch;
    driver->major_function[IO_MJ_WRITE] = ahci_dispatch;
#if defined(__x86_64__) || defined(__i386__)
    for (uint16_t bus = 0; bus < 256; ++bus)
        for (uint8_t slot = 0; slot < 32; ++slot)
            for (uint8_t fn = 0; fn < 8; ++fn) {
                uint32_t id = PciConfigRead32((uint8_t)bus, slot, fn, 0);
                uint32_t class = PciConfigRead32((uint8_t)bus, slot, fn, 8);
                if (id != 0xFFFFFFFFU && (class >> 8) == 0x010601U) {
                    /* Enable memory decoding and bus mastering before using
                       the controller's MMIO and DMA engines. */
                    PciConfigWrite32((uint8_t)bus, slot, fn, 4,
                                     PciConfigRead32((uint8_t)bus, slot, fn, 4) | 0x6U);
                    scan_controller((uint8_t)bus, slot, fn);
                }
            }
#endif
    return disk_count != 0;
}

int AhciBootInitialize(void) {
    IO_DRIVER_OBJECT *driver = IoCreateDriver("BootAhci", 0, 0);
    return driver && AhciInitialize(driver);
}
uint32_t AhciGetDiskCount(void) { return disk_count; }
uint32_t AhciGetDiskSectors(uint32_t index) { return index < disk_count ? disks[index].sectors : 0; }
const char *AhciGetDiskName(uint32_t index) { (void)index; return "SataDisk0"; }

__attribute__((weak)) int DriverEntry(IO_DRIVER_OBJECT *driver, void *context) {
    (void)context; return AhciInitialize(driver);
}
