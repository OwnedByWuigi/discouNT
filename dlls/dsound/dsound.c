#include "../directx_stub.h"
#include "../../include/win32/dsound.h"
#include "../../kernel/audio/audio_service.h"

static const GUID dsound_default_guid =
    {0x9e2d2f9a, 0x5a2a, 0x4d11, {0x9a, 0x1d, 0x41, 0x7c, 0x41, 0x44, 0x49, 0x4f}};
static const WCHAR dsound_name[] = L"discouNT AC97 Audio";
static const WCHAR dsound_module[] = L"AC97.SYS";

HRESULT WINAPI DirectSoundCreate(GUID *guid, void **object, void *outer) {
    (void)guid; (void)outer; return dx_not_implemented(object);
}

HRESULT WINAPI DirectSoundCreate8(GUID *guid, void **object, void *outer) {
    (void)guid; (void)outer; return dx_not_implemented(object);
}

HRESULT WINAPI DirectSoundCaptureCreate(GUID *guid, void **object, void *outer) {
    (void)guid; (void)outer; return dx_not_implemented(object);
}

HRESULT WINAPI DirectSoundEnumerateA(void *callback, void *context) {
    typedef BOOL (CALLBACK *ENUMCALLBACKA)(LPGUID, LPCSTR, LPCSTR, LPVOID);
    AUDIO_CAPS caps;
    if (!callback) return (HRESULT)0x80004003UL;
    if (AudioServiceGetCaps(&caps) != 0) return S_OK;
    return ((ENUMCALLBACKA)callback)((LPGUID)&dsound_default_guid,
        "discouNT AC97 Audio", "AC97.SYS", context) ? S_OK : (HRESULT)1;
}

HRESULT WINAPI DirectSoundEnumerateW(LPDSENUMCALLBACKW enum_callback, LPVOID context) {
    AUDIO_CAPS caps;
    if (!enum_callback) return (HRESULT)0x80004003UL;
    if (AudioServiceGetCaps(&caps) != 0) return S_OK;
    return enum_callback((LPGUID)&dsound_default_guid, dsound_name, dsound_module, context)
        ? S_OK : (HRESULT)1;
}

HRESULT WINAPI DirectSoundCaptureEnumerateW(LPDSENUMCALLBACKW callback, LPVOID context) {
    (void)callback; (void)context; return S_OK;
}

/* Small bridge used by multimedia clients that already have PCM data. */
HRESULT WINAPI DiscountAudioPlay(const void *samples, DWORD bytes, DWORD rate) {
    AUDIO_FORMAT format = {rate, 2, AUDIO_FORMAT_S16_STEREO};
    return AudioServicePlay(&format, samples, bytes) == 0 ? S_OK : (HRESULT)0x80004005UL;
}

int WINAPI DllMain(void *module, DWORD reason, void *reserved) {
    return dx_dll_main(module, reason, reserved);
}
