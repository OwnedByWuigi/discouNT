#include "sound.h"
#include "../../drivers/audio/audio.h"

extern int AudioServiceGetCaps(AUDIO_CAPS *caps);
extern int AudioServiceStreamStart(const AUDIO_FORMAT *format);
extern int AudioServiceStreamWrite(const AUDIO_FORMAT *format, const void *samples, uint32_t bytes);
extern int AudioServiceStreamStop(void);

static AUDIO_FORMAT sound_format;
static HWAVEOUT sound_handle;

int waveOutOpen(HWAVEOUT *handle, const SOUND_WAVEFORMATEX *format) {
    AUDIO_CAPS caps;
    if (!handle || !format || format->wFormatTag != SOUND_WAVE_FORMAT_PCM ||
        format->nChannels != 2 || format->wBitsPerSample != 16 ||
        format->nBlockAlign != 4 || !format->nSamplesPerSec || sound_handle)
        return 1;
    if (AudioServiceGetCaps(&caps) != 0 || !(caps.flags & AUDIO_CAP_PLAYBACK)) return 1;
    (void)caps;
    sound_format.sample_rate = format->nSamplesPerSec;
    sound_format.channels = 2;
    sound_format.format = AUDIO_FORMAT_S16_STEREO;
    if (AudioServiceStreamStart(&sound_format) != 0) return 1;
    sound_handle = 1;
    *handle = sound_handle;
    return 0;
}

int waveOutWrite(HWAVEOUT handle, const void *samples, uint32_t bytes) {
    const uint8_t *cursor = (const uint8_t *)samples;
    if (!handle || handle != sound_handle || !samples || !bytes || (bytes & 3)) return 1;
    while (bytes) {
        uint32_t chunk = bytes > 4096 ? 4096 : bytes;
        if (AudioServiceStreamWrite(&sound_format, cursor, chunk) != 0) return 1;
        cursor += chunk;
        bytes -= chunk;
    }
    return 0;
}

int waveOutClose(HWAVEOUT handle) {
    if (!handle || handle != sound_handle) return 1;
    if (AudioServiceStreamStop() != 0) return 1;
    sound_handle = 0;
    sound_format.sample_rate = 0;
    return 0;
}
