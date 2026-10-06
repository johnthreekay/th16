// Stand-ins for other units' functions that unit 12b's callers need LTCG
// to see (custom conventions, registers they leave alone). Compiled with
// /GL and not forced alive.
#include "../AnmManager.h"
#include "../BulletManager.h"
#include "../ScreenEffect.h"

void placeholder_sink(int a, float b);

// STUB: TH16 0x416e20
HARNESS_CALLED void BulletManager::cancel_rectangle_as_bomb(D3DXVECTOR3 *pos, D3DXVECTOR3 *size, f32 angle, i32 mode)
{
    placeholder_sink(mode, angle + pos->x + size->y);
}

// The real body: callers keep values in edx across calls to it, which LTCG
// only allows when it can see that the callee never touches edx.
// STUB: TH16 0x46f220
DECOMP_NOINLINE void AnmManager::mark_tree_for_deletion(AnmVm *vm)
{
    if (vm != NULL && !(vm->flags_hi & 0x4000000))
    {
        vm->flags_hi &= ~0x40;
        vm->flags_hi |= 0x20;
        for (ZunList<AnmVm> *node = vm->list_of_children.next; node != NULL; node = node->next)
        {
            mark_tree_for_deletion(node->entry);
        }
    }
}


// ScreenEffect::create and initialize as LTCG sees them: callers with a
// constant mode get both inlined with the switch folded away. Another unit
// owns the real ones.
#define SCREEN_EFFECT_ON_TICK(callback) \
    f = g_UpdateFuncRegistry->create_func(callback); \
    f->flags |= UPDATE_FUNC_ACTIVE; \
    f->arg = this; \
    g_UpdateFuncRegistry->register_on_tick(f, 19); \
    on_tick_func = f;
#define SCREEN_EFFECT_ON_DRAW(callback) \
    f = g_UpdateFuncRegistry->create_func(callback); \
    f->flags |= UPDATE_FUNC_ACTIVE; \
    f->arg = this; \
    g_UpdateFuncRegistry->register_on_draw(f, draw_priority); \
    on_draw_func = f;

// 0x45d150 (inlined everywhere here, so not annotated).
__forceinline ScreenEffect *ScreenEffect::create(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24, i32 draw_priority)
{
    ScreenEffect *effect = new ScreenEffect;
    effect->initialize(mode, arg_18, arg_1c, arg_20, arg_24, draw_priority);
    return effect;
}

// 0x45d1a0 (inlined everywhere here, so not annotated).
__forceinline void ScreenEffect::initialize(i32 mode, i32 arg_18, i32 arg_1c, i32 arg_20, i32 arg_24, i32 draw_priority)
{
    UpdateFunc *f;
    switch (mode)
    {
    case 0:
        SCREEN_EFFECT_ON_TICK(on_tick_a);
        SCREEN_EFFECT_ON_DRAW(on_draw_a);
        break;
    case 1:
        SCREEN_EFFECT_ON_TICK(on_tick_shake);
        break;
    case 2:
        SCREEN_EFFECT_ON_TICK(on_tick_b);
        SCREEN_EFFECT_ON_DRAW(on_draw_b);
        break;
    case 3:
        unk_14 = 0xff;
        SCREEN_EFFECT_ON_TICK(on_tick_a);
        SCREEN_EFFECT_ON_DRAW(on_draw_b);
        break;
    case 4:
        SCREEN_EFFECT_ON_TICK(on_tick_e);
        SCREEN_EFFECT_ON_DRAW(on_draw_e);
        break;
    case 5:
        SCREEN_EFFECT_ON_TICK(on_tick_b);
        SCREEN_EFFECT_ON_DRAW(on_draw_a);
        break;
    case 6:
        SCREEN_EFFECT_ON_TICK(on_tick_c);
        SCREEN_EFFECT_ON_DRAW(on_draw_c);
        break;
    case 7:
        SCREEN_EFFECT_ON_TICK(on_tick_c);
        SCREEN_EFFECT_ON_DRAW(on_draw_d);
        break;
    case 8:
        SCREEN_EFFECT_ON_TICK(on_tick_f);
        break;
    case 9:
        SCREEN_EFFECT_ON_TICK(on_tick_d);
        SCREEN_EFFECT_ON_DRAW(on_draw_c);
        break;
    }
    on_tick_func->on_cleanup = on_cleanup;
    timer = 0;
    this->mode = mode;
    this->arg_18 = arg_18;
    this->arg_1c = arg_1c;
    this->arg_20 = arg_20;
    this->arg_24 = arg_24;
}

// Taking the address emits out-of-line copies for callers in other files;
// LTCG still inlines the bodies there.
void *g_screen_effect_create_keepalive = (void *)&ScreenEffect::create;
