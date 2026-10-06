#include <string.h>

#include "AsciiManager.h"
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

    ascii_anm->init_vm_with_sprite(&vm, 0x103);
    vm.flags_hi = (vm.flags_hi & ~0x440000) | 0x380000;
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
void PopupManager::generate_small_score_popup(Float3 *pos, i32 value, D3DCOLOR color)
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
