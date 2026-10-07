// Stand-in callers for range F (0x4630f0-0x4748e0) of wave 3.
#include "../AnmManager.h"
#include "../SoundManager.h"

// Like AnmVm::run (0x45fb6b), which saves the script time on interrupts.
void harness_w3f_timer_copy(AnmVm *vm)
{
    vm->interrupt_return_time.set_from(vm->script_time);
}

// Like the code around 0x42ed08, which refreshes a texture from an image in
// memory.
i32 harness_w3f_reload_texture(AnmLoaded *anm, void *data, u32 size)
{
    return g_AnmManager->reload_texture(&anm->d3d[1], data, size, 0, 0, 0);
}

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
