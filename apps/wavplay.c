#include <stdint.h>
#include "audio/sound.h"

extern int CdfsReadFile(const char *path, uint8_t **buffer, uint32_t *size);
extern void kfree(void *memory);
extern void *kmalloc(uint32_t size);
extern void SerialPutString(const char *text);
extern void SerialPrintDec(uint32_t value);
extern void SerialPrintHex(uint32_t value);

static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int tag(const uint8_t *p, char a, char b, char c, char d) { return p[0] == a && p[1] == b && p[2] == c && p[3] == d; }

static const int ima_step[] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,
    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,
    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,
    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,
    3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,
    16818,18500,20350,22385,24623,27086,29794,32767
};
static const int ima_index[] = {-1,-1,-1,-1,2,4,6,8};

static int play_pcm(HWAVEOUT wave, const int16_t *samples, uint32_t frames) {
    uint32_t done = 0;
    while (done < frames) {
        uint32_t count = frames - done;
        if (count > 32768) count = 32768;
        if (waveOutWrite(wave, samples + done * 2, count * 4) != 0) return 0;
        done += count;
    }
    return 1;
}

static int16_t pcm_sample(const uint8_t *p, uint16_t bits, uint16_t encoding) {
    int32_t value;
    if (encoding == 1 || encoding == 0xfffe) {
        if (bits == 8)  return (int16_t)(((int32_t)p[0] - 128) << 8);
        if (bits == 16) return (int16_t)le16(p);
        if (bits == 24) {
            value = (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16);
            if (value & 0x800000) value |= (int32_t)0xff000000;
            return (int16_t)(value >> 8);
        }
        if (bits == 32) return (int16_t)((int32_t)le32(p) >> 16);
    }
    return 0;
}

static int play_pcm_bytes(const uint8_t *data, uint32_t bytes, uint16_t block_align,
                          uint16_t channels, uint16_t bits, uint16_t encoding, uint32_t rate) {
    uint32_t frame, frames, done;
    int16_t *pcm;
    SOUND_WAVEFORMATEX format = {SOUND_WAVE_FORMAT_PCM, 2, rate, rate * 4, 4, 16, 0};
    HWAVEOUT wave = 0;

    if (!data || !bytes || !block_align || !channels || channels > 2 ||
        (encoding != 1 && encoding != 0xfffe) ||
        (bits != 8 && bits != 16 && bits != 24 && bits != 32) ||
        block_align < (uint16_t)(channels * ((bits + 7) / 8))) return 0;
    if (waveOutOpen(&wave, &format) != 0) return 0;

    frames = bytes / block_align;
    pcm = (int16_t *)kmalloc(32768U * 4);
    if (!pcm) return 0;

    SerialPutString("[WAVPLAY] pcm-frames=");
    SerialPrintDec(frames);
    SerialPutString(" bytes=");
    SerialPrintDec(bytes);
    SerialPutString("\r\n");

    for (done = 0; done < frames;) {
        uint32_t count = frames - done;
        if (count > 32768) count = 32768;
        for (frame = 0; frame < count; ++frame) {
            const uint8_t *src = data + (done + frame) * block_align;
            int16_t left  = pcm_sample(src, bits, encoding);
            int16_t right = channels == 2 ? pcm_sample(src + (bits + 7) / 8, bits, encoding) : left;
            pcm[frame * 2]     = left;
            pcm[frame * 2 + 1] = right;
        }
        if (done < 40U * 32768U) {
            SerialPutString("[WAVPLAY] block=");
            SerialPrintDec(done / 32768U);
            SerialPutString(" pcm-first=");
            SerialPrintHex((uint16_t)pcm[0]);
            SerialPutString(" pcm-last=");
            SerialPrintHex((uint16_t)pcm[count * 2 - 2]);
            SerialPutString("\r\n");
        }
        if (!play_pcm(wave, pcm, count)) { waveOutClose(wave); kfree(pcm); return 0; }
        done += count;
    }

    waveOutClose(wave);
    kfree(pcm);
    return 1;
}

static int play_ima(const uint8_t *data, uint32_t bytes, uint16_t block_align, uint32_t rate) {
    uint32_t block;
    SOUND_WAVEFORMATEX format = {SOUND_WAVE_FORMAT_PCM, 2, rate, rate * 4, 4, 16, 0};
    HWAVEOUT wave = 0;

    if (bytes < block_align || block_align < 4 || bytes % block_align) return 0;
    if (waveOutOpen(&wave, &format) != 0) return 0;

    uint32_t max_frames_per_block = 1 + (block_align - 4) * 2;
    int16_t *pcm = (int16_t *)kmalloc(max_frames_per_block * 4);
    if (!pcm) { waveOutClose(wave); return 0; }

    SerialPutString("[WAVPLAY] ima-frames-est=");
    SerialPrintDec((bytes / block_align) * max_frames_per_block);
    SerialPutString("\r\n");

    for (block = 0; block < bytes; block += block_align) {
        uint32_t frames = 0;
        uint32_t block_nibbles = (block_align - 4) * 2;
        int predictor = (int16_t)le16(data + block);
        int index     = data[block + 2];

        if (index > 88) { kfree(pcm); return 0; }

        pcm[frames * 2]     = (int16_t)predictor;
        pcm[frames * 2 + 1] = (int16_t)predictor;
        frames++;

        for (uint32_t nibble = 0; nibble < block_nibbles; ++nibble) {
            uint8_t code = (data[block + 4 + (nibble >> 1)] >> ((nibble & 1) * 4)) & 0xf;
            int step  = ima_step[index];
            int delta = step >> 3;

            if (code & 1) delta += step >> 2;
            if (code & 2) delta += step >> 1;
            if (code & 4) delta += step;

            predictor += (code & 8) ? -delta : delta;
            if (predictor > 32767)  predictor = 32767;
            if (predictor < -32768) predictor = -32768;

            index += ima_index[code & 7];
            if (index < 0)  index = 0;
            if (index > 88) index = 88;

            pcm[frames * 2]     = (int16_t)predictor;
            pcm[frames * 2 + 1] = (int16_t)predictor;
            frames++;
        }

        if (frames) {
            if (block == 0) {
                SerialPutString("[WAVPLAY] pcm-first=");
                SerialPrintHex((uint16_t)pcm[0]);
                SerialPutString(" pcm-next=");
                SerialPrintHex((uint16_t)pcm[2]);
                SerialPutString("\r\n");
            }
            if (waveOutWrite(wave, pcm, frames * 4) != 0) {
                waveOutClose(wave);
                kfree(pcm);
                return 0;
            }
        }
    }

    waveOutClose(wave);
    kfree(pcm);
    return 1;
}

int main(int argc, char **argv) {
    uint8_t *file, *data = 0;
    uint32_t size, offset = 12, data_size = 0, chunk_size;
    SOUND_WAVEFORMATEX format = {0};
    uint16_t encoding = 0, block_align = 0, bits = 0, channels = 0;
    int have_format = 0;

    if (argc != 2 || !argv[1] || !CdfsReadFile(argv[1], &file, &size) || size < 12) return 1;

    SerialPutString("[WAVPLAY] file bytes=");
    SerialPrintDec(size);
    SerialPutString("\r\n");

    if (!tag(file, 'R', 'I', 'F', 'F') || !tag(file + 8, 'W', 'A', 'V', 'E')) {
        kfree(file);
        return 2;
    }

    while (offset + 8 <= size) {
        chunk_size = le32(file + offset + 4);
        uint8_t *chunk = file + offset;

        if (chunk_size > size - offset - 8) break;

        if (tag(chunk, 'f','m','t',' ')) {
            if (chunk_size < 16) { kfree(file); return 3; }

            encoding           = le16(chunk + 8);
            channels           = le16(chunk + 10);
            format.nSamplesPerSec = le32(chunk + 12);
            block_align        = le16(chunk + 20);
            bits               = le16(chunk + 22);

            if ((encoding == 1 || encoding == 0xfffe) &&
                channels >= 1 && channels <= 2 &&
                (bits == 8 || bits == 16 || bits == 24 || bits == 32)) {
                have_format = 1;
            } else if (encoding == 0x11 && channels == 1 && bits == 4) {
                have_format = 2;
            }
        } else if (tag(chunk, 'd','a','t','a')) {
            data      = chunk + 8;
            data_size = chunk_size;
        }

        offset += 8 + chunk_size + (chunk_size & 1);
    }

    if (!have_format || !data || !data_size || !format.nSamplesPerSec) {
        SerialPutString("[WAVPLAY] missing fmt/data or bad format\r\n");
        kfree(file);
        return 4;
    }

    SerialPutString("[WAVPLAY] encoding=");
    SerialPrintDec(encoding);
    SerialPutString(" data=");
    SerialPrintDec(data_size);
    SerialPutString(" rate=");
    SerialPrintDec(format.nSamplesPerSec);
    SerialPutString("\r\n");

    if (have_format == 2) {
        int ok = play_ima(data, data_size, block_align, format.nSamplesPerSec);
        kfree(file);
        return ok ? 0 : 5;
    }

    if (!play_pcm_bytes(data, data_size, block_align, channels, bits, encoding, format.nSamplesPerSec)) {
        kfree(file);
        return 5;
    }

    kfree(file);
    return 0;
}
