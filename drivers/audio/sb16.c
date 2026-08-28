#include <stdint.h>
#include "audio.h"
#include "io/io.h"
#include "io/port.h"
#include "serial.h"
#include "cpu.h"

#define SB_BASE 0x220
#define SB_RESET (SB_BASE + 6)
#define SB_READ (SB_BASE + 0x0a)
#define SB_WRITE (SB_BASE + 0x0c)
#define SB_WRITE_STATUS (SB_BASE + 0x0c)
#define SB_READ_STATUS (SB_BASE + 0x0e)
#define SB_MIXER_INDEX (SB_BASE + 4)
#define SB_MIXER_DATA (SB_BASE + 5)

static int sb16_present;
static int sb16_wait_write(void) { for (uint32_t n = 100000; n; --n) if (!(inb(SB_WRITE_STATUS) & 0x80)) return 1; return 0; }
static int sb16_command(uint8_t value) { return sb16_wait_write() ? (outb(SB_WRITE, value), 1) : 0; }
static int sb16_reset(void) {
    outb(SB_RESET, 1); for (volatile uint32_t n = 10000; n; --n) CpuRelax(); outb(SB_RESET, 0);
    return (inb(SB_READ_STATUS) & 0x80) && inb(SB_READ) == 0xaa;
}
static int sb16_control(IO_DEVICE_OBJECT *device, IO_REQUEST *request) {
    AUDIO_CAPS *caps; (void)device;
    if (request->parameters.device_control.code == AUDIO_IOCTL_GET_CAPS) {
        caps = (AUDIO_CAPS *)request->buffer; if (!caps || request->length < sizeof(*caps)) return IO_STATUS_INVALID_PARAMETER;
        caps->format.sample_rate = 22050; caps->format.channels = 1; caps->format.format = AUDIO_FORMAT_S16_STEREO;
        caps->max_buffer_size = 0; caps->flags = 0; request->io_status.information = sizeof(*caps); return IO_STATUS_SUCCESS;
    }
    return IO_STATUS_NOT_SUPPORTED; /* DMA channel ownership is added with the mixer service. */
}
int Sb16Initialize(IO_DRIVER_OBJECT *driver) {
    if (!driver || !sb16_reset()) return 0;
    /* Mixer values are percentages on the emulated SB16. */
    outb(SB_MIXER_INDEX, 0x22); outb(SB_MIXER_DATA, 0xff);
    outb(SB_MIXER_INDEX, 0x04); outb(SB_MIXER_DATA, 0xff);
    sb16_present = 1; driver->major_function[IO_MJ_DEVICE_CONTROL] = sb16_control;
    if (!IoCreateDevice(driver, "AudioSB16", 0)) return 0;
    SerialPutString("[SB16] Codec detected; endpoint registered\r\n"); return 1;
}
int DriverEntry(IO_DRIVER_OBJECT *driver, void *context) { (void)context; return Sb16Initialize(driver); }
