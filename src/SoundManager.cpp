#include <dsound.h>

#include "SoundManager.h"

// FUNCTION: TH16 0x43dcf0
void SoundManager::tick_bgm_fade()
{
    BgmStream *bgm = g_SoundManager.bgm_stream;
    if (bgm == NULL)
    {
        return;
    }
    if (bgm->fade_mode == 1)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
            bgm->buffers[0]->Stop();
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * 5000 / bgm->fade_duration - 5000);
        }
    }
    bgm = g_SoundManager.bgm_stream;
    if (bgm->fade_mode == 2)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * -5000 / bgm->fade_duration);
        }
    }
    bgm = g_SoundManager.bgm_stream;
    if (bgm->fade_mode == 4)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * 1000 / bgm->fade_duration - 1000);
        }
    }
    bgm = g_SoundManager.bgm_stream;
    if (bgm->fade_mode == 3)
    {
        bgm->fade_time_left--;
        if (bgm->fade_time_left <= 0)
        {
            bgm->fade_mode = 0;
        }
        else
        {
            bgm->set_volume(bgm->fade_time_left * -1000 / bgm->fade_duration);
        }
    }
}

// FUNCTION: TH16 0x42e4d0
void SoundManager::pause_sounds()
{
    g_SoundManager.unk_1c = -1;
    for (i32 i = 0; i < 78; i++)
    {
        SoundBuffer *sound = &g_SoundManager.sound_buffers[i];
        sound->was_playing = 0;
        if (sound->buffer != NULL)
        {
            DWORD status;
            sound->buffer->GetStatus(&status);
            sound->was_playing = status & DSBSTATUS_PLAYING;
            sound->buffer->Stop();
        }
    }
}

// FUNCTION: TH16 0x440bd0
void SoundManager::resume_sounds()
{
    for (i32 i = 0; i < 78; i++)
    {
        SoundBuffer *sound = &g_SoundManager.sound_buffers[i];
        if (sound->buffer != NULL && sound->was_playing)
        {
            sound->buffer->Play(0, 0, sound->desc->play_flags);
        }
    }
}
