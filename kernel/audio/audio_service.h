#ifndef DISCOUNT_AUDIO_SERVICE_H
#define DISCOUNT_AUDIO_SERVICE_H

#include <stdint.h>
#include "../../drivers/audio/audio.h"

int AudioServiceGetCaps(AUDIO_CAPS *caps);
int AudioServicePlay(const AUDIO_FORMAT *format, const void *samples, uint32_t bytes);
int AudioServiceStreamStart(const AUDIO_FORMAT *format);
int AudioServiceStreamWrite(const AUDIO_FORMAT *format, const void *samples, uint32_t bytes);
int AudioServiceStreamStop(void);

#endif
