// Placeholders compiled with /GL for functions other units own, where LTCG
// has to see a body (see src/placeholder/unit2.cpp). Each forwards to an
// opaque stub.
#include "../SoundManager.h"

int w3e_placeholder_sink(void *object, int value);

// STUB: TH16 0x470250
DECOMP_NOINLINE HRESULT CSoundManager::set_primary_buffer_format(DWORD channels, DWORD frequency, DWORD bits)
{
    return w3e_placeholder_sink(this, 0);
}

// The name, flags and notification count are unused here, as they are
// constant (folded) in the original.

// STUB: TH16 0x470320
DECOMP_NOINLINE HRESULT CSoundManager::create_streaming(BgmStream **out, const char *name, DWORD flags, GUID guid,
                                                        DWORD notify_count, DWORD notify_size, HANDLE event,
                                                        ThBgmFormat *format)
{
    return w3e_placeholder_sink(this, (int)out + notify_size + (int)event + (int)format + guid.Data1);
}

// STUB: TH16 0x470680
DECOMP_NOINLINE HRESULT CSoundManager::create_streaming_from_memory(BgmStream **out, u8 *data, i32 size,
                                                                    ThBgmFormat *format, DWORD flags, GUID guid,
                                                                    DWORD notify_count, DWORD notify_size,
                                                                    HANDLE event)
{
    return w3e_placeholder_sink(this, (int)out + (int)data + size + (int)format + guid.Data1 + notify_size + (int)event);
}
