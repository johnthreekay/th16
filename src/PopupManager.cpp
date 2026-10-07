#include <string.h>

#include "AsciiManager.h"
#include "Player.h"
#include "PopupManager.h"
#include "Supervisor.h"

// GLOBAL: TH16 0x4a6f10
PopupManager *g_PopupManager;

// FUNCTION: TH16 0x449c70
PopupManager::PopupManager()
{
    memset(this, 0, sizeof(PopupManager));
    g_PopupManager = this;
    flags |= 2;
}

// FUNCTION: TH16 0x449cd0
int PopupManager::initialize()
{
    UpdateFunc *f;

    ascii_anm = g_AsciiManager->ascii_anm;

    f = g_UpdateFuncRegistry->create_func(on_tick_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x14);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_thunk);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x2f);
    on_draw_func = f;

    ascii_anm->init_vm_with_sprite(&vm, ASCII_SPRITE_POPUP_DIGITS);
    vm.flags_hi = (vm.flags_hi & ~(ANM_VM_ORIGIN_GAME | ANM_VM_RESOLUTION_HALF_SCALED_4)) | ANM_VM_ORIGIN_HUD |
                  ANM_VM_RESOLUTION_SCALED_3;
    return 0;
}

// FUNCTION: TH16 0x449d60
PopupManager::~PopupManager()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    g_PopupManager = NULL;
}

// FUNCTION: TH16 0x449e50
PopupManager *PopupManager::create()
{
    PopupManager *manager = new PopupManager();
    if (manager->initialize() != 0)
    {
        delete manager;
        return NULL;
    }
    return manager;
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x44a440
int __fastcall PopupManager::on_tick_thunk(void *arg)
{
    return ((PopupManager *)arg)->on_tick();
}

// The original's callback is a jmp to the member function, most likely the
// fastcall invoker of a capture-less lambda; a static thunk compiles the same.
// FUNCTION: TH16 0x44a450
int __fastcall PopupManager::on_draw_thunk(void *arg)
{
    return ((PopupManager *)arg)->on_draw();
}

// TODO: the original addresses each string through its timer's current
// field and assigns the hoisted float constants to other xmm registers.
// FUNCTION: TH16 0x449ea0
int PopupManager::on_tick()
{
    PopupString *str = strings;
    for (i32 i = 0; i < 13; i++, str++)
    {
        if (str->active)
        {
            str->pos.y -= str->unk_18 * g_game_speed;
            str->unk_18 *= 0.95f;
            str->time.tick();
            if (str->time.current > 60)
            {
                str->active = 0;
            }
        }
    }
    for (i32 i = 13; i < 18; i++, str++)
    {
        if (str->active)
        {
            str->time.tick();
            if (str->time.current > 60)
            {
                i32 alpha = (str->color >> 24) - 4;
                if (alpha <= 0)
                {
                    str->active = 0;
                }
                else
                {
                    str->color = (str->color & 0xffffff) | (alpha << 24);
                }
            }
        }
    }
    return 1;
}

// FUNCTION: TH16 0x44a460
HARNESS_CALLED void PopupManager::generate_small_score_popup(Float3 *pos, i32 value, D3DCOLOR color)
{
    PopupManager *mgr = g_PopupManager;
    if (mgr->next_index >= 10)
    {
        mgr->next_index = 0;
    }
    PopupString *str = &mgr->strings[mgr->next_index];
    i32 n = 0;
    str->active = 1;
    if (value >= 0)
    {
        while (value != 0)
        {
            str->digits[n] = value % 10;
            n++;
            value /= 10;
        }
        if (n == 0)
        {
            str->digits[0] = 0;
            n = 1;
        }
    }
    else
    {
        str->digits[0] = 10;
        n = 1;
    }
    str->num_digits = n;
    str->color = color;
    str->time.reset();
    str->pos = *pos;
    str->unk_18 = 1.0f;
    mgr->next_index++;
}

static_assert(offsetof(AnmVm, sprite_size) == 0x70, "AnmVm::sprite_size");
static_assert(offsetof(PopupManager, strings) == 0x614, "PopupManager::strings");

// The score strings rise digit by digit and fade near the player; the
// release bonus strings (13 and up) go through the ASCII manager.
// TODO: same operations; register allocation and the order of the sprite setup stores differ.
// FUNCTION: TH16 0x44a000
int PopupManager::on_draw()
{
    if (!(g_Supervisor.config.flags_2c & 4))
    {
        g_Supervisor.disable_d3d_fog_inline();
    }
    PopupString *s = strings;
    for (i32 i = 0; i < 13; i++, s++)
    {
        f32 spacing = 8.0f;
        if (!s->active)
        {
            continue;
        }
        if (s->time.current < 8)
        {
            spacing /= s->time.current_f;
        }
        vm.entity_pos.x = s->pos.x - s->num_digits * spacing * 0.5f;
        vm.entity_pos.y = s->pos.y;
        vm.color_1.d3d = s->color;
        f32 dx = g_Player->inner.pos.x - s->pos.x;
        f32 dy = g_Player->inner.pos.y - s->pos.y;
        i32 dist = dy * dy + dx * dx;
        i32 alpha;
        if (dist > 0x4000)
        {
            alpha = 0xff;
        }
        else if (dist > 0x1000)
        {
            alpha = ((dist - 0x1000) << 7) / 0x3000 + 0x80;
        }
        else
        {
            alpha = 0x80;
        }
        u8 *digit = (u8 *)&s->digits[s->num_digits - 1];
        for (i32 j = s->num_digits; j > 0; j--, digit--)
        {
            i32 sprite;
            if (s->time.current < 0x34 - j * 2 || *digit == 10)
            {
                sprite = *digit + ASCII_SPRITE_POPUP_DIGITS;
            }
            else if (s->time.current < 0x38 - j * 2)
            {
                sprite = *digit + 0x10e;
            }
            else if (s->time.current < 0x3c - j * 2)
            {
                sprite = *digit + 0x118;
            }
            else
            {
                goto next;
            }
            {
                AnmVm *v = &vm;
                AnmManager *anm = g_AnmManager;
                v->set_sprite_uvs(sprite);
                vm.color_1.a = alpha;
                v->flags_lo |= ANM_VM_SCALE_CHANGED;
                v->sprite_size.x = anm->loaded_anms[v->anm_loaded_index]->sprites[v->sprite_id].sprite_width;
                AnmVm::write_sprite_corners__without_rot(
                    v, (Float3 *)&g_sprite_temp_buffer[0].pos, (Float3 *)&g_sprite_temp_buffer[1].pos,
                    (Float3 *)&g_sprite_temp_buffer[2].pos, (Float3 *)&g_sprite_temp_buffer[3].pos);
                anm->render_sprite_2d(v, 1);
            }
        next:
            vm.entity_pos.x += spacing;
        }
    }
    for (i32 i = 0; i < 5; i++, s++)
    {
        if (!s->active)
        {
            continue;
        }
        AsciiInf *ascii = g_AsciiManager;
        ascii->font_id = 2;
        ascii->group = 2;
        ascii->color.d3d = s->color;
        ascii->align_h = 0;
        ascii->align_v = 0;
        Float3 pos = s->pos;
        pos.x += 224.0f;
        pos.y += 16.0f;
        if (s->bonus >= 0)
        {
            ascii->create_stringf(&pos, "BONUS %.1f", s->bonus_rate);
            pos.y += 11.0f;
            g_AsciiManager->create_stringf(&pos, "%d", s->bonus);
        }
        else
        {
            ascii->create_stringf(&pos, "NO BONUS");
        }
        g_AsciiManager->color.d3d = 0xffffffff;
        g_AsciiManager->group = 0;
        g_AsciiManager->font_id = 0;
        g_AsciiManager->align_h = 1;
        g_AsciiManager->align_v = 1;
    }
    return 1;
}
