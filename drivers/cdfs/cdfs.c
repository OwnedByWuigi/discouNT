#include <stdint.h>
#include "cdfs.h"
#include "portio.h"
#include "serial.h"
#include "mm.h"
#include "util.h"
#include "rtlpath.h"

static int cdrom_present = 0;
static int cdrom_ready = 0;
static int ide_base = 0;
static int ide_slave = 0;

static void ata_delay(void) {
    for (volatile int i = 0; i < 100; i++);
}

static int ata_wait_not_busy(int base) {
    int timeout = 100000;
    while ((inb(base + 7) & 0x80) && timeout--) ata_delay();
    return timeout > 0;
}

static int ata_wait_drq(int base) {
    int timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(base + 7);
        if (status & 0x01) return 0; // Error
        if (status & 0x08) return 1; // DRQ
        ata_delay();
    }
    return 0;
}

static int atapi_read_sector_internal(int base, int slave, uint32_t lba, uint8_t *buffer) {
    // Select drive with LBA mode
    outb(base + 6, slave ? 0xB0 : 0xA0);
    
    // Wait for BSY to clear and DRDY to set
    int timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(base + 7);
        if (!(status & 0x80) && (status & 0x40)) break; // !BSY && DRDY
        ata_delay();
    }
    if (timeout == 0) {
        SerialPutString("[ATAPI] DRDY timeout\r\n");
        return 0;
    }
    
    // Set features to 0 (DMA off)
    outb(base + 1, 0x00);
    
    // Set byte count to 2048
    outb(base + 4, (2048 >> 0) & 0xFF);  // Cylinder low = byte count low
    outb(base + 5, (2048 >> 8) & 0xFF);  // Cylinder high = byte count high
    
    // Send PACKET command
    outb(base + 7, 0xA0);
    
    // Wait for DRQ or error
    timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(base + 7);
        if (status & 0x01) {  // Error
            uint8_t err = inb(base + 1);
            SerialPutString("[ATAPI] Error 0x");
            SerialPrintHex(err);
            uint8_t ir = inb(base + 2);
            SerialPutString(" reason 0x");
            SerialPrintHex(ir);
            SerialPutString("\r\n");
            return 0;
        }
        if (status & 0x08) break;  // DRQ
        ata_delay();
    }
    if (timeout == 0) {
        SerialPutString("[ATAPI] DRQ timeout\r\n");
        return 0;
    }
    
    // Check interrupt reason (should be 0 = command)
    uint8_t reason = inb(base + 2);
    SerialPutString("[ATAPI] Reason: 0x");
    SerialPrintHex(reason);
    SerialPutString("\r\n");
    
    // Send READ(12) command packet
    uint8_t packet[12];
    packet[0] = 0xA8;  // READ(12)
    packet[1] = 0x00;  // Reserved
    packet[2] = (lba >> 24) & 0xFF;
    packet[3] = (lba >> 16) & 0xFF;
    packet[4] = (lba >> 8) & 0xFF;
    packet[5] = lba & 0xFF;
    packet[6] = 0x00;  // Reserved
    packet[7] = 0x00;  // Reserved
    packet[8] = 0x00;  // Reserved
    packet[9] = 0x01;  // Transfer length (1 sector)
    packet[10] = 0x00; // Reserved
    packet[11] = 0x00; // Reserved
    
    // Send the 12-byte packet as 6 words
    uint16_t *pkt16 = (uint16_t*)packet;
    for (int i = 0; i < 6; i++) {
        outw(base, pkt16[i]);
    }
    
    // Wait for data
    timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(base + 7);
        if (status & 0x01) {  // Error
            uint8_t err = inb(base + 1);
            SerialPutString("[ATAPI] Read error 0x");
            SerialPrintHex(err);
            SerialPutString("\r\n");
            return 0;
        }
        if (status & 0x08) break;  // DRQ
        ata_delay();
    }
    if (timeout == 0) {
        SerialPutString("[ATAPI] Data timeout\r\n");
        return 0;
    }
    
    // Get transfer size
    int byte_count = inb(base + 5) << 8 | inb(base + 4);
    if (byte_count == 0) byte_count = 2048;
    
    SerialPutString("[ATAPI] Transferring ");
    SerialPrintDec(byte_count);
    SerialPutString(" bytes\r\n");
    
    // Read data
    uint16_t *buf16 = (uint16_t*)buffer;
    for (int i = 0; i < byte_count / 2 && i < 1024; i++) {
        buf16[i] = inw(base);
    }
    
    return 1;
}

static int probe_ide_device(int base, int slave) {
    uint8_t drive_sel = slave ? 0xB0 : 0xA0;
    
    SerialPutString("[ATA] Probe IDE 0x");
    SerialPrintHex(base);
    SerialPutString(slave ? " slave...\r\n" : " master...\r\n");
    
    // Select drive
    outb(base + 6, drive_sel);
    ata_delay();
    ata_delay();
    
    // Read status
    uint8_t status = inb(base + 7);
    SerialPutString("[ATA] Status: 0x");
    SerialPrintHex(status);
    SerialPutString("\r\n");
    
    // Floating bus - no device
    if (status == 0xFF || status == 0x00) {
        SerialPutString("[ATA] Floating bus - no device\r\n");
        return 0;
    }
    
    // Wait for BSY to clear
    if (!ata_wait_not_busy(base)) {
        SerialPutString("[ATA] BSY timeout - ghost device?\r\n");
        return 0;
    }
    
    // Check signature registers without sending any command
    uint8_t sc = inb(base + 2);  // Sector count
    uint8_t sn = inb(base + 3);  // Sector number / LBA low
    uint8_t cl = inb(base + 4);  // Cylinder low / LBA mid
    uint8_t ch = inb(base + 5);  // Cylinder high / LBA high
    
    SerialPutString("[ATA] Regs: SC=0x");
    SerialPrintHex(sc);
    SerialPutString(" SN=0x");
    SerialPrintHex(sn);
    SerialPutString(" CL=0x");
    SerialPrintHex(cl);
    SerialPutString(" CH=0x");
    SerialPrintHex(ch);
    SerialPutString("\r\n");
    
    // ATAPI signature: SC=0x01 SN=0x01 CL=0x14 CH=0xEB
    if (cl == 0x14 && ch == 0xEB) {
        SerialPutString("[ATA] ATAPI signature detected!\r\n");
        return 1;
    }
    
    // Also try with 0x00/0x00 for regular ATA
    if (cl == 0x00 && ch == 0x00 && sc == 0x01 && sn == 0x01) {
        SerialPutString("[ATA] ATA hard disk (not CD-ROM)\r\n");
        return 0;
    }
    
    // Try IDENTIFY command
    outb(base + 6, drive_sel);
    ata_delay();
    outb(base + 7, 0xEC);  // IDENTIFY
    
    ata_delay();
    
    status = inb(base + 7);
    SerialPutString("[ATA] IDENTIFY status: 0x");
    SerialPrintHex(status);
    SerialPutString("\r\n");
    
    if (status == 0 || status == 0xFF) {
        return 0;
    }
    
    // Wait for data
    if (ata_wait_drq(base)) {
        uint16_t buf[256];
        for (int i = 0; i < 256; i++) buf[i] = inw(base);
        
        SerialPutString("[ATA] IDENTIFY word 0: 0x");
        SerialPrintHex(buf[0]);
        SerialPutString("\r\n");
        
        // ATAPI device has bit 15 set, bit 7 set
        if ((buf[0] & 0x8000) && (buf[0] & 0x0080)) {
            SerialPutString("[ATA] ATAPI from IDENTIFY!\r\n");
            return 1;
        }
    }
    
    // Try ATAPI soft reset
    outb(base + 6, drive_sel);
    ata_delay();
    outb(base + 7, 0x08);  // DEVICE RESET
    
    // Wait 5ms
    for (volatile int i = 0; i < 50000; i++) ata_delay();
    
    // Check signature again after reset
    cl = inb(base + 4);
    ch = inb(base + 5);
    
    SerialPutString("[ATA] After reset: CL=0x");
    SerialPrintHex(cl);
    SerialPutString(" CH=0x");
    SerialPrintHex(ch);
    SerialPutString("\r\n");
    
    if (cl == 0x14 && ch == 0xEB) {
        SerialPutString("[ATA] ATAPI after reset!\r\n");
        return 1;
    }
    
    return 0;
}

void CdfsInit(void) {
    SerialPutString("[CDFS] Initializing CD-ROM...\r\n");
    
    cdrom_present = 0;
    cdrom_ready = 0;
    
    // Probe all 4 possible IDE positions
    struct { int base; int slave; } ports[] = {
        {0x1F0, 0}, {0x1F0, 1}, {0x170, 0}, {0x170, 1}
    };
    
    for (int i = 0; i < 4; i++) {
        if (probe_ide_device(ports[i].base, ports[i].slave)) {
            ide_base = ports[i].base;
            ide_slave = ports[i].slave;
            cdrom_present = 1;
            break;
        }
    }
    
    if (!cdrom_present) {
        SerialPutString("[CDFS] No CD-ROM found on any IDE port\r\n");
        SerialPutString("[CDFS] Make sure QEMU has: -cdrom or -drive media=cdrom\r\n");
        return;
    }
    
    SerialPutString("[CDFS] CD-ROM found at IDE 0x");
    SerialPrintHex(ide_base);
    SerialPutString(ide_slave ? " slave\r\n" : " master\r\n");
    
    // Read primary volume descriptor (sector 16)
    uint8_t sector[CDFS_SECTOR_SIZE];
    
    if (!atapi_read_sector_internal(ide_base, ide_slave, 16, sector)) {
        SerialPutString("[CDFS] Failed to read sector 16\r\n");
        SerialPutString("[CDFS] Trying sector 17...\r\n");
        
        // Try sector 17 (sometimes the primary descriptor is there)
        if (!atapi_read_sector_internal(ide_base, ide_slave, 17, sector)) {
            SerialPutString("[CDFS] Failed to read sector 17 too\r\n");
            return;
        }
    }
    
    // Check for ISO 9660 signature
    if (sector[0] == 1 && 
        sector[1] == 'C' && sector[2] == 'D' && 
        sector[3] == '0' && sector[4] == '0' && sector[5] == '1') {
        
        cdrom_ready = 1;
        SerialPutString("[CDFS] ISO 9660 filesystem found!\r\n");
        
        // Print volume ID
        SerialPutString("[CDFS] Volume: ");
        for (int i = 0; i < 32; i++) {
            char c = sector[40 + i];
            if (c >= ' ' && c <= '~') SerialPutChar(c);
        }
        SerialPutString("\r\n");
    } else {
        SerialPutString("[CDFS] No ISO 9660 signature at sector 16/17\r\n");
        SerialPutString("[CDFS] First bytes: ");
        for (int i = 0; i < 6; i++) {
            SerialPrintHex(sector[i]);
            SerialPutString(" ");
        }
        SerialPutString("\r\n");
    }
}

int CdfsReadSector(uint32_t lba, uint8_t *buffer) {
    if (!cdrom_present || !cdrom_ready) return 0;
    return atapi_read_sector_internal(ide_base, ide_slave, lba, buffer);
}

int CdfsFindFile(const char *path, uint32_t *out_lba, uint32_t *out_size) {
    char normalized_path[256];
    char component[256];
    const char *cursor;
    int component_result;
    if (!cdrom_ready || !path || !out_lba || !out_size) return 0;
    
    // Read primary descriptor to get root directory
    uint8_t sector[CDFS_SECTOR_SIZE];
    if (!CdfsReadSector(16, sector)) return 0;
    
    uint8_t *root_record = sector + 156;
    uint32_t dir_lba = *(uint32_t*)(root_record + 2);
    uint32_t dir_size = *(uint32_t*)(root_record + 10);
    
    SerialPutString("[CDFS] Finding: ");
    SerialPutString(path);
    SerialPutString("\r\n");
    
    if (RtlNormalizePath(path, normalized_path, sizeof(normalized_path)) < 0)
        return 0;
    cursor = normalized_path;
    component_result = RtlNextPathComponent(&cursor, component, sizeof(component));
    
    // Navigate through path components
    while (component_result > 0) {
        SerialPutString("[CDFS] Searching for: ");
        SerialPutString(component);
        SerialPutString("\r\n");
        
        uint32_t sectors = (dir_size + CDFS_SECTOR_SIZE - 1) / CDFS_SECTOR_SIZE;
        uint8_t *dir_buf = (uint8_t*)kmalloc(sectors * CDFS_SECTOR_SIZE);
        if (!dir_buf) return 0;
        
        for (uint32_t i = 0; i < sectors; i++)
            CdfsReadSector(dir_lba + i, dir_buf + i * CDFS_SECTOR_SIZE);
        
        int off = 0;
        int found = 0;
        uint32_t found_lba = 0, found_size = 0;
        int found_is_dir = 0;
        
        while (off < (int)(sectors * CDFS_SECTOR_SIZE)) {
            uint8_t len = dir_buf[off];
            if (len == 0) {
                int next_sector_off = ((off / CDFS_SECTOR_SIZE) + 1) * CDFS_SECTOR_SIZE;
                if (next_sector_off <= off) break;
                off = next_sector_off;
                continue;
            }
            
            uint8_t flags = dir_buf[off + 25];
            uint8_t name_len = dir_buf[off + 32];
            char *name = (char*)(dir_buf + off + 33);
            
            // Build entry name (strip version)
            char entry_name[256];
            int ei = 0;
            for (int i = 0; i < name_len && ei < 254; i++) {
                if (name[i] == ';') break;
                entry_name[ei++] = name[i];
            }
            entry_name[ei] = 0;
            
            SerialPutString("[CDFS]   Entry: ");
            SerialPutString(entry_name);
            SerialPutString(flags & 0x02 ? " (dir)\r\n" : " (file)\r\n");
            
            if (strcmp(component, entry_name) == 0) {
                found_lba = *(uint32_t*)(dir_buf + off + 2);
                found_size = *(uint32_t*)(dir_buf + off + 10);
                found_is_dir = (flags & 0x02) ? 1 : 0;
                found = 1;
                SerialPutString("[CDFS] Found!\r\n");
                break;
            }
            
            off += len;
        }
        
        kfree(dir_buf);
        
        if (!found) {
            SerialPutString("[CDFS] Not found: ");
            SerialPutString(component);
            SerialPutString("\r\n");
            return 0;
        }
        
        if (!*cursor) {
            // This is the final component
            *out_lba = found_lba;
            *out_size = found_size;
            SerialPutString("[CDFS] Final: LBA=");
            SerialPrintDec(found_lba);
            SerialPutString(" size=");
            SerialPrintDec(found_size);
            SerialPutString("\r\n");
            return 1;
        } else if (found_is_dir) {
            dir_lba = found_lba;
            dir_size = found_size;
            component_result = RtlNextPathComponent(&cursor, component,
                                                     sizeof(component));
        } else {
            SerialPutString("[CDFS] Not a directory!\r\n");
            return 0;
        }
    }
    
    return 0;
}

int CdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size) {
    uint32_t lba, size;
    uint32_t sectors;
    uint32_t alloc_size;
    
    if (!CdfsFindFile(path, &lba, &size)) return 0;
    
    sectors = (size + 2047) / 2048;
    alloc_size = sectors * 2048;
    if (alloc_size == 0) alloc_size = 1;

    uint8_t *buffer = (uint8_t*)kmalloc(alloc_size);
    if (!buffer) {
        SerialPutString("[CDFS] Out of memory!\r\n");
        return 0;
    }
    memset(buffer, 0, alloc_size);
    
    SerialPutString("[CDFS] Reading ");
    SerialPrintDec(size);
    SerialPutString(" bytes from LBA ");
    SerialPrintDec(lba);
    SerialPutString("...\r\n");
    
    for (uint32_t i = 0; i < sectors; i++) {
        if (!CdfsReadSector(lba + i, buffer + i * 2048)) {
            SerialPutString("[CDFS] Read error at sector ");
            SerialPrintDec(lba + i);
            SerialPutString("\r\n");
            kfree(buffer);
            return 0;
        }
    }
    
    *out_buffer = buffer;
    *out_size = size;
    return 1;
}
