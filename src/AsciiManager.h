#pragma once

#include <d3d9.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "types.h"

// Base of the game's task objects. The name is ZUN's, from RTTI; it has no
// vtable of its own in the executable.
class TaskInf
{
  public:
    // Size of the whole object (AsciiInf: 0x19254, TitleInf: 0x5e00).
    virtual u32 get_size() = 0;
};

// The fonts of ascii.anm (AsciiStr::font_id).
enum AsciiFont
{
    // Full character set, character_spacing_for_font_0 apart.
    ASCII_FONT_DEFAULT = 0,
    // Small debug font (create_debug_stringf).
    ASCII_FONT_DEBUG = 1,
    // 7 pixels wide, letters and a few symbols; 3 with point filtering.
    ASCII_FONT_SMALL = 2,
    ASCII_FONT_SMALL_POINT = 3,
    // 12 pixels wide, digits and a few symbols; 5 with point filtering.
    ASCII_FONT_LARGE = 4,
    ASCII_FONT_LARGE_POINT = 5,
};

// AsciiStr::align_h and align_v.
enum AsciiAlign
{
    ASCII_ALIGN_CENTER = 0,
    // Left or top: pos is where the text starts.
    ASCII_ALIGN_START = 1,
    // Right or bottom.
    ASCII_ALIGN_END = 2,
};

// Sprites of ascii.anm that other code uses.
enum AsciiSprite
{
    // Shown by a VM whose sprite mapping callback returns a negative sprite
    // (AnmVm::run_script).
    ASCII_SPRITE_FALLBACK = 0x102,
    // The popup digits (PopupManager), 0 first.
    ASCII_SPRITE_POPUP_DIGITS = 0x103,
};

// One line of text drawn with the ASCII font. Layout from ExpHP's
// th-re-data (zAsciiStr).
struct AsciiStr
{
    char text[0x100];
    Float3 pos;
    D3DCOLOR color;
    Float2 scale;
    // Never used.
    i32 unk_118;
    // Copied from AsciiInf::unk_19218, never read.
    i32 unk_11c;
    // AsciiFont.
    i32 font_id;
    // Draw a shadow 2 pixels down and right first.
    i32 draw_shadows;
    // Which draw callback draws it (AsciiInf::draw_group).
    i32 render_group;
    // Frames left before AsciiInf::tick drops it.
    i32 remaining_time;
    // AsciiAlign.
    i32 align_h;
    i32 align_v;
};

// Draws the debug and HUD text. ExpHP calls it AsciiManager; the name is
// ZUN's, from RTTI. Layout from ExpHP's th-re-data (zAsciiManager).
//
// VTABLE: TH16 0x491ce0
class AsciiInf : public TaskInf
{
  public:
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func_1;
    // Draws every character of every string (draw_string).
    AnmVm glyph_vm;
    // Set to sprite 0x62 by initialize, otherwise unused.
    AnmVm vm_2;
    // The strings to draw, added by create_string and dropped by tick when
    // their time runs out.
    AsciiStr strings[0x140];
    i32 num_strings;
    // Settings copied into each new string.
    ZunColor color;
    Float2 scale;
    i32 unk_19218;
    // Set to 0 by the constructor, never read.
    i32 unk_1921c;
    i32 draw_shadows;
    i32 font_id;
    i32 group;
    i32 duration;
    i32 align_h;
    i32 align_v;
    i32 character_spacing_for_font_0;
    i32 num_ticks_alive;
    AnmLoaded *ascii_anm;
    // A VM GameThread interrupts on exit; nothing in TH16 sets it.
    AnmId unk_19244;
    AnmId now_loading_id;
    UpdateFunc *on_draw_func_2;
    UpdateFunc *on_draw_func_3;

    AsciiInf();
    ~AsciiInf();
    virtual u32 get_size();
    // Loads the ASCII font for the window size, registers the callbacks and
    // sets up the glyph VMs.
    i32 initialize();
    // Drops the strings whose time ran out.
    void tick();
    // Adds a string at pos (in 640x480 coordinates) with the current
    // settings; dropped when all 0x140 are in use.
    void create_string(Float3 *pos, const char *text);
    // Variadic, so __cdecl with this pushed first (ExpHP:
    // ascii_sprintf_408260).
    void create_stringf(Float3 *pos, const char *fmt, ...);
    // The same in the debug font (ExpHP: AsciiManager::drawf_debug).
    void create_debug_stringf(Float3 *pos, const char *fmt, ...);
    // Score-style number with thousands separators. Static (it uses
    // g_AsciiManager): Gui::on_draw_2_body calls it through a function
    // pointer (see create_number_func in Gui.cpp).
    static void __stdcall create_number(Float3 *pos, u32 value);
    // The same with a last digit drawn after the separators (value * 10 + digit).
    HARNESS_CALLED void create_number_with_digit(Float3 *pos, u32 value, u32 digit);
    // Draws one string a glyph at a time with glyph_vm.
    void draw_string(AsciiStr *str);
    // Draws the strings of one render group, then makes camera 2 current.
    i32 draw_group(i32 group);
    // Group 1: drawn with camera 0 and the arcade HUD origin.
    i32 draw_group_1();

    // 0x41a390. Shows the "now loading" animation at (x, y) (in 640x480
    // coordinates) unless it is already up. Every caller goes through
    // g_AsciiManager, so LTCG replaced this with the global; x and y arrive
    // in xmm1 and xmm2.
    HARNESS_CALLED void show_now_loading(f32 x, f32 y);
    // 0x41a360. Ends the "now loading" animation.
    HARNESS_CALLED void hide_now_loading();

    // The two above as LTCG inlined them into some callers.
    void show_now_loading_inline(f32 x, f32 y)
    {
        Float3 pos(x * 2.0f, y * 2.0f, 0.0f);
        if (now_loading_id.id == 0)
        {
            now_loading_id = ascii_anm->create_vm(0x11, &pos, 0.0f, -1, 0);
        }
    }
    void hide_now_loading_inline()
    {
        AnmManager::interrupt_tree(now_loading_id, 1);
        now_loading_id.id = 0;
    }

    // The ASCII font's ANM file. Callers that create effects from it write
    // `g_AsciiManager->get_anm()->create_effect(...)`: with the accessor the
    // original evaluates the object after the arguments (g_AsciiManager in
    // eax, the result slot in ecx, pushed before ascii_anm is loaded); the
    // plain field access loads g_AsciiManager into ecx first.
    AnmLoaded *get_anm()
    {
        return ascii_anm;
    }

    // UpdateFunc callbacks; the argument is the AsciiInf. on_tick (priority
    // 4) ages the strings; the draw callbacks draw group 0 at priority 0x51,
    // group 1 at 0x35 (draw_group_1) and group 2 at 0x42 with camera 0.
    static int __fastcall on_tick_callback(void *arg);
    static int __fastcall on_draw_1_callback(void *arg);
    static int __fastcall on_draw_2_callback(void *arg);
    static int __fastcall on_draw_3_callback(void *arg);
};

extern AsciiInf *g_AsciiManager;
