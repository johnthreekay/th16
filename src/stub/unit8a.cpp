// Placeholders for unit 8a functions that are not decompiled yet.
#include "../AnmManager.h"
#include "../Interp.h"
#include "../SoundManager.h"

// STUB: TH16 0x4718a0
HRESULT CWaveFile::open_bgm(ThBgmFormat *track, i32 unk)
{
    return 0;
}

// STUB: TH16 0x471270
HRESULT BgmStream::stop(i32 unk)
{
    return 0;
}

CStreamingSound::~CStreamingSound()
{
}

// STUB: TH16 0x406e10
D3DXVECTOR3 InterpFloat3::step()
{
    return current;
}

// STUB: TH16 0x464080
ZunAngle InterpAngle::step()
{
    return current;
}

// STUB: TH16 0x466f00
void AnmManager::render_sub_466f00(AnmVm *vm)
{
}

// STUB: TH16 0x465280
i32 AnmManager::render_sprite_2d(AnmVm *vm, i32 unk)
{
    return 0;
}

// STUB: TH16 0x465c40
void __stdcall AnmVm::write_sprite_corners__without_rot(AnmVm *vm, Float3 *a, Float3 *b, Float3 *c, Float3 *d)
{
}

// STUB: TH16 0x4660b0
void __stdcall AnmVm::write_sprite_corners__with_z_rot(AnmVm *vm, Float3 *a, Float3 *b, Float3 *c, Float3 *d)
{
}

// STUB: TH16 0x4714c0
HRESULT BgmStream::handle_wave_stream_notification(i32 unused)
{
    return 0;
}
