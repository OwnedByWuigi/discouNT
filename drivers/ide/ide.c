#include <stdint.h>
#include "ide.h"
#include "arch/x86/portio.h"
#include "serial.h"
#include "core/util.h"

#define ATA_DATA       0
#define ATA_ERROR      1
#define ATA_SECCOUNT   2
#define ATA_LBA_LOW    3
#define ATA_LBA_MID    4
#define ATA_LBA_HIGH   5
#define ATA_DRIVE      6
#define ATA_STATUS     7
#define ATA_COMMAND    7

#define ATA_SR_ERR     0x01
#define ATA_SR_DRQ     0x08
#define ATA_SR_DF      0x20
#define ATA_SR_BSY     0x80
#define ATA_CMD_READ   0x20
#define ATA_CMD_WRITE  0x30
#define ATA_CMD_READ_EXT  0x24
#define ATA_CMD_WRITE_EXT 0x34
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH  0xE7
#define ATA_CMD_FLUSH_EXT 0xEA

#define ATA_TIMEOUT 1000000U

typedef struct {
    uint16_t io_base;
    uint16_t control_base;
    uint8_t slave;
    uint8_t lba48;
    uint32_t sectors;
} IDE_DISK;

static IO_DRIVER_OBJECT *ide_driver;
static uint32_t disk_count;
static uint32_t disk_sectors[4];

static void ide_delay(const IDE_DISK *disk) {
    (void)inb(disk->control_base);
    (void)inb(disk->control_base);
    (void)inb(disk->control_base);
    (void)inb(disk->control_base);
}

static int ide_wait(const IDE_DISK *disk, int need_data) {
    for (uint32_t timeout = ATA_TIMEOUT; timeout; --timeout) {
        uint8_t status = inb(disk->io_base + ATA_STATUS);
        if (status == 0xFF || status == 0) continue;
        if (!(status & ATA_SR_BSY)) {
            if (status & (ATA_SR_ERR | ATA_SR_DF)) return 0;
            if (!need_data || (status & ATA_SR_DRQ)) return 1;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

static void ide_select(const IDE_DISK *disk, uint32_t lba) {
    outb(disk->io_base + ATA_DRIVE,
         (uint8_t)(0xE0 | (disk->slave << 4) |
                   (disk->lba48 ? 0 : ((lba >> 24) & 0x0F))));
    ide_delay(disk);
}

static int ide_transfer(IDE_DISK *disk, uint32_t lba, uint8_t *buffer,
                        uint32_t sectors, int write) {
    while (sectors) {
        uint32_t batch = sectors > 256 ? 256 : sectors;
        if (lba >= disk->sectors || batch > disk->sectors - lba) return 0;
        ide_select(disk, lba);
        if (disk->lba48) {
            /* ATA-6 requires the high-order register bank first. */
            outb(disk->io_base + ATA_ERROR, 0);
            outb(disk->io_base + ATA_SECCOUNT, (uint8_t)(batch >> 8));
            outb(disk->io_base + ATA_LBA_LOW, (uint8_t)(lba >> 24));
            outb(disk->io_base + ATA_LBA_MID, 0);
            outb(disk->io_base + ATA_LBA_HIGH, 0);
        }
        outb(disk->io_base + ATA_ERROR, 0);
        outb(disk->io_base + ATA_SECCOUNT, (uint8_t)batch);
        outb(disk->io_base + ATA_LBA_LOW, (uint8_t)lba);
        outb(disk->io_base + ATA_LBA_MID, (uint8_t)(lba >> 8));
        outb(disk->io_base + ATA_LBA_HIGH, (uint8_t)(lba >> 16));
        outb(disk->io_base + ATA_COMMAND,
             disk->lba48 ? (write ? ATA_CMD_WRITE_EXT : ATA_CMD_READ_EXT)
                         : (write ? ATA_CMD_WRITE : ATA_CMD_READ));

        for (uint32_t sector = 0; sector < batch; ++sector) {
            if (!ide_wait(disk, 1)) return 0;
            if (write) {
                for (uint32_t word = 0; word < 256; ++word) {
                    uint16_t value = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
                    outw(disk->io_base + ATA_DATA, value);
                    buffer += 2;
                }
            } else {
                for (uint32_t word = 0; word < 256; ++word) {
                    uint16_t value = inw(disk->io_base + ATA_DATA);
                    *buffer++ = (uint8_t)value;
                    *buffer++ = (uint8_t)(value >> 8);
                }
            }
        }
        lba += batch;
        sectors -= batch;
    }
    if (write) {
        outb(disk->io_base + ATA_COMMAND,
             disk->lba48 ? ATA_CMD_FLUSH_EXT : ATA_CMD_FLUSH);
        if (!ide_wait(disk, 0)) return 0;
    }
    return 1;
}

static int ide_dispatch(IO_DEVICE_OBJECT *device, IO_REQUEST *request) {
    IDE_DISK *disk = (IDE_DISK *)device->device_extension;
    uint64_t offset = request->parameters.read_write.offset;
    if (!request->buffer || !request->length || (offset & 511) ||
        (request->length & 511) || (offset >> 9) > 0xFFFFFFFFULL)
        return IO_STATUS_INVALID_PARAMETER;
    if (!ide_transfer(disk, (uint32_t)(offset >> 9), (uint8_t *)request->buffer,
                      request->length >> 9, request->major_function == IO_MJ_WRITE))
        return IO_STATUS_DEVICE_ERROR;
    request->io_status.information = request->length;
    return IO_STATUS_SUCCESS;
}

static int ide_probe(uint16_t io_base, uint16_t control_base, uint8_t slave) {
    uint16_t identify[256];
    IDE_DISK probe;
    probe.io_base = io_base;
    probe.control_base = control_base;
    probe.slave = slave;
    probe.lba48 = 0;
    probe.sectors = 0;

    outb(io_base + ATA_DRIVE, (uint8_t)(0xA0 | (slave << 4)));
    ide_delay(&probe);
    outb(io_base + ATA_SECCOUNT, 0);
    outb(io_base + ATA_LBA_LOW, 0);
    outb(io_base + ATA_LBA_MID, 0);
    outb(io_base + ATA_LBA_HIGH, 0);
    outb(io_base + ATA_COMMAND, ATA_CMD_IDENTIFY);
    if (inb(io_base + ATA_STATUS) == 0) return 0;
    if (!ide_wait(&probe, 1)) return 0;
    /* Non-zero signature bytes identify ATAPI/SATA packet devices, not disks. */
    if (inb(io_base + ATA_LBA_MID) || inb(io_base + ATA_LBA_HIGH)) return 0;
    for (uint32_t i = 0; i < 256; ++i) identify[i] = inw(io_base + ATA_DATA);
    probe.sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
    if (identify[83] & (1U << 10)) {
        uint64_t sectors48 = (uint64_t)identify[100] |
            ((uint64_t)identify[101] << 16) |
            ((uint64_t)identify[102] << 32) |
            ((uint64_t)identify[103] << 48);
        if (sectors48) {
            probe.lba48 = 1;
            probe.sectors = sectors48 > 0xFFFFFFFFULL
                ? 0xFFFFFFFFU : (uint32_t)sectors48;
        }
    }
    if (!probe.sectors || disk_count >= 4) return 0;

    char name[16] = "Harddisk0";
    name[8] = (char)('0' + disk_count);
    IO_DEVICE_OBJECT *device = IoCreateDevice(ide_driver, name, sizeof(IDE_DISK));
    if (!device) return 0;
    memcpy(device->device_extension, &probe, sizeof(probe));
    SerialPutString("[IDE] Attached ");
    SerialPutString(name);
    SerialPutString(", 512-byte sectors: ");
    SerialPrintDec(probe.sectors);
    SerialPutString("\r\n");
    ++disk_count;
    disk_sectors[disk_count - 1] = probe.sectors;
    return 1;
}

int IdeInitialize(IO_DRIVER_OBJECT *driver) {
    if (!driver) return 0;
    ide_driver = driver;
    disk_count = 0;
    memset(disk_sectors, 0, sizeof(disk_sectors));
    driver->major_function[IO_MJ_READ] = ide_dispatch;
    driver->major_function[IO_MJ_WRITE] = ide_dispatch;
    ide_probe(0x1F0, 0x3F6, 0);
    ide_probe(0x1F0, 0x3F6, 1);
    ide_probe(0x170, 0x376, 0);
    ide_probe(0x170, 0x376, 1);
    return disk_count != 0;
}

uint32_t IdeGetDiskCount(void) { return disk_count; }
uint32_t IdeGetDiskSectors(uint32_t index) {
    return index < disk_count ? disk_sectors[index] : 0;
}

int IdeBootInitialize(void) {
    IO_DRIVER_OBJECT *driver = IoCreateDriver("BootIde", 0, 0);
    if (!driver) return 0;
    return IdeInitialize(driver);
}

/* Weak so the boot-linked copy can coexist with other built-in boot drivers. */
__attribute__((weak)) int DriverEntry(IO_DRIVER_OBJECT *driver, void *context) {
    (void)context;
    return IdeInitialize(driver);
}
