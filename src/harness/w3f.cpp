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
