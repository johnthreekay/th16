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
