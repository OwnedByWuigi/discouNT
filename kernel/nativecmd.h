#ifndef NATIVECMD_H
#define NATIVECMD_H

#include <stdint.h>
#include "keyboard.h"

void NativeCmdInit(void);
void NativeCmdHandleKeyEvent(const KEYBOARD_EVENT *event);

#endif
