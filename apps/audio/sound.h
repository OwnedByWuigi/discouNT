#ifndef DISCOUNT_SOUND_H
#define DISCOUNT_SOUND_H

#include <stdint.h>

/* Small WinMM-like playback API.  Applications never identify a hardware
   endpoint; the audio service chooses the installed device. */
#define SOUND_WAVE_FORMAT_PCM 1U
typedef uint32_t HWAVEOUT;

typedef struct {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
} SOUND_WAVEFORMATEX;

int waveOutOpen(HWAVEOUT *handle, const SOUND_WAVEFORMATEX *format);
int waveOutWrite(HWAVEOUT handle, const void *samples, uint32_t bytes);
int waveOutClose(HWAVEOUT handle);

#endif
