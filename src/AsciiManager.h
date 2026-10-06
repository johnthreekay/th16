#pragma once

#include <d3d9.h>

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

// One line of text drawn with the ASCII font. Layout from ExpHP's
// th-re-data (zAsciiStr).
struct AsciiStr
{
    char text[0x100];
    Float3 pos;
    D3DCOLOR color;
    Float2 scale;
    i32 unk_118;
    i32 unk_11c;
    i32 font_id;
    i32 draw_shadows;
    i32 render_group;
    i32 remaining_time;
    i32 align_h;
    i32 align_v;
};

// Draws the debug and HUD text. ExpHP calls it AsciiManager; the name is
// ZUN's, from RTTI. Layout from ExpHP's th-re-data (zAsciiManager).
class AsciiInf : public TaskInf
{
  public:
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw_1;
    AnmVm vm_1;
    AnmVm vm_2;
    AsciiStr strings[0x140];
    i32 num_strings;
    // Settings copied into each new string.
    D3DCOLOR color;
    Float2 scale;
    i32 unk_19218;
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
    AnmId unk_19244;
    AnmId id_for_now_loading;
    UpdateFunc *on_draw_2;
    UpdateFunc *on_draw_3;

    AsciiInf();
    ~AsciiInf();
    virtual u32 get_size();
    i32 initialize();
    void tick();
    void create_string(Float3 *pos, const char *text);
    void create_stringf(Float3 *pos, const char *fmt, ...);
    void create_debug_stringf(Float3 *pos, const char *fmt, ...);
    // Score-style number with thousands separators.
    void create_number(Float3 *pos, u32 value);
    // The same with a last digit drawn after the separators (value * 10 + digit).
    void create_number_with_digit(Float3 *pos, u32 value, u32 digit);
    void draw_string(AsciiStr *str);
    i32 draw_group(i32 group);
    i32 draw_group_1();

    // UpdateFunc callbacks; the argument is the AsciiInf.
    static int __fastcall on_tick_callback(void *arg);
    static int __fastcall on_draw_1_callback(void *arg);
    static int __fastcall on_draw_2_callback(void *arg);
    static int __fastcall on_draw_3_callback(void *arg);
};

extern AsciiInf *g_AsciiManager;
