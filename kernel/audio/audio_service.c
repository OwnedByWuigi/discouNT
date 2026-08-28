#include "audio_service.h"
#include "io/io.h"
#include "core/util.h"
#include "mm/mm.h"
#include "serial.h"

/* Prefer VMware's HD Audio/ES1371 endpoints, then AC'97, then SB16. */
static const char *audio_endpoint(void) {
    AUDIO_CAPS caps;
    if (IoDeviceControl("AudioHDA", AUDIO_IOCTL_GET_CAPS, 0, 0, &caps, sizeof(caps), 0) == IO_STATUS_SUCCESS &&
        (caps.flags & AUDIO_CAP_PLAYBACK)) {
        SerialPutString("[AUDIO] selected HDA\r\n");
        return "AudioHDA";
    }
    if (IoDeviceControl("AudioES1371", AUDIO_IOCTL_GET_CAPS, 0, 0, &caps, sizeof(caps), 0) == IO_STATUS_SUCCESS &&
        (caps.flags & AUDIO_CAP_PLAYBACK)) {
        SerialPutString("[AUDIO] selected ES1371\r\n");
        return "AudioES1371";
    }
    if (IoDeviceControl("AudioAC97", AUDIO_IOCTL_GET_CAPS, 0, 0, &caps, sizeof(caps), 0) == IO_STATUS_SUCCESS &&
        (caps.flags & AUDIO_CAP_PLAYBACK)) {
        SerialPutString("[AUDIO] selected AC97\r\n");
        return "AudioAC97";
    }
    if (IoDeviceControl("AudioSB16", AUDIO_IOCTL_GET_CAPS, 0, 0, &caps, sizeof(caps), 0) == IO_STATUS_SUCCESS &&
        (caps.flags & AUDIO_CAP_PLAYBACK))
        { SerialPutString("[AUDIO] selected SB16\r\n"); return "AudioSB16"; }
    return 0;
}

static const char *audio_stream_endpoint;
static int audio_stream_fallback;
static int audio_stream_active;

int AudioServiceGetCaps(AUDIO_CAPS *caps) {
    const char *endpoint = audio_endpoint();
    if (!endpoint || !caps) return IO_STATUS_NOT_FOUND;
    return IoDeviceControl(endpoint, AUDIO_IOCTL_GET_CAPS, 0, 0, caps, sizeof(*caps), 0);
}

int AudioServicePlay(const AUDIO_FORMAT *format, const void *samples, uint32_t bytes) {
    uint8_t *packet;
    AUDIO_PCM_PACKET *header;
    const char *endpoint = audio_endpoint();
    int status;
    if (!endpoint) { SerialPutString("[AUDIO] no playback endpoint\r\n"); return IO_STATUS_NOT_FOUND; }
    if (!format || !samples || !bytes || bytes > (4U * 1024U * 1024U)) {
        SerialPutString("[AUDIO] invalid playback buffer\r\n"); return IO_STATUS_INVALID_PARAMETER;
    }
    packet = (uint8_t *)kmalloc(sizeof(*header) + bytes);
    if (!packet) return IO_STATUS_NO_MEMORY;
    header = (AUDIO_PCM_PACKET *)packet; header->format = *format; header->sample_bytes = bytes;
    memcpy(packet + sizeof(*header), samples, bytes);
    status = IoDeviceControl(endpoint, AUDIO_IOCTL_PLAY_PCM, packet, sizeof(*header) + bytes, 0, 0, 0);
    SerialPutString("[AUDIO] playback status="); SerialPrintDec((uint32_t)status); SerialPutString("\r\n");
    if (status != IO_STATUS_SUCCESS) { SerialPutString("[AUDIO] endpoint rejected playback\r\n"); }
    kfree(packet);
    return status;
}

int AudioServiceStreamStart(const AUDIO_FORMAT *format) {
    const char *endpoint;
    int status;
    if (!format || !format->sample_rate || !format->channels)
        return IO_STATUS_INVALID_PARAMETER;
    if (audio_stream_active) return IO_STATUS_INVALID_PARAMETER;
    endpoint = audio_endpoint();
    if (!endpoint) return IO_STATUS_NOT_FOUND;

    status = IoDeviceControl(endpoint, AUDIO_IOCTL_STREAM_START,
                             (void *)format, sizeof(*format), 0, 0, 0);
    audio_stream_endpoint = endpoint;
    audio_stream_fallback = (status == IO_STATUS_NOT_SUPPORTED);
    if (!audio_stream_fallback && status != IO_STATUS_SUCCESS) {
        audio_stream_endpoint = 0;
        return status;
    }
    audio_stream_active = 1;
    SerialPutString("[AUDIO] stream start ");
    SerialPutString(audio_stream_fallback ? "fallback\r\n" : "hardware\r\n");
    return IO_STATUS_SUCCESS;
}

int AudioServiceStreamWrite(const AUDIO_FORMAT *format, const void *samples, uint32_t bytes) {
    uint8_t *packet;
    AUDIO_PCM_PACKET *header;
    int status;
    if (!audio_stream_active || !format || !samples || !bytes || bytes > 131072)
        return IO_STATUS_INVALID_PARAMETER;
    if (audio_stream_fallback)
        return AudioServicePlay(format, samples, bytes);

    packet = (uint8_t *)kmalloc(sizeof(*header) + bytes);
    if (!packet) return IO_STATUS_NO_MEMORY;
    header = (AUDIO_PCM_PACKET *)packet;
    header->format = *format;
    header->sample_bytes = bytes;
    memcpy(packet + sizeof(*header), samples, bytes);
    status = IoDeviceControl(audio_stream_endpoint, AUDIO_IOCTL_STREAM_WRITE,
                             packet, sizeof(*header) + bytes, 0, 0, 0);
    kfree(packet);
    return status;
}

int AudioServiceStreamStop(void) {
    int status = IO_STATUS_SUCCESS;
    if (!audio_stream_active) return IO_STATUS_INVALID_PARAMETER;
    if (!audio_stream_fallback)
        status = IoDeviceControl(audio_stream_endpoint, AUDIO_IOCTL_STREAM_STOP,
                                 0, 0, 0, 0, 0);
    audio_stream_endpoint = 0;
    audio_stream_fallback = 0;
    audio_stream_active = 0;
    return status;
}
