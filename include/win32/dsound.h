#ifndef DISCOUNT_DSOUND_H
#define DISCOUNT_DSOUND_H
#include "windows.h"
typedef BOOL (CALLBACK *LPDSENUMCALLBACKW)(LPGUID,LPCWSTR,LPCWSTR,LPVOID);
HRESULT WINAPI DirectSoundEnumerateW(LPDSENUMCALLBACKW callback, LPVOID context);
HRESULT WINAPI DirectSoundCaptureEnumerateW(LPDSENUMCALLBACKW callback, LPVOID context);
HRESULT WINAPI DiscountAudioPlay(const void *samples, DWORD bytes, DWORD rate);
#endif
