#include <string.h>

#include "ScreenEffect.h"

#include <d3dx9math.h>

#include "AnmManager.h"
#include "GameThread.h"
#include "Rng.h"
#include "Supervisor.h"

extern i32 g_cancel_screen_effects;

struct ZunRect
{
    f32 left;
    f32 top;
    f32 right;
    f32 bottom;
};

// D3DFVF_XYZRHW | D3DFVF_DIFFUSE
struct ScreenEffectVertex
{
    D3DXVECTOR3 position;
    f32 rhw;
    D3DCOLOR diffuse;
};

// Fills a rectangle (in window pixels) with one color, bypassing the
// sprite code.
// FUNCTION: TH16 0x45c690
HARNESS_CALLED void screen_effect_draw_rect(ZunRect *rect, D3DCOLOR color)
{
    ScreenEffectVertex vertices[4];

    g_AnmManager->flush_sprites();
    vertices[0].position = D3DXVECTOR3(rect->left, rect->top, 0.0f);
    vertices[1].position = D3DXVECTOR3(rect->right, rect->top, 0.0f);
    vertices[2].position = D3DXVECTOR3(rect->left, rect->bottom, 0.0f);
    vertices[3].position = D3DXVECTOR3(rect->right, rect->bottom, 0.0f);
    vertices[0].rhw = vertices[1].rhw = vertices[2].rhw = vertices[3].rhw = 1.0f;
    vertices[0].diffuse = vertices[1].diffuse = vertices[2].diffuse = vertices[3].diffuse = color;
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_Supervisor.d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_Supervisor.d3d_device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_Supervisor.d3d_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0]));
    g_AnmManager->render_cache_184fbb5 = 0xff;
    g_AnmManager->last_vertex_setup = ANM_VERTEX_SETUP_NONE;
    g_AnmManager->last_texture_matrix_sprite = 0;
    g_AnmManager->last_texture_id = -1;
    g_AnmManager->last_blend_mode = ANM_BLEND_FORCE_RESET;
    g_AnmManager->render_cache_184fbb7 = 0xff;
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_Supervisor.d3d_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
}

// FUNCTION: TH16 0x45c630
i32 __fastcall ScreenEffect::on_tick_fade_in(ScreenEffect *self)
{
    if (g_cancel_screen_effects != 0)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    if (self->arg_18 != 0)
    {
        i32 alpha = 255.0f - self->timer.current_f * 255.0f / self->arg_18;
        self->alpha = alpha < 0 ? 0 : alpha;
    }
    if (self->timer.current >= self->arg_18)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    self->timer++;
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45c860
i32 __fastcall ScreenEffect::on_draw_viewport(ScreenEffect *self)
{
    ZunRect rect = {0.0f, 0.0f, (f32)g_resolution_x, (f32)g_resolution_y};

    g_AnmManager->flush_sprites();
    g_Supervisor.full_window_viewport.X = 0;
    g_Supervisor.full_window_viewport.Y = 0;
    g_Supervisor.full_window_viewport.Width = g_resolution_x;
    g_Supervisor.full_window_viewport.Height = g_resolution_y;
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.full_window_viewport);
    screen_effect_draw_rect(&rect, (self->alpha << 24) | self->arg_1c);
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45c900
i32 __fastcall ScreenEffect::on_tick_fade_out(ScreenEffect *self)
{
    if (g_cancel_screen_effects != 0)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    if (self->arg_18 != 0)
    {
        if (self->timer.current < self->arg_18)
        {
            self->alpha = self->timer.current_f * 255.0f / self->arg_18;
        }
        else
        {
            self->alpha = 255;
        }
        if (self->alpha < 0)
        {
            self->alpha = 0;
        }
    }
    if (self->timer.current >= self->arg_18 + 2)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    if (g_GameThread == NULL || !(g_GameThread->flags.flag_0 | g_GameThread->flags.loading))
    {
        self->timer++;
    }
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45c990
i32 __fastcall ScreenEffect::on_draw_screen(ScreenEffect *self)
{
    ZunRect rect = {0.0f, 0.0f, (f32)g_resolution_x, (f32)g_resolution_y};

    screen_effect_draw_rect(&rect, (self->alpha << 24) | self->arg_1c);
    return UPDATE_FUNC_CONTINUE;
}

// TODO: the inlined tick's slow path loads current_f into xmm0 and adds the
// speed; the original adds current_f from memory into the speed's xmm1
// (tick_split, tick_in_place and either operand order give ours).
// FUNCTION: TH16 0x45c9e0
i32 __fastcall ScreenEffect::on_tick_flash(ScreenEffect *self)
{
    if (self->unk_28 == 0)
    {
        if (self->arg_18 != 0 && self->timer.current <= self->arg_18)
        {
            self->alpha = self->timer.current_f * 128.0f / self->arg_18;
        }
    }
    else if (self->timer.current <= 8)
    {
        self->alpha = 128 - (i32)(self->timer.current_f * 128.0f / 8.0f);
    }
    else
    {
        return UPDATE_FUNC_CLEANUP;
    }
    self->timer.tick();
    return UPDATE_FUNC_CONTINUE;
}

// TODO: the inlined tick's slow path loads current_f into xmm0 and adds the
// speed; the original adds current_f from memory into the speed's xmm1
// (tick_split, tick_in_place and either operand order give ours).
// FUNCTION: TH16 0x45cac0
i32 __fastcall ScreenEffect::on_tick_hold(ScreenEffect *self)
{
    self->alpha = 255;
    if (self->timer.current >= self->arg_18)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    self->timer.tick();
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45cb50
i32 __fastcall ScreenEffect::on_draw_screen_2(ScreenEffect *self)
{
    ZunRect rect = {0.0f, 0.0f, (f32)g_resolution_x, (f32)g_resolution_y};

    screen_effect_draw_rect(&rect, (self->alpha << 24) | self->arg_1c);
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45cba0
i32 __fastcall ScreenEffect::on_draw_arcade(ScreenEffect *self)
{
    ZunRect rect = {128.0f, 16.0f, 512.0f, 464.0f};

    screen_effect_draw_rect(&rect, (self->alpha << 24) | self->arg_1c);
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45cbd0
i32 __fastcall ScreenEffect::on_tick_pulse(ScreenEffect *self)
{
    u32 start_alpha = (u32)self->arg_20 >> 24;
    if (g_cancel_screen_effects != 0)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    if (self->timer.current < self->arg_18)
    {
        self->alpha = start_alpha - (i32)(start_alpha * self->timer.current_f / self->arg_18);
        if (self->alpha < 0)
        {
            self->alpha = 0;
        }
    }
    else
    {
        self->arg_1c--;
        self->alpha = 0;
        if (self->arg_1c <= 0)
        {
            return UPDATE_FUNC_CLEANUP;
        }
        self->timer.set_value(0);
    }
    self->timer.tick_split();
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45ccc0
i32 __fastcall ScreenEffect::on_draw_arcade_2(ScreenEffect *self)
{
    ZunRect rect = {(f32)g_early_arcade_offset_x, (f32)g_early_arcade_offset_y,
                    (f32)g_arcade_width + (f32)g_early_arcade_offset_x,
                    (f32)g_arcade_height + (f32)g_early_arcade_offset_y};
    screen_effect_draw_rect(&rect, (self->arg_20 & 0xffffff) | (self->alpha << 24));
    return UPDATE_FUNC_CONTINUE;
}

// TODO: the inlined tick's slow path loads current_f into xmm0 and adds the
// speed (the original adds current_f from memory into the speed's xmm1), and
// push esi comes before the current store.
// FUNCTION: TH16 0x45cd30
i32 __fastcall ScreenEffect::on_tick_shake(ScreenEffect *self)
{
    if (g_cancel_screen_effects != 0)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    self->timer.tick_split();
    if (self->timer.current >= self->arg_18)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    f32 amount = (f32)(self->arg_20 - self->arg_1c) * self->timer.current_f / self->arg_18 + self->arg_1c;
    switch (g_replay_unsafe_rng.rand_u32() % 3)
    {
    case 0:
        g_Supervisor.cameras[3].shake_offset.x = 0.0f;
        g_Supervisor.cameras[1].shake_offset.x = 0.0f;
        break;
    case 1:
        g_Supervisor.cameras[3].shake_offset.x = amount;
        g_Supervisor.cameras[1].shake_offset.x = amount * g_screen_coord_scale;
        break;
    case 2:
        g_Supervisor.cameras[3].shake_offset.x = -amount;
        // Not negated, unlike every other case.
        g_Supervisor.cameras[1].shake_offset.x = amount * g_screen_coord_scale;
        break;
    }
    switch (g_replay_unsafe_rng.rand_u32() % 3)
    {
    case 0:
        g_Supervisor.cameras[3].shake_offset.y = 0.0f;
        g_Supervisor.cameras[1].shake_offset.y = 0.0f;
        break;
    case 1:
        g_Supervisor.cameras[3].shake_offset.y = amount;
        g_Supervisor.cameras[1].shake_offset.y = amount * g_screen_coord_scale;
        break;
    case 2:
        g_Supervisor.cameras[3].shake_offset.y = -amount;
        g_Supervisor.cameras[1].shake_offset.y = -(amount * g_screen_coord_scale);
        break;
    }
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45cf10
i32 __fastcall ScreenEffect::on_tick_shake_with_ramp(ScreenEffect *self)
{
    f32 t;

    if (g_cancel_screen_effects != 0)
    {
        return UPDATE_FUNC_CLEANUP;
    }
    if (g_GameThread == NULL || (g_GameThread->flags.flag_0 | g_GameThread->flags.loading) ||
        g_GameThread->flags.flag_1 || g_GameThread->flags.in_menu || g_GameThread->flags.flag_5 ||
        g_GameThread->flags.flag_6)
    {
        return UPDATE_FUNC_CONTINUE;
    }
    self->timer++;
    if (self->timer.current < self->arg_1c)
    {
        t = self->timer.current_f / self->arg_1c;
    }
    else if (self->timer.current < self->arg_20 + self->arg_1c)
    {
        t = 1.0f;
    }
    else if (self->timer.current < self->arg_24 + self->arg_20 + self->arg_1c)
    {
        t = ((f32)(u32)(self->arg_24 + self->arg_20 + self->arg_1c) - self->timer.current_f) / (f32)(u32)self->arg_24;
    }
    else
    {
        return UPDATE_FUNC_CLEANUP;
    }
    f32 amount = self->arg_18 * t;
    switch (g_replay_safe_rng.rand_u32() % 3)
    {
    case 0:
        g_Supervisor.cameras[3].shake_offset.x = 0.0f;
        g_Supervisor.cameras[1].shake_offset.x = 0.0f;
        break;
    case 1:
        g_Supervisor.cameras[3].shake_offset.x = amount;
        g_Supervisor.cameras[1].shake_offset.x = amount * g_screen_coord_scale;
        break;
    case 2:
        g_Supervisor.cameras[3].shake_offset.x = -amount;
        g_Supervisor.cameras[1].shake_offset.x = -(amount * g_screen_coord_scale);
        break;
    }
    switch (g_replay_safe_rng.rand_u32() % 3)
    {
    case 0:
        g_Supervisor.cameras[3].shake_offset.y = 0.0f;
        g_Supervisor.cameras[1].shake_offset.y = 0.0f;
        break;
    case 1:
        g_Supervisor.cameras[3].shake_offset.y = amount;
        g_Supervisor.cameras[1].shake_offset.y = amount * g_screen_coord_scale;
        break;
    case 2:
        g_Supervisor.cameras[3].shake_offset.y = -amount;
        g_Supervisor.cameras[1].shake_offset.y = -(amount * g_screen_coord_scale);
        break;
    }
    return UPDATE_FUNC_CONTINUE;
}

// FUNCTION: TH16 0x45d130
i32 __fastcall ScreenEffect::on_cleanup(ScreenEffect *self)
{
    delete self;
    return 0;
}

// FUNCTION: TH16 0x45d150
DECOMP_NOINLINE ScreenEffect *LTCG_FASTCALL ScreenEffect::create(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24,
                                                i32 draw_priority)
{
    ScreenEffect *effect = new ScreenEffect;
    memset(effect, 0, sizeof(ScreenEffect));
    effect->flags |= 2;
    effect->initialize(mode, arg_18, arg_1c, arg_20, arg_24, draw_priority);
    return effect;
}

// FUNCTION: TH16 0x45d1a0
void ScreenEffect::initialize(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24, i32 draw_priority)
{
    initialize_inline(mode, arg_18, arg_1c, arg_20, arg_24, draw_priority);
}

// A ScreenEffect nothing uses besides its static constructor and
// destructor.
// GLOBAL: TH16 0x4d9dd0
ScreenEffect g_screen_effect = ScreenEffect("initialize ScreenInf\n");
// SYNTHETIC: TH16 0x401170
// ??__Eg_screen_effect@@YAXXZ
// SYNTHETIC: TH16 0x48acd0
// ??__Fg_screen_effect@@YAXXZ

// FUNCTION: TH16 0x45d360
ScreenEffect::~ScreenEffect()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
}

// The static constructor at 0x401170 logs "initialize ScreenInf" through it.
// FUNCTION: TH16 0x45d410
void screen_debug_log(const char *fmt, ...)
{
}
