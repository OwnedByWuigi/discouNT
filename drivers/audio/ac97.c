#include <stdint.h>
#include "audio.h"
#include "io/io.h"
#include "io/pci.h"
#include "io/port.h"
#include "mm/vmm.h"
#include "cpu.h"
#include "core/ke.h"
#include "serial.h"
#include "core/util.h"

#define AC97_CLASS          0x040100U

#define AC97_NAM_MASTER     0x02
#define AC97_NAM_PCM        0x18
#define AC97_NAM_EXTID      0x28
#define AC97_NAM_EXTSTAT    0x2a
#define AC97_NAM_PCM_RATE   0x2c

#define AC97_BM_PCM         0x10
#define AC97_BM_BDL         0x00
#define AC97_BM_CIV         0x04
#define AC97_BM_LVI         0x05
#define AC97_BM_STATUS      0x06
#define AC97_BM_PICB        0x08
#define AC97_BM_CONTROL     0x0b

#define AC97_BM_RESET       0x02
#define AC97_BM_IOCE        0x10
#define AC97_BM_CR_START    0x01

#define AC97_BM_ST_DCH      0x01
#define AC97_BM_ST_CELV     0x04
#define AC97_BM_ST_LVBCI    0x08
#define AC97_BM_ST_IOC      0x10

typedef struct __attribute__((packed)) {
    uint32_t address;
    uint32_t length_ioc; /* low 16 bits = frames (samples per channel), bit 31 = IOC */
} AC97_BDL;

typedef struct {
    uint16_t mixer;
    uint16_t bus;
    uint8_t *bdl;
    uint8_t *buffer;
} AC97_STATE;

#define AC97_MAX_BDL_ENTRIES 32
#define AC97_STREAM_SLOTS    32
/* Keep descriptors below the controller's large-buffer edge case.  The
   BDL length is a 16-bit count of 16-bit samples; 32 KiB is 16384 samples
   and is also small enough for older AC'97 implementations. */
#define AC97_STREAM_SLOT_BYTES 32768U
#define AC97_BUFFER_BYTES    (AC97_STREAM_SLOT_BYTES * AC97_STREAM_SLOTS)
#define AC97_STREAM_PREROLL   4

static AC97_STATE ac97;
static uint8_t ac97_stream_active;
static uint8_t ac97_stream_running;
static uint8_t ac97_stream_read;
static uint8_t ac97_stream_write_slot;
static uint8_t ac97_stream_queued;
static uint32_t ac97_stream_fill;
static uint32_t ac97_stream_commits;
static uint8_t ac97_last_civ;
static AUDIO_FORMAT ac97_stream_format;

static void ac97_log_hex(const char *label, uint32_t value) {
    SerialPutString(label);
    SerialPrintHex(value);
    SerialPutString("\r\n");
}

static inline uint16_t mixer_read(uint16_t reg) {
    return inw(ac97.mixer + reg);
}
static inline void mixer_write(uint16_t reg, uint16_t value) {
    outw(ac97.mixer + reg, value);
}

static inline uint8_t bm_read8(uint16_t off) {
    return inb(ac97.bus + off);
}
static inline void bm_write8(uint16_t off, uint8_t v) {
    outb(ac97.bus + off, v);
}
static inline uint16_t bm_read16(uint16_t off) {
    return inw(ac97.bus + off);
}
static inline void bm_write16(uint16_t off, uint16_t v) {
    outw(ac97.bus + off, v);
}
static inline uint32_t bm_read32(uint16_t off) {
    return inl(ac97.bus + off);
}
static inline void bm_write32(uint16_t off, uint32_t v) {
    outl(ac97.bus + off, v);
}

/* AC'97 stops at LVI; it is not an implicit endless ring. Extend LVI only
   after a completed descriptor has been refilled. */
static void ac97_stream_extend_dma(uint8_t slot) {
    uint8_t status;
    status = bm_read8(AC97_BM_PCM + AC97_BM_STATUS);

    /* LVBCI/IOC are write-one-to-clear.  A descriptor can be completely
       consumed while the controller is still running, so acknowledge the
       boundary before publishing the new LVI.  Leaving LVBCI latched makes
       some AC'97 implementations repeatedly service the old tail. */
    bm_write8(AC97_BM_PCM + AC97_BM_STATUS,
              AC97_BM_ST_LVBCI | AC97_BM_ST_IOC);
    bm_write8(AC97_BM_PCM + AC97_BM_LVI, slot);
    if (status & AC97_BM_ST_DCH) {
        bm_write8(AC97_BM_PCM + AC97_BM_CONTROL,
                  AC97_BM_IOCE | AC97_BM_CR_START);
        ac97_stream_running = 1;
        SerialPutString("[AC97] resumed at slot=");
        SerialPrintDec(slot);
        SerialPutString("\r\n");
    }
}

static void ac97_stream_reap(void) {
    uint8_t civ = bm_read8(AC97_BM_PCM + AC97_BM_CIV) & (AC97_STREAM_SLOTS - 1);
    uint16_t picb = bm_read16(AC97_BM_PCM + AC97_BM_PICB);
    if (civ != ac97_last_civ) {
        SerialPutString("[AC97] advance civ=");
        SerialPrintDec(civ);
        SerialPutString(" queued=");
        SerialPrintDec(ac97_stream_queued);
        SerialPutString(" picb=");
        SerialPrintDec(picb);
        SerialPutString("\r\n");
        ac97_last_civ = civ;
    }
    while (ac97_stream_queued && ac97_stream_read != civ) {
        ac97_stream_queued--;
        ac97_stream_read = (ac97_stream_read + 1) & (AC97_STREAM_SLOTS - 1);
    }
    /* QEMU leaves CIV on the final entry when it raises DCH. */
    if (ac97_stream_queued && (bm_read8(AC97_BM_PCM + AC97_BM_STATUS) & AC97_BM_ST_DCH) && !picb) {
        ac97_stream_queued--;
        ac97_stream_read = (ac97_stream_read + 1) & (AC97_STREAM_SLOTS - 1);
    }
}

static int ac97_stream_start(const AUDIO_FORMAT *format) {
    AC97_BDL *bdl = (AC97_BDL *)ac97.bdl;
    if (!format || format->format != AUDIO_FORMAT_S16_STEREO || format->channels != 2 || !format->sample_rate)
        return IO_STATUS_INVALID_PARAMETER;
    mixer_write(AC97_NAM_PCM_RATE, (uint16_t)format->sample_rate);
    mixer_write(AC97_NAM_MASTER, 0x0000);
    mixer_write(AC97_NAM_PCM, 0x0000);
    bm_write8(AC97_BM_PCM + AC97_BM_CONTROL, AC97_BM_RESET);
    for (uint32_t wait = 10000; wait && (bm_read8(AC97_BM_PCM + AC97_BM_CONTROL) & AC97_BM_RESET); --wait) CpuRelax();
    memset(bdl, 0, 4096);
    for (uint32_t i = 0; i < AC97_STREAM_SLOTS; ++i) {
        bdl[i].address = (uint32_t)VmmGetPhysicalAddress(ac97.buffer + i * AC97_STREAM_SLOT_BYTES);
    }
    bm_write32(AC97_BM_PCM + AC97_BM_BDL, (uint32_t)VmmGetPhysicalAddress(ac97.bdl));
    bm_write8(AC97_BM_PCM + AC97_BM_CIV, 0);
    bm_write8(AC97_BM_PCM + AC97_BM_LVI, 0);
    bm_write8(AC97_BM_PCM + AC97_BM_STATUS, AC97_BM_ST_DCH | AC97_BM_ST_CELV | AC97_BM_ST_LVBCI | AC97_BM_ST_IOC);
    ac97_stream_format = *format;
    ac97_stream_active = 1; ac97_stream_running = 0;
    ac97_stream_read = 0; ac97_stream_write_slot = 0; ac97_stream_queued = 0;
    ac97_stream_fill = 0;
    ac97_stream_commits = 0;
    ac97_last_civ = 0;
    SerialPutString("[AC97] stream start rate=");
    SerialPrintDec(format->sample_rate);
    SerialPutString(" slots=");
    SerialPrintDec(AC97_STREAM_SLOTS);
    SerialPutString(" slot-bytes=");
    SerialPrintDec(AC97_STREAM_SLOT_BYTES);
    SerialPutString("\r\n");
    return IO_STATUS_SUCCESS;
}

static void ac97_stream_rearm_batch(void) {
    AC97_BDL *bdl = (AC97_BDL *)ac97.bdl;
    SerialPutString("[AC97] rearm batch after drain\r\n");
    bm_write8(AC97_BM_PCM + AC97_BM_CONTROL, 0);
    bm_write8(AC97_BM_PCM + AC97_BM_STATUS,
              AC97_BM_ST_DCH | AC97_BM_ST_CELV | AC97_BM_ST_LVBCI | AC97_BM_ST_IOC);
    memset(bdl, 0, 4096);
    for (uint32_t i = 0; i < AC97_STREAM_SLOTS; ++i)
        bdl[i].address = (uint32_t)VmmGetPhysicalAddress(ac97.buffer + i * AC97_STREAM_SLOT_BYTES);
    bm_write8(AC97_BM_PCM + AC97_BM_CIV, 0);
    bm_write8(AC97_BM_PCM + AC97_BM_LVI, 0);
    ac97_stream_read = 0;
    ac97_stream_write_slot = 0;
    ac97_stream_queued = 0;
    ac97_stream_fill = 0;
    ac97_stream_running = 0;
}

static void ac97_stream_commit_slot(uint32_t length) {
    AC97_BDL *bdl = (AC97_BDL *)ac97.bdl;
    uint8_t slot = ac97_stream_write_slot;
    uint32_t samples = length >> 1;

    /* Re-publish the address as well as the length.  This is harmless on
       controllers that retain the address and avoids relying on a stale
       prefetched BDL entry when a slot is recycled after wraparound. */
    bdl[slot].address = (uint32_t)VmmGetPhysicalAddress(
        ac97.buffer + slot * AC97_STREAM_SLOT_BYTES);
    bdl[slot].length_ioc = (samples & 0xffffU) | 0x80000000U;
    ac97_stream_queued++;
    ac97_stream_write_slot = (ac97_stream_write_slot + 1) & (AC97_STREAM_SLOTS - 1);
    if (ac97_stream_running)
        ac97_stream_extend_dma(slot);
    ac97_stream_commits++;
    if (ac97_stream_commits <= 8 ||
        (ac97_stream_commits >= 32 && ac97_stream_commits <= 40) ||
        !(ac97_stream_commits & 31U)) {
        SerialPutString("[AC97] commit slot=");
        SerialPrintDec(slot);
        SerialPutString(" bytes=");
        SerialPrintDec(length);
        SerialPutString(" first=");
        SerialPrintHex(*(uint16_t *)(ac97.buffer + slot * AC97_STREAM_SLOT_BYTES));
        SerialPutString(" mid=");
        SerialPrintHex(*(uint16_t *)(ac97.buffer + slot * AC97_STREAM_SLOT_BYTES + 16384));
        SerialPutString(" last=");
        SerialPrintHex(*(uint16_t *)(ac97.buffer + slot * AC97_STREAM_SLOT_BYTES + length - 2));
        SerialPutString(" civ=");
        SerialPrintDec(bm_read8(AC97_BM_PCM + AC97_BM_CIV));
        SerialPutString(" picb=");
        SerialPrintDec(bm_read16(AC97_BM_PCM + AC97_BM_PICB));
        SerialPutString(" status=");
        SerialPrintHex(bm_read8(AC97_BM_PCM + AC97_BM_STATUS));
        SerialPutString("\r\n");
    }
}

static void ac97_stream_start_dma(uint8_t last_valid) {
    bm_write8(AC97_BM_PCM + AC97_BM_LVI, last_valid);
    bm_write8(AC97_BM_PCM + AC97_BM_CONTROL, AC97_BM_IOCE | AC97_BM_CR_START);
    ac97_stream_running = 1;
}

static int ac97_stream_write(const void *data, uint32_t length) {
    const uint8_t *source = (const uint8_t *)data;
    if (!ac97_stream_active || !data || !length || (length & 3))
        return IO_STATUS_INVALID_PARAMETER;

    while (length) {
        uint32_t take;

        /* AC'97 playback is driven asynchronously.  Yield while the ring is
           full so the scheduler can run the rest of the system and the
           controller can reach the descriptor we are waiting to recycle. */
        while (ac97_stream_queued == AC97_STREAM_SLOTS) {
            ac97_stream_reap();
            KeYield();
        }

        take = AC97_STREAM_SLOT_BYTES - ac97_stream_fill;
        if (take > length) take = length;
        memcpy(ac97.buffer + ac97_stream_write_slot * AC97_STREAM_SLOT_BYTES + ac97_stream_fill,
               source, take);
        ac97_stream_fill += take;
        source += take;
        length -= take;

        if (ac97_stream_fill == AC97_STREAM_SLOT_BYTES) {
            ac97_stream_commit_slot(ac97_stream_fill);
            ac97_stream_fill = 0;
            /* Prime the controller once.  Afterwards the driver advances LVI
               as each completed descriptor is refilled; this prevents both
               underrun gaps and replaying stale descriptors. */
            if (!ac97_stream_running && ac97_stream_queued == AC97_STREAM_SLOTS)
                ac97_stream_start_dma(AC97_STREAM_SLOTS - 1);
        }
    }
    return IO_STATUS_SUCCESS;
}

static int ac97_stream_stop(void) {
    if (!ac97_stream_active) return IO_STATUS_INVALID_PARAMETER;
    if (ac97_stream_fill) {
        ac97_stream_commit_slot(ac97_stream_fill);
        ac97_stream_fill = 0;
    }
    if (!ac97_stream_running && ac97_stream_queued)
        ac97_stream_start_dma((ac97_stream_write_slot + AC97_STREAM_SLOTS - 1) &
                              (AC97_STREAM_SLOTS - 1));
    else if (ac97_stream_running && ac97_stream_queued)
        /* The final tail is now known.  The ring may wrap once to this
           descriptor, then AC'97 stops instead of replaying old audio. */
        bm_write8(AC97_BM_PCM + AC97_BM_LVI,
                  (ac97_stream_write_slot + AC97_STREAM_SLOTS - 1) &
                  (AC97_STREAM_SLOTS - 1));
    while (ac97_stream_queued) {
        ac97_stream_reap();
        KeYield();
    }
    SerialPutString("[AC97] stream stop\r\n");
    bm_write8(AC97_BM_PCM + AC97_BM_STATUS, AC97_BM_ST_DCH | AC97_BM_ST_CELV | AC97_BM_ST_LVBCI | AC97_BM_ST_IOC);
    ac97_stream_active = 0;
    return IO_STATUS_SUCCESS;
}

/* Blocking compatibility wrapper backed by the streaming ring. */
static int ac97_play(const void *data, uint32_t length, const AUDIO_FORMAT *format) {
    uint32_t offset = 0;
    int status;
    if (!data || !length || length > AC97_BUFFER_BYTES ||
        !format || format->format != AUDIO_FORMAT_S16_STEREO ||
        format->channels != 2 || !format->sample_rate) {
        SerialPutString("[AC97] invalid PCM request\r\n");
        return IO_STATUS_INVALID_PARAMETER;
    }

    SerialPutString("[AC97] play bytes=");
    SerialPrintDec(length);
    SerialPutString(" rate=");
    SerialPrintDec(format->sample_rate);
    SerialPutString("\r\n");

    status = ac97_stream_start(format);
    while (status == IO_STATUS_SUCCESS && offset < length) {
        uint32_t chunk = length - offset;
        if (chunk > AC97_STREAM_SLOT_BYTES) chunk = AC97_STREAM_SLOT_BYTES;
        chunk &= ~3U;
        status = ac97_stream_write((const uint8_t *)data + offset, chunk);
        offset += chunk;
    }
    if (status == IO_STATUS_SUCCESS) status = ac97_stream_stop();
    return status;
}

static int ac97_control(IO_DEVICE_OBJECT *device, IO_REQUEST *request) {
    (void)device;

    if (request->parameters.device_control.code == AUDIO_IOCTL_GET_CAPS) {
        AUDIO_CAPS *caps = (AUDIO_CAPS *)request->buffer;
        if (!caps || request->length < sizeof(*caps))
            return IO_STATUS_INVALID_PARAMETER;

        caps->format.sample_rate = 48000;
        caps->format.channels    = 2;
        caps->format.format      = AUDIO_FORMAT_S16_STEREO;
        caps->max_buffer_size    = AC97_BUFFER_BYTES;
        caps->flags              = AUDIO_CAP_PLAYBACK;

        request->io_status.information = sizeof(*caps);
        return IO_STATUS_SUCCESS;
    }

    if (request->parameters.device_control.code == AUDIO_IOCTL_PLAY_PCM) {
        const AUDIO_PCM_PACKET *packet =
            (const AUDIO_PCM_PACKET *)request->parameters.device_control.input_buffer;

        if (!packet ||
            request->parameters.device_control.input_length < sizeof(*packet) ||
            packet->sample_bytes >
                request->parameters.device_control.input_length - sizeof(*packet)) {
            return IO_STATUS_INVALID_PARAMETER;
        }

        return ac97_play((const uint8_t *)packet + sizeof(*packet),
                         packet->sample_bytes, &packet->format);
    }

    if (request->parameters.device_control.code == AUDIO_IOCTL_STREAM_START) {
        const AUDIO_FORMAT *format =
            (const AUDIO_FORMAT *)request->parameters.device_control.input_buffer;
        if (!format || request->parameters.device_control.input_length < sizeof(*format))
            return IO_STATUS_INVALID_PARAMETER;
        return ac97_stream_start(format);
    }

    if (request->parameters.device_control.code == AUDIO_IOCTL_STREAM_WRITE) {
        const AUDIO_PCM_PACKET *packet =
            (const AUDIO_PCM_PACKET *)request->parameters.device_control.input_buffer;
        if (!packet || request->parameters.device_control.input_length < sizeof(*packet) ||
            packet->sample_bytes > request->parameters.device_control.input_length - sizeof(*packet))
            return IO_STATUS_INVALID_PARAMETER;
        return ac97_stream_write((const uint8_t *)packet + sizeof(*packet),
                                 packet->sample_bytes);
    }

    if (request->parameters.device_control.code == AUDIO_IOCTL_STREAM_STOP)
        return ac97_stream_stop();

    return IO_STATUS_NOT_SUPPORTED;
}

int Ac97Initialize(IO_DRIVER_OBJECT *driver) {
    if (!driver) return 0;

    SerialPutString("[AC97] scanning PCI (I/O)\r\n");

    for (uint16_t bus = 0; bus < 256; ++bus)
        for (uint8_t slot = 0; slot < 32; ++slot)
            for (uint8_t fn = 0; fn < 8; ++fn) {
                uint32_t id = PciConfigRead32((uint8_t)bus, slot, fn, 0);
                if (id == 0xffffffffU)
                    continue;

                uint32_t class = PciConfigRead32((uint8_t)bus, slot, fn, 8);
                if ((class >> 8) != AC97_CLASS)
                    continue;

                uint32_t mixer_bar = PciConfigRead32((uint8_t)bus, slot, fn, 0x10);
                uint32_t bm_bar    = PciConfigRead32((uint8_t)bus, slot, fn, 0x14);

                if (!(mixer_bar & 1) || !(bm_bar & 1))
                    continue;

                SerialPutString("[AC97] PCI controller ");
                SerialPrintHex(id);
                SerialPutString(" mixer-bar=");
                SerialPrintHex(mixer_bar);
                SerialPutString(" bm-bar=");
                SerialPrintHex(bm_bar);
                SerialPutString("\r\n");

                uint32_t cmd = PciConfigRead32((uint8_t)bus, slot, fn, 4);
                cmd |= 0x0005U;
                PciConfigWrite32((uint8_t)bus, slot, fn, 4, cmd);

                ac97.mixer  = (uint16_t)(mixer_bar & 0xfffcU);
                ac97.bus    = (uint16_t)(bm_bar & 0xfffcU);
                ac97.bdl    = (uint8_t *)VmmAllocatePages(1);
                ac97.buffer = (uint8_t *)VmmAllocatePages((AC97_BUFFER_BYTES + 4095U) / 4096U);

                if (!ac97.bdl || !ac97.buffer)
                    return 0;

                memset(ac97.bdl, 0, 4096);
                memset(ac97.buffer, 0, AC97_BUFFER_BYTES);

                mixer_write(0x00, 0);
                mixer_write(AC97_NAM_MASTER, 0x0000);
                mixer_write(AC97_NAM_PCM, 0x0000);

                mixer_write(AC97_NAM_EXTSTAT, 0x0009);
                mixer_write(AC97_NAM_PCM_RATE, 48000);

                ac97_log_hex("[AC97] master=", mixer_read(AC97_NAM_MASTER));
                ac97_log_hex("[AC97] pcm=", mixer_read(AC97_NAM_PCM));
                ac97_log_hex("[AC97] ext-status=", mixer_read(AC97_NAM_EXTSTAT));

                if (!IoCreateDevice(driver, "AudioAC97", 0))
                    return 0;

                driver->major_function[IO_MJ_DEVICE_CONTROL] = ac97_control;
                SerialPutString("[AC97] Playback device ready (I/O, multi‑BDL)\r\n");
                return 1;
            }

    return 0;
}

int DriverEntry(IO_DRIVER_OBJECT *driver, void *context) {
    (void)context;
    return Ac97Initialize(driver);
}
