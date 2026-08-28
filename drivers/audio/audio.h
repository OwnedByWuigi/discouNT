#ifndef DISCOUNT_AUDIO_H
#define DISCOUNT_AUDIO_H

#include <stdint.h>

#define AUDIO_IOCTL_GET_CAPS 0x41554401U
#define AUDIO_IOCTL_PLAY_PCM 0x41554402U
#define AUDIO_IOCTL_STREAM_START 0x41554403U
#define AUDIO_IOCTL_STREAM_WRITE 0x41554404U
#define AUDIO_IOCTL_STREAM_STOP 0x41554405U

#define AUDIO_FORMAT_S16_STEREO 1U

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t format;
} AUDIO_FORMAT;

typedef struct {
    AUDIO_FORMAT format;
    uint32_t max_buffer_size;
    uint32_t flags;
} AUDIO_CAPS;

/* Input to AUDIO_IOCTL_PLAY_PCM.  Samples immediately follow the header. */
typedef struct {
    AUDIO_FORMAT format;
    uint32_t sample_bytes;
} AUDIO_PCM_PACKET;

#define AUDIO_CAP_PLAYBACK 0x1U

#endif
