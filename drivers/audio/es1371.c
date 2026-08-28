#include <stdint.h>
#include "audio.h"
#include "io/io.h"
#include "io/pci.h"
#include "io/port.h"
#include "mm/vmm.h"
#include "cpu.h"
#include "serial.h"
#include "core/util.h"

/* Ensoniq AudioPCI ES1371, used by VMware's virtual sound device. */
#define ES_VENDOR_ENSONIQ 0x1274U
#define ES_DEVICE_ES1370  0x5000U
#define ES_DEVICE_ES1371  0x1371U
#define ES_DEVICE_ES1373  0x5880U
#define ES_DEVICE_EV1938  0x8938U
#define ES_CLASS          0x040100U

#define ES_CONTROL        0x00
#define ES_STATUS         0x04
#define ES_SMPRATE        0x10
#define ES_CODEC          0x14
#define ES_MEM_PAGE       0x0c
#define ES_SERIAL         0x20
#define ES_DAC1_COUNT     0x24
#define ES_DAC1_FRAME     0x30
#define ES_DAC1_SIZE      0x34

#define ES_BREQ           (1U << 7)
#define ES_DAC1_EN        (1U << 6)
#define ES_SYNC_RES       (1U << 14)
#define ES_DAC1_IRQ       (1U << 2)

#define ES_P1_LOOP_SEL    (1U << 13)
#define ES_P1_SCT_RLD     (1U << 7)
#define ES_P1_INT_EN      (1U << 8)
#define ES_P1_MODE_STEREO16 3U

#define ES_SRC_RAM_WE     (1U << 24)
#define ES_SRC_RAM_BUSY   (1U << 23)
#define ES_SRC_DISABLE    (1U << 22)
#define ES_DIS_P1         (1U << 21)
#define ES_DIS_P2         (1U << 20)
#define ES_DIS_R1         (1U << 19)

#define ES_SRC_DAC1       0x70
#define ES_SRC_INT_REGS   0x01
#define ES_SRC_VFREQ_FRAC 0x03

#define ES_CODEC_RDY      (1U << 31)
#define ES_CODEC_WIP      (1U << 30)
#define ES_CODEC_PIRD     (1U << 23)

#define ES_BUFFER_BYTES   131072U

typedef struct {
    uint16_t io;
    uint8_t legacy_1370;
    uint8_t *buffer;
    uint32_t control;
    uint32_t serial;
} ES_STATE;

static ES_STATE es;

static inline uint32_t es_read(uint16_t reg) { return inl(es.io + reg); }
static inline void es_write(uint16_t reg, uint32_t value) { outl(es.io + reg, value); }

static void es_log_hex(const char *label, uint32_t value) {
    SerialPutString(label);
    SerialPrintHex(value);
    SerialPutString("\r\n");
}

static uint32_t es_src_ready(void) {
    uint32_t value = 0;
    for (uint32_t i = 0; i < 0xa000U; ++i) {
        value = es_read(ES_SMPRATE);
        if (!(value & ES_SRC_RAM_BUSY)) return value;
        CpuRelax();
    }
    return value;
}

static void es_src_write(uint16_t reg, uint16_t data) {
    uint32_t state = es_src_ready() & (ES_SRC_DISABLE | ES_DIS_P1 | ES_DIS_P2 | ES_DIS_R1);
    es_write(ES_SMPRATE, state | ((uint32_t)(reg & 0x7f) << 25) |
             ES_SRC_RAM_WE | data);
    es_src_ready();
}

static void es_set_rate(uint32_t rate) {
    if (es.legacy_1370) return;
    /* ES1371's DAC converter uses freq = rate * 2^15 / 3000. */
    uint32_t freq = (rate * 32768U + 1500U) / 3000U;
    uint32_t state = es_src_ready() & (ES_SRC_DISABLE | ES_DIS_P2 | ES_DIS_R1);
    es_write(ES_SMPRATE, state | ES_DIS_P1);
    es_src_write(ES_SRC_DAC1 + ES_SRC_INT_REGS, (uint16_t)((freq >> 5) & 0xfc00U));
    es_src_write(ES_SRC_DAC1 + ES_SRC_VFREQ_FRAC, (uint16_t)(freq & 0x7fffU));
    es_write(ES_SMPRATE, es_src_ready() & (ES_SRC_DISABLE | ES_DIS_P2 | ES_DIS_R1));
}

static void es_codec_write(uint8_t reg, uint16_t value) {
    for (uint32_t i = 0; i < 0xa000U; ++i) {
        if (!(es_read(ES_CODEC) & ES_CODEC_WIP)) {
            es_write(ES_CODEC, ((uint32_t)(reg & 0x7f) << 16) | value);
            return;
        }
        CpuRelax();
    }
}

static int es_play(const void *data, uint32_t length, const AUDIO_FORMAT *format) {
    uint32_t words;
    if (!data || !length || length > ES_BUFFER_BYTES || (length & 3) ||
        !format || format->format != AUDIO_FORMAT_S16_STEREO ||
        format->channels != 2 || !format->sample_rate)
        return IO_STATUS_INVALID_PARAMETER;

    memcpy(es.buffer, data, length);
    words = length >> 2;

    SerialPutString("[ES1371] play bytes=");
    SerialPrintDec(length);
    SerialPutString(" rate=");
    SerialPrintDec(format->sample_rate);
    SerialPutString("\r\n");

    /* Keep the codec open and at full volume, matching the AC'97 endpoint. */
    es_codec_write(0x02, 0x0000);
    es_codec_write(0x18, 0x0000);
    es_set_rate(format->sample_rate);

    es.control &= ~ES_DAC1_EN;
    es_write(ES_CONTROL, es.control);
    es_write(ES_MEM_PAGE, 0x0c);
    es_write(ES_DAC1_FRAME, (uint32_t)VmmGetPhysicalAddress(es.buffer));
    es_write(ES_DAC1_SIZE, words - 1U);
    /* SIZE describes the circular frame; COUNT is the playback period and
       must also be programmed or the channel never advances on ES1371. */
    es_write(ES_DAC1_COUNT, words - 1U);
    es.serial &= ~(ES_P1_LOOP_SEL | ES_P1_SCT_RLD | 3U);
    es.serial |= ES_P1_LOOP_SEL | ES_P1_SCT_RLD | ES_P1_INT_EN | ES_P1_MODE_STEREO16;
    es_write(ES_SERIAL, es.serial);
    es_write(ES_STATUS, ES_DAC1_IRQ);
    es.control |= ES_BREQ | ES_DAC1_EN;
    es_write(ES_CONTROL, es.control);

    /* In stop mode the hardware reaches the end once and raises DAC1. */
    uint8_t started = 0;
    for (uint32_t wait = 0; wait < 0x02000000U; ++wait) {
        uint32_t current = es_read(ES_DAC1_SIZE) >> 16;
        if (current && current != 0xffffU) started = 1;
        if (es_read(ES_STATUS) & ES_DAC1_IRQ) break;
        if (started && current == 0) break;
        CpuRelax();
    }
    es.control &= ~ES_DAC1_EN;
    es_write(ES_CONTROL, es.control);
    es_write(ES_STATUS, ES_DAC1_IRQ);
    es_log_hex("[ES1371] status=0x", es_read(ES_STATUS));
    return IO_STATUS_SUCCESS;
}

static int es_control(IO_DEVICE_OBJECT *device, IO_REQUEST *request) {
    (void)device;
    if (request->parameters.device_control.code == AUDIO_IOCTL_GET_CAPS) {
        AUDIO_CAPS *caps = (AUDIO_CAPS *)request->buffer;
        if (!caps || request->length < sizeof(*caps)) return IO_STATUS_INVALID_PARAMETER;
        caps->format.sample_rate = es.legacy_1370 ? 44100 : 48000;
        caps->format.channels = 2;
        caps->format.format = AUDIO_FORMAT_S16_STEREO;
        caps->max_buffer_size = ES_BUFFER_BYTES;
        caps->flags = AUDIO_CAP_PLAYBACK;
        request->io_status.information = sizeof(*caps);
        return IO_STATUS_SUCCESS;
    }
    if (request->parameters.device_control.code == AUDIO_IOCTL_PLAY_PCM) {
        const AUDIO_PCM_PACKET *packet = (const AUDIO_PCM_PACKET *)request->parameters.device_control.input_buffer;
        if (!packet || request->parameters.device_control.input_length < sizeof(*packet) ||
            packet->sample_bytes > request->parameters.device_control.input_length - sizeof(*packet))
            return IO_STATUS_INVALID_PARAMETER;
        return es_play(packet + 1, packet->sample_bytes, &packet->format);
    }
    return IO_STATUS_NOT_SUPPORTED;
}

int Es1371Initialize(IO_DRIVER_OBJECT *driver) {
    if (!driver) return 0;
    SerialPutString("[ES1371] scanning PCI (I/O)\r\n");
    for (uint16_t bus = 0; bus < 256; ++bus)
        for (uint8_t slot = 0; slot < 32; ++slot)
            for (uint8_t fn = 0; fn < 8; ++fn) {
                uint32_t id = PciConfigRead32((uint8_t)bus, slot, fn, 0);
                uint16_t vendor = (uint16_t)id;
        uint16_t device = (uint16_t)(id >> 16);
        if (vendor != ES_VENDOR_ENSONIQ ||
                    (device != ES_DEVICE_ES1370 && device != ES_DEVICE_ES1371 &&
                     device != ES_DEVICE_ES1373 && device != ES_DEVICE_EV1938))
                    continue;
                uint32_t bar = PciConfigRead32((uint8_t)bus, slot, fn, 0x10);
                if (!(bar & 1)) continue;
                if ((PciConfigRead32((uint8_t)bus, slot, fn, 8) >> 8) != ES_CLASS) continue;
                uint32_t command = PciConfigRead32((uint8_t)bus, slot, fn, 4) | 0x0005U;
                PciConfigWrite32((uint8_t)bus, slot, fn, 4, command);
                es.io = (uint16_t)(bar & 0xfffcU);
                es.legacy_1370 = (device == ES_DEVICE_ES1370);
                es.buffer = (uint8_t *)VmmAllocatePages(32);
                if (!es.buffer) return 0;
                memset(es.buffer, 0, ES_BUFFER_BYTES);
                es.control = ES_BREQ;
                es.serial = 0;
                es_write(ES_CONTROL, es.control | ES_SYNC_RES);
                es_write(ES_CONTROL, es.control);
                es_codec_write(0x02, 0x0000);
                es_codec_write(0x18, 0x0000);
                if (!IoCreateDevice(driver, "AudioES1371", 0)) return 0;
                driver->major_function[IO_MJ_DEVICE_CONTROL] = es_control;
                SerialPutString("[ES1371] Playback device ready (Ensoniq AudioPCI, ");
                SerialPutString(es.legacy_1370 ? "ES1370" : "ES1371");
                SerialPutString(")\r\n");
                return 1;
            }
    return 0;
}

int DriverEntry(IO_DRIVER_OBJECT *driver, void *context) {
    (void)context;
    return Es1371Initialize(driver);
}
