// Text rendered into the texture of a VM's sprite (dialogue, the ending,
// spell card names, menus).
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "AnmManager.h"
#include "TextHelper.h"

// Kept out of line as in AsciiManager.cpp.
DECOMP_NOINLINE int __CRTDECL _vsnprintf_l(char *buffer, size_t count, const char *format, _locale_t locale, va_list args);

// The height of each font draw_text knows, by font id.
// GLOBAL: TH16 0x491b68
const i32 g_text_font_heights[10] = {17, 17, 17, 17, 21, 21, 21, 21, 14, 12};

// TODO: scheduling only: the original indexes the height table between the vsprintf pushes.
// FUNCTION: TH16 0x46d990
void AnmManager::draw_text(AnmVm *vm, D3DCOLOR color, i32 shadow_color, i32 font, i32 x, i32 spacing,
                           const char *fmt, ...)
{
    char buf[0x80];
    RECT rect;
    va_list args;

    va_start(args, fmt);
    i32 height = g_text_font_heights[font];
    vsprintf(buf, fmt, args);
    va_end(args);
    AnmLoadedSprite *sprite = &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
    for (i32 i = 0; i < 4; i++)
    {
        (&rect.left)[i] = (&sprite->start_pixel_inclusive.x)[i];
    }
    i32 texture_id = sprite->image_file_num_in_all;
    IDirect3DTexture9 *texture = loaded_anms[texture_id >> 8]->d3d[texture_id & 0xff].texture;
    spacing *= 2;
    x *= 2;
    if (height <= 0)
    {
        height = 17;
    }
    if (height > 8)
    {
        if (!(vm->flags_hi & ANM_VM_TEXT_NO_OUTLINE))
        {
            ::draw_text(&rect, x, height, color, shadow_color, buf, texture, font, spacing, 1);
        }
        else
        {
            ::draw_text(&rect, x, height, color, 0, buf, texture, font, spacing, 0);
        }
    }
    vm->flags_lo |= ANM_VM_VISIBLE;
}

// draw_text right-aligned in the sprite (ExpHP: draw_rtext).
// FUNCTION: TH16 0x46dab0
void AnmManager::draw_text_right(AnmVm *vm, D3DCOLOR color, D3DCOLOR shadow_color, i32 font, i32 spacing,
                                 const char *fmt, ...)
{
    char buf[0x80];
    RECT rect;
    va_list args;

    i32 height = g_text_font_heights[font];
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    AnmLoadedSprite *sprite = &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
    for (i32 i = 0; i < 4; i++)
    {
        (&rect.left)[i] = (&sprite->start_pixel_inclusive.x)[i];
    }
    size_t len = strlen(buf);
    i32 texture_id = sprite->image_file_num_in_all;
    IDirect3DTexture9 *texture = loaded_anms[texture_id >> 8]->d3d[texture_id & 0xff].texture;
    spacing *= 2;
    i32 x = sprite->sprite_width - (f32)((height * 2 - 1) * len / 2);
    if (height <= 0)
    {
        height = 17;
    }
    if (height > 8)
    {
        if (!(vm->flags_hi & ANM_VM_TEXT_NO_OUTLINE))
        {
            ::draw_text(&rect, x, height, color, shadow_color, buf, texture, font, spacing, 1);
        }
        else
        {
            ::draw_text(&rect, x, height, color, 0, buf, texture, font, spacing, 0);
        }
    }
    vm->flags_lo |= ANM_VM_VISIBLE;
}

// draw_text centered in the sprite.
// FUNCTION: TH16 0x46dc20
void AnmManager::draw_text_centered(AnmVm *vm, D3DCOLOR color, D3DCOLOR shadow_color, i32 font, i32 spacing,
                                    const char *fmt, ...)
{
    char buf[0x80];
    RECT rect;
    va_list args;

    i32 height = g_text_font_heights[font];
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    AnmLoadedSprite *sprite = &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
    for (i32 i = 0; i < 4; i++)
    {
        (&rect.left)[i] = (&sprite->start_pixel_inclusive.x)[i];
    }
    size_t len = strlen(buf);
    i32 texture_id = sprite->image_file_num_in_all;
    IDirect3DTexture9 *texture = loaded_anms[texture_id >> 8]->d3d[texture_id & 0xff].texture;
    spacing *= 2;
    i32 x = (i32)sprite->sprite_width / 2 - (height * 2 - 1) * len / 4;
    if (height <= 0)
    {
        height = 17;
    }
    if (height > 8)
    {
        if (!(vm->flags_hi & ANM_VM_TEXT_NO_OUTLINE))
        {
            ::draw_text(&rect, x, height, color, shadow_color, buf, texture, font, spacing, 1);
        }
        else
        {
            ::draw_text(&rect, x, height, color, 0, buf, texture, font, spacing, 0);
        }
    }
    vm->flags_lo |= ANM_VM_VISIBLE;
}
