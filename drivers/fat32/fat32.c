#include "fat32.h"
#include "io.h"
#include "object.h"
#include "mm.h"
#include "util.h"
#include "serial.h"

typedef struct __attribute__((packed)) {
    uint8_t jump[3], oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fats;
    uint16_t root_entries, total16;
    uint8_t media;
    uint16_t fat16, sectors_per_track, heads;
    uint32_t hidden_sectors, total32, fat32;
    uint16_t flags, version;
    uint32_t root_cluster;
} FAT32_BPB;

typedef struct __attribute__((packed)) {
    uint8_t name[11], attributes, nt_reserved, create_tenths;
    uint16_t create_time, create_date, access_date, cluster_high;
    uint16_t modify_time, modify_date, cluster_low;
    uint32_t size;
} FAT_DIRECTORY_ENTRY;

typedef struct {
    IO_DEVICE_OBJECT *device;
    uint32_t partition_lba, fat_lba, data_lba, root_cluster;
    uint32_t sectors_per_fat;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster, mounted;
} FAT32_VOLUME;

static FAT32_VOLUME volume;

static int read_bytes(uint64_t offset, void *buffer, uint32_t length) {
    IO_REQUEST request;
    memset(&request, 0, sizeof(request));
    request.major_function = IO_MJ_READ;
    request.buffer = buffer;
    request.length = length;
    request.parameters.read_write.offset = offset;
    return IoCallDriver(volume.device, &request) == IO_STATUS_SUCCESS;
}

static int read_sector(uint32_t lba, void *buffer) {
    return read_bytes((uint64_t)lba * volume.bytes_per_sector,
                      buffer, volume.bytes_per_sector);
}

static uint32_t next_cluster(uint32_t cluster, uint8_t *sector) {
    uint32_t fat_offset = cluster * 4;
    uint32_t lba = volume.fat_lba + fat_offset / volume.bytes_per_sector;
    uint32_t offset = fat_offset % volume.bytes_per_sector;
    if (!read_sector(lba, sector)) return 0x0FFFFFFFU;
    return (*(uint32_t *)(sector + offset)) & 0x0FFFFFFFU;
}

static uint32_t cluster_lba(uint32_t cluster) {
    return volume.data_lba + (cluster - 2) * volume.sectors_per_cluster;
}

static uint8_t upper(uint8_t c) { return c >= 'a' && c <= 'z' ? (uint8_t)(c - 32) : c; }
static int names_equal(const uint8_t *a, const uint8_t *b) {
    for (uint8_t i = 0; i < 11; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
static void short_name(const char *component, uint8_t name[11]) {
    uint32_t base = 0, extension = 8;
    memset(name, ' ', 11);
    while (*component && *component != '/' && *component != '\\') {
        if (*component == '.') { extension = 8; component++; break; }
        if (base < 8) name[base++] = upper((uint8_t)*component);
        component++;
    }
    while (*component && *component != '/' && *component != '\\') {
        if (extension < 11) name[extension++] = upper((uint8_t)*component);
        component++;
    }
}

static int find_entry(uint32_t directory_cluster, const char *component,
                      FAT_DIRECTORY_ENTRY *found, uint8_t *sector) {
    uint8_t wanted[11]; short_name(component, wanted);
    while (directory_cluster >= 2 && directory_cluster < 0x0FFFFFF8U) {
        uint32_t first = cluster_lba(directory_cluster);
        for (uint8_t s = 0; s < volume.sectors_per_cluster; ++s) {
            if (!read_sector(first + s, sector)) return 0;
            for (uint32_t offset = 0; offset < volume.bytes_per_sector; offset += 32) {
                FAT_DIRECTORY_ENTRY *entry = (FAT_DIRECTORY_ENTRY *)(sector + offset);
                if (entry->name[0] == 0) return 0;
                if (entry->name[0] == 0xE5 || entry->attributes == 0x0F ||
                    (entry->attributes & 8)) continue;
                if (names_equal(entry->name, wanted)) {
                    memcpy(found, entry, sizeof(*found)); return 1;
                }
            }
        }
        directory_cluster = next_cluster(directory_cluster, sector);
    }
    return 0;
}

int Fat32Initialize(const char *device_name) {
    uint8_t sector[512];
    FAT32_BPB *bpb;
    memset(&volume, 0, sizeof(volume));
    volume.device = IoGetDevice(device_name);
    if (!volume.device) return 0;
    volume.bytes_per_sector = 512;
    if (!read_sector(0, sector)) goto fail;
    /* Accept both partitioned disks and FAT32 superfloppies. */
    if (sector[510] == 0x55 && sector[511] == 0xAA &&
        (sector[0x1C2] == 0x0B || sector[0x1C2] == 0x0C))
        volume.partition_lba = *(uint32_t *)(sector + 0x1C6);
    if (!read_sector(volume.partition_lba, sector)) goto fail;
    bpb = (FAT32_BPB *)sector;
    if (bpb->bytes_per_sector != 512 || !bpb->sectors_per_cluster ||
        !bpb->reserved_sectors || !bpb->fat32 || !bpb->root_cluster) goto fail;
    volume.bytes_per_sector = bpb->bytes_per_sector;
    volume.sectors_per_cluster = bpb->sectors_per_cluster;
    volume.sectors_per_fat = bpb->fat32;
    volume.root_cluster = bpb->root_cluster;
    volume.fat_lba = volume.partition_lba + bpb->reserved_sectors;
    volume.data_lba = volume.fat_lba + (uint32_t)bpb->fats * bpb->fat32;
    volume.mounted = 1;
    SerialPutString("[FAT32] Mounted boot volume from ");
    SerialPutString(device_name);
    SerialPutString("\r\n");
    return 1;
fail:
    ObDereferenceObject(volume.device->handle);
    memset(&volume, 0, sizeof(volume));
    return 0;
}

int Fat32IsMounted(void) { return volume.mounted; }

int Fat32ReadFile(const char *path, uint8_t **buffer, uint32_t *size) {
    uint8_t *sector;
    FAT_DIRECTORY_ENTRY entry;
    uint32_t directory, cluster, copied = 0;
    const char *cursor = path;
    if (!volume.mounted || !path || !buffer || !size) return 0;
    sector = (uint8_t *)kmalloc(volume.bytes_per_sector);
    if (!sector) return 0;
    while (*cursor == '/' || *cursor == '\\') cursor++;
    directory = volume.root_cluster;
    for (;;) {
        const char *next = cursor;
        while (*next && *next != '/' && *next != '\\') next++;
        if (!find_entry(directory, cursor, &entry, sector)) { kfree(sector); return 0; }
        while (*next == '/' || *next == '\\') next++;
        cluster = ((uint32_t)entry.cluster_high << 16) | entry.cluster_low;
        if (!*next) break;
        if (!(entry.attributes & 0x10)) { kfree(sector); return 0; }
        directory = cluster; cursor = next;
    }
    if (entry.attributes & 0x10) { kfree(sector); return 0; }
    *buffer = (uint8_t *)kmalloc(entry.size ? entry.size : 1);
    if (!*buffer) { kfree(sector); return 0; }
    *size = entry.size;
    while (cluster >= 2 && cluster < 0x0FFFFFF8U && copied < entry.size) {
        uint32_t first = cluster_lba(cluster);
        for (uint8_t s = 0; s < volume.sectors_per_cluster && copied < entry.size; ++s) {
            uint32_t amount = entry.size - copied;
            if (amount > volume.bytes_per_sector) amount = volume.bytes_per_sector;
            if (!read_sector(first + s, sector)) { kfree(*buffer); kfree(sector); return 0; }
            memcpy(*buffer + copied, sector, amount); copied += amount;
        }
        cluster = next_cluster(cluster, sector);
    }
    kfree(sector);
    if (copied != entry.size) { kfree(*buffer); return 0; }
    return 1;
}
