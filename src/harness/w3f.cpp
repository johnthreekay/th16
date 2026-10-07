// Stand-in callers for range F (0x4630f0-0x4748e0) of wave 3.
#include "../SoundManager.h"

// Like SoundManager::initialize (0x45d510), which sets the primary buffer
// format once DirectSound is up.
void harness_w3f_set_primary_format()
{
    g_SoundManager.manager->SetPrimaryBufferFormat(2, 44100, 16);
}

// Like the music room code around 0x43f463, which shows the play time.
double harness_w3f_bgm_play_time()
{
    return ((CStreamingSound *)g_SoundManager.bgm_stream)->get_play_time();
}

// Like the callers around 0x42daec and 0x440b17, which seek the BGM.
void harness_w3f_bgm_seek(double seconds)
{
    ((CStreamingSound *)g_SoundManager.bgm_stream)->seek(seconds);
}

// Like the BGM start in SoundManager's sound thread (0x45db30 and 0x45dea0).
HRESULT harness_w3f_create_bgm_stream(ThBgmFormat *track, DWORD size, HANDLE event)
{
    return g_SoundManager.manager->CreateStreaming((CStreamingSound **)&g_SoundManager.bgm_stream, "thbgm.dat", 0,
                                                   GUID_NULL, 8, size, event, track);
}

HRESULT harness_w3f_create_bgm_stream_from_memory(BYTE *data, ULONG data_size, ThBgmFormat *track, DWORD size)
{
    return g_SoundManager.manager->CreateStreamingFromMemory((CStreamingSound **)&g_SoundManager.bgm_stream, data,
                                                             data_size, track, 0, GUID_NULL, 8, size,
                                                             g_SoundManager.bgm_event);
}
