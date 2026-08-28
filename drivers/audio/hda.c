#include <stdint.h>
#include "audio.h"
#include "io/io.h"
#include "io/pci.h"
#include "mm/vmm.h"
#include "cpu.h"
#include "serial.h"
#include "core/util.h"

/* Intel High Definition Audio controller. VMware Fusion may expose this
   instead of the older Ensoniq AudioPCI device. */
#define HDA_CLASS       0x040300U
#define HDA_VENDOR_VMWARE 0x15adU
#define HDA_VENDOR_INTEL  0x8086U

#define HDA_GCAP         0x00
#define HDA_GCTL         0x08
#define HDA_STATESTS     0x0e
#define HDA_CORBLBASE    0x40
#define HDA_CORBUBASE    0x44
#define HDA_CORBWP       0x48
#define HDA_CORBRP       0x4a
#define HDA_CORBCTL      0x4c
#define HDA_RIRBLBASE    0x50
#define HDA_RIRBUBASE    0x54
#define HDA_RIRBWP       0x58
#define HDA_RINTCNT      0x5a
#define HDA_RIRBCTL      0x5c
#define HDA_STREAM_BASE  0x80
#define HDA_STREAM_SIZE  0x20
#define HDA_SD_CTL       0x00
#define HDA_SD_STS       0x03
#define HDA_SD_LPIB      0x04
#define HDA_SD_CBL       0x08
#define HDA_SD_LVI       0x0c
#define HDA_SD_FMT       0x12
#define HDA_SD_BDPL      0x18
#define HDA_SD_BDPU      0x1c

#define HDA_GCTL_RESET   0x00000001U
#define HDA_SD_RUN       0x00000002U
#define HDA_SD_SRST      0x00000001U
#define HDA_SD_TAG       (1U << 20)
#define HDA_CODEC_READY  0x80000000U
#define HDA_BDL_ENTRIES  4U
#define HDA_BUFFER_BYTES 131072U
#define HDA_SLOT_BYTES   (HDA_BUFFER_BYTES / HDA_BDL_ENTRIES)

typedef struct __attribute__((packed)) {
    uint32_t address_low;
    uint32_t address_high;
    uint32_t length;
    uint32_t flags;
} HDA_BDL;

typedef struct {
    uintptr_t mmio;
    uint8_t *bdl;
    uint8_t *buffer;
    uint8_t *corb;
    uint8_t *rirb;
    uint8_t codec;
    uint8_t output;
    uint8_t pin;
    uint8_t stream;
    uint32_t stream_mmio;
} HDA_STATE;

static HDA_STATE hda;

static inline uint32_t hread(uint32_t reg) { return *(volatile uint32_t *)(uintptr_t)(hda.mmio + reg); }
static inline void hwrite(uint32_t reg, uint32_t value) { *(volatile uint32_t *)(uintptr_t)(hda.mmio + reg) = value; }
static inline uint16_t hread16(uint32_t reg) { return *(volatile uint16_t *)(uintptr_t)(hda.mmio + reg); }
static inline void hwrite16(uint32_t reg, uint16_t value) { *(volatile uint16_t *)(uintptr_t)(hda.mmio + reg) = value; }
static inline uint8_t hread8(uint32_t reg) { return *(volatile uint8_t *)(uintptr_t)(hda.mmio + reg); }
static inline void hwrite8(uint32_t reg, uint8_t value) { *(volatile uint8_t *)(uintptr_t)(hda.mmio + reg) = value; }
static inline uint32_t sread(uint32_t reg) { return *(volatile uint32_t *)(uintptr_t)(hda.stream_mmio + reg); }
static inline void swrite(uint32_t reg, uint32_t value) { *(volatile uint32_t *)(uintptr_t)(hda.stream_mmio + reg) = value; }
static inline void swrite16(uint32_t reg, uint16_t value) { *(volatile uint16_t *)(uintptr_t)(hda.stream_mmio + reg) = value; }
static inline void swrite8(uint32_t reg, uint8_t value) { *(volatile uint8_t *)(uintptr_t)(hda.stream_mmio + reg) = value; }

static int hda_reset(void) {
    uint32_t before = hread(HDA_GCTL);
    hwrite(HDA_GCTL, hread(HDA_GCTL) & ~HDA_GCTL_RESET);
    for (uint32_t i = 0; i < 100000; ++i) { if (!(hread(HDA_GCTL) & HDA_GCTL_RESET)) break; CpuRelax(); }
    hwrite(HDA_GCTL, hread(HDA_GCTL) | HDA_GCTL_RESET);
    for (uint32_t i = 0; i < 100000; ++i) { if (hread(HDA_GCTL) & HDA_GCTL_RESET) {
        SerialPutString("[HDA] reset gctl before="); SerialPrintHex(before);
        SerialPutString(" after="); SerialPrintHex(hread(HDA_GCTL)); SerialPutString("\r\n");
        return 0;
    } CpuRelax(); }
    SerialPutString("[HDA] reset exit gctl="); SerialPrintHex(hread(HDA_GCTL)); SerialPutString("\r\n");
    SerialPutString("[HDA] reset timeout gctl="); SerialPrintHex(hread(HDA_GCTL)); SerialPutString("\r\n");
    return 1;
}

static int hda_rings_init(void) {
    uint32_t phys_corb = (uint32_t)VmmGetPhysicalAddress(hda.corb);
    uint32_t phys_rirb = (uint32_t)VmmGetPhysicalAddress(hda.rirb);
    memset(hda.corb, 0, 4096); memset(hda.rirb, 0, 4096);
    hwrite8(HDA_CORBCTL, 0); hwrite8(HDA_RIRBCTL, 0);
    hwrite16(HDA_CORBRP, 0x8000); hwrite16(HDA_CORBWP, 0);
    hwrite16(HDA_RIRBWP, 0); hwrite16(HDA_RINTCNT, 1);
    hwrite(HDA_CORBLBASE, phys_corb); hwrite(HDA_CORBUBASE, 0);
    hwrite(HDA_RIRBLBASE, phys_rirb); hwrite(HDA_RIRBUBASE, 0);
    hwrite8(HDA_CORBCTL, 0x02); hwrite8(HDA_RIRBCTL, 0x02);
    return 1;
}

static uint32_t hda_verb(uint8_t codec, uint8_t node, uint16_t verb, uint32_t payload) {
    uint8_t wp = (uint8_t)(hread8(HDA_CORBWP) + 1);
    uint32_t *corb = (uint32_t *)hda.corb;
    uint8_t old_rirb = hread8(HDA_RIRBWP);
    corb[wp] = ((uint32_t)codec << 28) | ((uint32_t)node << 20) |
               ((uint32_t)(verb & 0xfff) << 8) | (payload & 0xfffffU);
    hwrite8(HDA_CORBWP, wp);
    for (uint32_t i = 0; i < 100000; ++i) {
        if (hread8(HDA_RIRBWP) != old_rirb)
            return ((uint32_t *)hda.rirb)[(hread8(HDA_RIRBWP) & 0xff) * 2] & 0x00ffffffU;
        CpuRelax();
    }
    return 0xffffffffU;
}

static void hda_codec_setup(void) {
    uint32_t state = hread(HDA_STATESTS);
    for (uint8_t codec = 0; codec < 15; ++codec) {
        if (!(state & (1U << codec))) continue;
        uint32_t nodes = hda_verb(codec, 0, 0xf00, 4);
        uint8_t first = (uint8_t)(nodes >> 16), count = (uint8_t)nodes;
        uint8_t output = 0, pin = 0;
        for (uint16_t node = first; node < (uint16_t)first + count; ++node) {
            uint32_t caps = hda_verb(codec, (uint8_t)node, 0xf00, 9);
            uint8_t type = (uint8_t)((caps >> 20) & 0x0f);
            if (type == 0 && !output) output = (uint8_t)node;
            if (type == 4 && !pin) pin = (uint8_t)node;
        }
        if (output && pin) {
            hda.codec = codec; hda.output = output; hda.pin = pin;
            hda_verb(codec, pin, 0x701, 0);       /* route first pin input */
            hda_verb(codec, pin, 0x707, 0x40);    /* pin widget: output enabled */
            hda_verb(codec, output, 0x300, 0x00b0); /* unmute/max amp */
            SerialPutString("[HDA] codec="); SerialPrintDec(codec);
            SerialPutString(" output="); SerialPrintDec(output);
            SerialPutString(" pin="); SerialPrintDec(pin); SerialPutString("\r\n");
            return;
        }
    }
}

static int hda_play(const void *data, uint32_t length, const AUDIO_FORMAT *format) {
    HDA_BDL *bdl = (HDA_BDL *)hda.bdl;
    uint32_t stream_format;
    if (!data || !length || length > HDA_BUFFER_BYTES || (length & 3) ||
        !format || format->format != AUDIO_FORMAT_S16_STEREO || format->channels != 2)
        return IO_STATUS_INVALID_PARAMETER;
    if (format->sample_rate == 48000) stream_format = 0x0011;
    else if (format->sample_rate == 44100) stream_format = 0x4011;
    else return IO_STATUS_NOT_SUPPORTED;

    memcpy(hda.buffer, data, length);
    /* Bind the discovered converter to stream tag 1 and the exact PCM
       format. These are separate codec verbs from the controller format. */
    hda_verb(hda.codec, hda.output, 0x200, stream_format);
    hda_verb(hda.codec, hda.output, 0x706, 0x10);
    memset(bdl, 0, 4096);
    for (uint32_t i = 0; i < HDA_BDL_ENTRIES && i * HDA_SLOT_BYTES < length; ++i) {
        uint32_t left = length - i * HDA_SLOT_BYTES;
        uint32_t bytes = left > HDA_SLOT_BYTES ? HDA_SLOT_BYTES : left;
        bdl[i].address_low = (uint32_t)VmmGetPhysicalAddress(hda.buffer + i * HDA_SLOT_BYTES);
        bdl[i].length = bytes;
        bdl[i].flags = 1;
    }
    swrite(HDA_SD_CTL, 0);
    swrite(HDA_SD_CBL, length);
    swrite8(HDA_SD_LVI, (uint8_t)((length + HDA_SLOT_BYTES - 1) / HDA_SLOT_BYTES - 1));
    swrite16(HDA_SD_FMT, (uint16_t)stream_format);
    swrite(HDA_SD_BDPL, (uint32_t)VmmGetPhysicalAddress(hda.bdl));
    swrite(HDA_SD_BDPU, 0);
    swrite8(HDA_SD_STS, 0xff);
    swrite(HDA_SD_CTL, HDA_SD_TAG | HDA_SD_RUN);

    for (uint32_t wait = 0; wait < 0x02000000U; ++wait) {
        if (sread(HDA_SD_LPIB) >= length) break;
        CpuRelax();
    }
    swrite(HDA_SD_CTL, 0);
    swrite8(HDA_SD_STS, 0xff);
    SerialPutString("[HDA] lpib="); SerialPrintDec(sread(HDA_SD_LPIB));
    SerialPutString(" status="); SerialPrintHex(*(volatile uint8_t *)(uintptr_t)(hda.stream_mmio + HDA_SD_STS)); SerialPutString("\r\n");
    return IO_STATUS_SUCCESS;
}

static int hda_control(IO_DEVICE_OBJECT *device, IO_REQUEST *request) {
    (void)device;
    if (request->parameters.device_control.code == AUDIO_IOCTL_GET_CAPS) {
        AUDIO_CAPS *caps = (AUDIO_CAPS *)request->buffer;
        if (!caps || request->length < sizeof(*caps)) return IO_STATUS_INVALID_PARAMETER;
        caps->format.sample_rate = 48000; caps->format.channels = 2;
        caps->format.format = AUDIO_FORMAT_S16_STEREO; caps->max_buffer_size = HDA_BUFFER_BYTES;
        caps->flags = AUDIO_CAP_PLAYBACK; request->io_status.information = sizeof(*caps);
        return IO_STATUS_SUCCESS;
    }
    if (request->parameters.device_control.code == AUDIO_IOCTL_PLAY_PCM) {
        const AUDIO_PCM_PACKET *packet = (const AUDIO_PCM_PACKET *)request->parameters.device_control.input_buffer;
        if (!packet || request->parameters.device_control.input_length < sizeof(*packet) ||
            packet->sample_bytes > request->parameters.device_control.input_length - sizeof(*packet))
            return IO_STATUS_INVALID_PARAMETER;
        return hda_play(packet + 1, packet->sample_bytes, &packet->format);
    }
    return IO_STATUS_NOT_SUPPORTED;
}

int HdaInitialize(IO_DRIVER_OBJECT *driver) {
    if (!driver) return 0;
    SerialPutString("[HDA] scanning PCI (MMIO)\r\n");
    for (uint16_t bus = 0; bus < 256; ++bus)
        for (uint8_t slot = 0; slot < 32; ++slot)
            for (uint8_t fn = 0; fn < 8; ++fn) {
                uint32_t id = PciConfigRead32((uint8_t)bus, slot, fn, 0);
                uint32_t class = PciConfigRead32((uint8_t)bus, slot, fn, 8);
                uint16_t vendor = (uint16_t)id;
                if ((class >> 8) != HDA_CLASS || (vendor != HDA_VENDOR_VMWARE && vendor != HDA_VENDOR_INTEL)) continue;
                uint32_t bar = PciConfigRead32((uint8_t)bus, slot, fn, 0x10);
                SerialPutString("[HDA] controller id="); SerialPrintHex(id);
                SerialPutString(" class="); SerialPrintHex(class);
                SerialPutString(" bar="); SerialPrintHex(bar); SerialPutString("\r\n");
                if (!bar || (bar & 1)) continue;
                uint32_t command = PciConfigRead32((uint8_t)bus, slot, fn, 4) | 0x0006U;
                PciConfigWrite32((uint8_t)bus, slot, fn, 4, command);
                hda.mmio = (uintptr_t)VmmMapMmioRange((uint64_t)(bar & 0xfffffff0U), 0x1000);
                if (!hda.mmio) return 0;
                hda.bdl = (uint8_t *)VmmAllocatePages(1);
                hda.buffer = (uint8_t *)VmmAllocatePages(32);
                hda.corb = (uint8_t *)VmmAllocatePages(1);
                hda.rirb = (uint8_t *)VmmAllocatePages(1);
                if (!hda.bdl || !hda.buffer || !hda.corb || !hda.rirb) return 0;
                if (hda_reset()) { SerialPutString("[HDA] reset failed\r\n"); return 0; }
                if (!hda_rings_init()) { SerialPutString("[HDA] ring setup failed\r\n"); return 0; }
                hda_codec_setup();
                /* Intel HDA places input streams first and output streams
                   after them; stream 4 is the first playback descriptor on
                   the VMware/QEMU controller layout. */
                hda.stream = 4; hda.stream_mmio = hda.mmio + HDA_STREAM_BASE + 4 * HDA_STREAM_SIZE;
                if (!IoCreateDevice(driver, "AudioHDA", 0)) return 0;
                driver->major_function[IO_MJ_DEVICE_CONTROL] = hda_control;
                SerialPutString("[HDA] Playback device ready (Intel HD Audio)\r\n");
                return 1;
            }
    return 0;
}

int DriverEntry(IO_DRIVER_OBJECT *driver, void *context) {
    (void)context;
    return HdaInitialize(driver);
}
