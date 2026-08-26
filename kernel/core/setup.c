#include <stdint.h>
#include "core/setup.h"
#include "hal.h"
#include "keyboard.h"
#include "cdfs.h"
#include "ide.h"
#include "ahci.h"
#include "io/io.h"
#include "ob/object.h"
#include "core/util.h"
#include "cpu.h"

#define SETUP_COLOR 0x1F
#define SETUP_STATUS 0x70
#define ISO_SECTOR_SIZE 2048U

static void line(int row, const char *text) {
    HalSetCursor(2, row);
    HalPutString(text, SETUP_COLOR);
}

static void status(const char *text) {
    HalSetCursor(0, 24);
    HalPutString("                                                                                ", SETUP_STATUS);
    HalSetCursor(1, 24);
    HalPutString(text, SETUP_STATUS);
}

static void title(const char *page) {
    HalClearScreen(SETUP_COLOR);
    line(1, "discouNT Native-Mode Setup");
    line(2, page);
    line(3, "----------------------------------------------------------------------------");
}

static KEYBOARD_EVENT wait_key(void) {
    KEYBOARD_EVENT event;
    for (;;) {
        while (KeyboardHandleControllerEvent()) {}
        if (KeyboardPollEvent(&event) && event.pressed) return event;
        CpuRelax();
    }
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t target_sectors(uint32_t disk, int use_ahci) {
    return use_ahci ? AhciGetDiskSectors(disk) : IdeGetDiskSectors(disk);
}

static int write_target(uint32_t disk, int use_ahci, uint64_t offset,
                        void *data, uint32_t size) {
    IO_DEVICE_OBJECT *device;
    IO_REQUEST request;
    char name[16] = "Harddisk0";
    int result;
    if (use_ahci) strcpy(name, "SataDisk0");
    name[8] = (char)('0' + disk);
    device = IoGetDevice(name);
    if (!device) return 0;
    memset(&request, 0, sizeof(request));
    request.major_function = IO_MJ_WRITE;
    request.buffer = data;
    request.length = size;
    request.parameters.read_write.offset = offset;
    result = IoCallDriver(device, &request) == IO_STATUS_SUCCESS;
    ObDereferenceObject(device->handle);
    return result;
}

static int deploy(uint32_t disk, int use_ahci) {
    uint8_t sector[ISO_SECTOR_SIZE];
    uint32_t total, target_size = target_sectors(disk, use_ahci);
    char progress[64], number[16];
    if (!CdfsReadSector(16, sector) || sector[0] != 1 ||
        sector[1] != 'C' || sector[2] != 'D' || sector[3] != '0' ||
        sector[4] != '0' || sector[5] != '1') return 0;
    total = le32(sector + 80);
    if (!total || (uint64_t)total * 4 > target_size) return 0;
    for (uint32_t current = 0; current < total; ++current) {
        if (!CdfsReadSector(current, sector) ||
            !write_target(disk, use_ahci, (uint64_t)current * ISO_SECTOR_SIZE,
                          sector, ISO_SECTOR_SIZE)) return 0;
        if (!(current & 127) || current + 1 == total) {
            strcpy(progress, "Copying installation media: ");
            itoa((int)((uint64_t)(current + 1) * 100 / total), number, 10);
            strcat(progress, number);
            strcat(progress, "%");
            line(14, progress);
        }
    }
    return 1;
}

/* A fast format clears the area containing partition and filesystem metadata.
 * A slow format performs a complete zero-fill before the image is copied. */
static int format_disk(uint32_t disk, int use_ahci, int slow) {
    uint8_t zeros[ISO_SECTOR_SIZE];
    uint32_t total = target_sectors(disk, use_ahci);
    uint32_t sectors = slow ? total : (total > 2048 ? 2048 : total);
    uint32_t done = 0;
    char progress[64], number[16];
    memset(zeros, 0, sizeof(zeros));
    if (!sectors) return 0;
    while (done < sectors) {
        uint32_t batch = sectors - done;
        if (batch > 4) batch = 4;
        if (!write_target(disk, use_ahci, (uint64_t)done * 512,
                          zeros, batch * 512)) return 0;
        done += batch;
        if (!(done & 127) || done == sectors) {
            strcpy(progress, "Formatting disk: ");
            itoa((int)((uint64_t)done * 100 / sectors), number, 10);
            strcat(progress, number); strcat(progress, "%");
            line(14, progress);
        }
    }
    return 1;
}

void SetupRun(void) {
    uint32_t selected = 0, count = IdeGetDiskCount();
    int use_ahci = 0, slow_format = 0;
    KEYBOARD_EVENT key;
    title("Welcome to Setup");
    line(6, "This portion of Setup prepares discouNT for use on your computer.");
    line(8, "ENTER  Continue");
    line(9, "F3     Exit Setup");
    status("ENTER=Continue   F3=Exit");
    key = wait_key();
    if (key.scancode == 0x3D) goto exit_setup;
    if (key.ascii != '\n') goto exit_setup;

    title("Select the installation disk");
    if (!count) {
        use_ahci = AhciBootInitialize();
        count = AhciGetDiskCount();
    }
    if (!count) {
        line(7, "Setup did not find a supported ATA/SATA hard disk.");
        status("F3=Exit");
        while (wait_key().scancode != 0x3D) {}
        goto exit_setup;
    }
    for (;;) {
        char description[64], number[16];
        strcpy(description, use_ahci ? "> SataDisk" : "> Harddisk");
        itoa((int)selected, number, 10); strcat(description, number);
        strcat(description, "  (");
        itoa((int)((use_ahci ? AhciGetDiskSectors(selected) :
                   IdeGetDiskSectors(selected)) / 2048), number, 10);
        strcat(description, number); strcat(description, " MB)");
        line(7, description);
        line(10, "UP/DOWN selects a disk. ENTER continues.");
        status("ENTER=Install   UP/DOWN=Select   F3=Exit");
        key = wait_key();
        if (key.scancode == 0x3D) goto exit_setup;
        if (key.extended && key.scancode == 0x48 && selected) --selected;
        if (key.extended && key.scancode == 0x50 && selected + 1 < count) ++selected;
        if (key.ascii == '\n') break;
    }

    title("Choose format type");
    line(6, "Choose how the selected disk should be formatted.");
    line(8, "> Fast format");
    line(9, "  Slow format (zero the entire disk)");
    line(11, "Fast format clears existing disk metadata.");
    line(12, "Slow format may take a long time on large disks.");
    status("UP/DOWN=Select   ENTER=Continue   F3=Exit");
    for (;;) {
        key = wait_key();
        if (key.scancode == 0x3D) goto exit_setup;
        if (key.extended && key.scancode == 0x48 && slow_format) {
            slow_format = 0;
            line(8, "> Fast format");
            line(9, "  Slow format (zero the entire disk)");
        }
        if (key.extended && key.scancode == 0x50 && !slow_format) {
            slow_format = 1;
            line(8, "  Fast format");
            line(9, "> Slow format (zero the entire disk)");
        }
        if (key.ascii == '\n') break;
    }

    title("Confirm installation");
    line(6, "WARNING: All existing data on the selected disk will be overwritten.");
    line(8, slow_format ? "Slow format and install discouNT?" :
         "Fast format and install discouNT?");
    line(10, "Press F10 to continue, or F3 to cancel.");
    status("F10=Install   F3=Cancel");
    for (;;) {
        key = wait_key();
        if (key.scancode == 0x3D) goto exit_setup;
        if (key.scancode == 0x44) break;
    }

    title("Installing discouNT");
    line(6, slow_format ? "Formatting disk (slow format)..." :
         "Formatting disk (fast format)...");
    line(14, "Formatting disk: 0%");
    if (format_disk(selected, use_ahci, slow_format)) {
        line(6, "Copying system files to the disk...");
        line(14, "Copying files: 0%");
        if (deploy(selected, use_ahci)) {
            line(18, "Setup completed successfully.");
            line(20, "Remove the installation media and restart the computer.");
            status("ENTER=Restart");
        } else {
            line(18, "Setup could not copy the installation media.");
            line(20, "Check the source media and target disk capacity.");
            status("ENTER=Exit");
        }
    } else {
        line(18, "Setup could not format the selected disk.");
        line(20, "Check the target disk and restart Setup.");
        status("ENTER=Exit");
    }
    while (wait_key().ascii != '\n') {}
exit_setup:
    title("Setup is no longer running");
    line(8, "You may now restart the computer.");
    status("Setup stopped");
    CpuHalt();
}
