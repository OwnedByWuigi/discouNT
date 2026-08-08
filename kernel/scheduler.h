// kernel/scheduler.h
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "object.h"

void KiInitScheduler(void);
void KiStartScheduler(void);
void KeStartThread(THREAD *thread, void (*entry)(void));
void irq0_handler(void);

extern THREAD *KiCurrentThread;

#endif