/*
 * discouNT WDF primitive bindings.
 *
 * This is deliberately a platform implementation, not a compatibility
 * translation unit.  WDF framework code calls these WDM-shaped primitives;
 * the bodies bind them to discouNT's allocator and the machine's atomic
 * instructions.
 */
#include <stdint.h>
#include <stddef.h>
#include <ntddk.h>
#include "mm/mm.h"

PVOID ExAllocatePoolWithTag(POOL_TYPE PoolType, SIZE_T NumberOfBytes, ULONG Tag) {
    (void)PoolType;
    (void)Tag;
    return kmalloc((uint32_t)NumberOfBytes);
}

PVOID ExAllocatePool2(ULONG Flags, SIZE_T NumberOfBytes, ULONG Tag) {
    (void)Flags;
    return ExAllocatePoolWithTag(NonPagedPoolNx, NumberOfBytes, Tag);
}

VOID ExFreePool(PVOID P) { kfree(P); }
VOID ExFreePoolWithTag(PVOID P, ULONG Tag) { (void)Tag; kfree(P); }

VOID RtlZeroMemory(PVOID Destination, SIZE_T Length) {
    uint8_t *p = (uint8_t *)Destination;
    while (p && Length--) *p++ = 0;
}

VOID RtlCopyMemory(PVOID Destination, CONST PVOID Source, SIZE_T Length) {
    uint8_t *d = (uint8_t *)Destination;
    const uint8_t *s = (const uint8_t *)Source;
    while (d && s && Length--) *d++ = *s++;
}

LONG InterlockedIncrement(volatile LONG *Addend) {
    return __sync_add_and_fetch(Addend, 1);
}

LONG InterlockedDecrement(volatile LONG *Addend) {
    return __sync_sub_and_fetch(Addend, 1);
}

LONG InterlockedExchange(volatile LONG *Target, LONG Value) {
    return __sync_lock_test_and_set(Target, Value);
}

LONG InterlockedCompareExchange(volatile LONG *Destination, LONG Exchange, LONG Comperand) {
    return __sync_val_compare_and_swap(Destination, Comperand, Exchange);
}

VOID KeInitializeEvent(PKEVENT Event, int Type, BOOLEAN State) {
    if (!Event) return;
    Event->Type = Type;
    Event->SignalState = State ? 1 : 0;
}

LONG KeSetEvent(PKEVENT Event, LONG Increment, BOOLEAN Wait) {
    (void)Increment;
    (void)Wait;
    if (!Event) return 0;
    return InterlockedExchange((volatile LONG *)&Event->SignalState, 1);
}

LONG KeResetEvent(PKEVENT Event) {
    if (!Event) return 0;
    return InterlockedExchange((volatile LONG *)&Event->SignalState, 0);
}

LONG KeReadStateEvent(PKEVENT Event) {
    return Event ? Event->SignalState : 0;
}

VOID KeInitializeDpc(PKDPC Dpc, PVOID Routine, PVOID Context) {
    if (!Dpc) return;
    Dpc->DeferredRoutine = Routine;
    Dpc->DeferredContext = Context;
    Dpc->Inserted = 0;
}

VOID KeInitializeSpinLock(KSPIN_LOCK *SpinLock) {
    if (SpinLock) *SpinLock = 0;
}

VOID KeAcquireSpinLock(KSPIN_LOCK *SpinLock, KIRQL *OldIrql) {
    if (OldIrql) *OldIrql = 0;
    if (!SpinLock) return;
    while (__sync_lock_test_and_set(SpinLock, 1)) {
        __asm__ __volatile__("pause");
    }
}

VOID KeReleaseSpinLock(KSPIN_LOCK *SpinLock, KIRQL OldIrql) {
    (void)OldIrql;
    if (SpinLock) __sync_lock_release(SpinLock);
}

VOID KeAcquireSpinLockAtDpcLevel(KSPIN_LOCK *SpinLock) {
    KIRQL old_irql;
    KeAcquireSpinLock(SpinLock, &old_irql);
}

VOID KeReleaseSpinLockFromDpcLevel(KSPIN_LOCK *SpinLock) {
    KeReleaseSpinLock(SpinLock, 0);
}

VOID KeStallExecutionProcessor(ULONG Microseconds) {
    while (Microseconds--) __asm__ __volatile__("pause");
}
