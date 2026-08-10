#ifndef CDFS_H
#define CDFS_H
#include <stdint.h>

// ISO 9660 structures
typedef struct {
    uint8_t  type;
    char     id[5];           // "CD001"
    uint8_t  version;
    uint8_t  unused1;
    char     system_id[32];
    char     volume_id[32];
    uint8_t  unused2[8];
    uint32_t volume_space_size_l;
    uint32_t volume_space_size_m;
    uint8_t  unused3[32];
    uint16_t volume_set_size_l;
    uint16_t volume_set_size_m;
    uint16_t volume_seq_num_l;
    uint16_t volume_seq_num_m;
    uint16_t block_size_l;
    uint16_t block_size_m;
    uint32_t path_table_size_l;
    uint32_t path_table_size_m;
    uint32_t path_table_l;
    uint32_t path_table_opt_l;
    uint32_t path_table_m;
    uint32_t path_table_opt_m;
    uint8_t  root_dir_record[34];
    char     volume_set_id[128];
    char     publisher_id[128];
    char     data_preparer_id[128];
    char     application_id[128];
    char     copyright_file_id[37];
    char     abstract_file_id[37];
    char     bibliographic_file_id[37];
    char     creation_date[17];
    char     modification_date[17];
    char     expiration_date[17];
    char     effective_date[17];
    uint8_t  file_structure_version;
    uint8_t  unused4;
    uint8_t  application_used[512];
    uint8_t  reserved[653];
} __attribute__((packed)) ISO_PRIMARY_DESCRIPTOR;

typedef struct {
    uint8_t  length;
    uint8_t  ext_attr_length;
    uint32_t extent_l;
    uint32_t extent_m;
    uint32_t size_l;
    uint32_t size_m;
    uint8_t  date[7];
    uint8_t  flags;
    uint8_t  file_unit_size;
    uint8_t  interleave_gap;
    uint16_t vol_seq_num_l;
    uint16_t vol_seq_num_m;
    uint8_t  name_len;
    char     name[];
} __attribute__((packed)) ISO_DIR_ENTRY;

#define CDFS_SECTOR_SIZE 2048

// ATAPI / ATA ports for CD-ROM
#define ATA_PRIMARY_DATA    0x1F0
#define ATA_PRIMARY_ERR     0x1F1
#define ATA_PRIMARY_COUNT   0x1F2
#define ATA_PRIMARY_LBA_LO  0x1F3
#define ATA_PRIMARY_LBA_MID 0x1F4
#define ATA_PRIMARY_LBA_HI  0x1F5
#define ATA_PRIMARY_DRIVE   0x1F6
#define ATA_PRIMARY_CMD     0x1F7
#define ATA_PRIMARY_STATUS  0x1F7

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_IDENTIFY     0xEC
#define ATA_CMD_PACKET       0xA0

void CdfsInit(void);
int CdfsReadSector(uint32_t lba, uint8_t *buffer);
int CdfsFindFile(const char *path, uint32_t *out_lba, uint32_t *out_size);
int CdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size);

#endif