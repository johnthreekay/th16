#pragma once

#include "AnmManager.h"
#include "AnmVm.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// A dialogue script file: a count, then one entry per script whose first
// word is the script's offset from the start of the file.
struct MsgFile
{
    i32 script_count;
    struct
    {
        i32 offset;
        i32 unk_4;
    } scripts[1];
};

// One dialogue instruction (ExpHP: zMsgRawInstr).
struct MsgRawInstr
{
    u16 time;
    u8 opcode;
    // Size of the arguments.
    u8 args_size;
    union
    {
        i32 i[1];
        f32 f[1];
        char s[1];
    } args;
};

// The bitfields of GuiMsgVm::flags that dialogue instructions assign; the
// assignments compile to xor/and/xor.
struct GuiMsgVmFlags
{
    // Instruction 10: skipping is allowed.
    u32 skippable : 1;
    // Instruction 26: the text uses the second font.
    u32 font : 1;
    // Instruction 29: the speech bubble type.
    u32 textbox_type : 4;
    u32 unk_6 : 26;
};

// A running dialogue script. Layout from ExpHP's th-re-data (zGuiMsgVm).
struct GuiMsgVm
{
    i32 script_num;
    ZunTimer timer_4;
    ZunTimer time_in_script;
    ZunTimer pause_timer;
    AnmId player_face;
    AnmId enemy_faces[4];
    AnmId id_54;
    AnmId text_line_1;
    AnmId text_line_2;
    AnmId furigana_1;
    AnmId furigana_2;
    AnmId intro;
    // The speech bubble.
    AnmId textbox;
    AnmId id_70;
    i32 menu_time;
    i32 menu_state;
    MenuHelper menu;
    i32 unk_154;
    void *current_instr;
    Float3 vec_15c;
    Float3 vec_168;
    Float3 vec_174;
    Float3 vec_180;
    i32 unk_18c;
    u32 flags;
    i32 next_text_line;
    i32 unk_198;
    // 0: player, 1: enemy.
    i32 active_side;
    i32 unk_1a0;
    i32 unk_1a4;
    i32 unk_1a8;
    i32 unk_1ac;
    f32 unk_1b0;
    f32 unk_1b4;
    f32 unk_1b8;
    f32 unk_1bc;
    i32 unk_1c0;
    // Which speech bubble shape; front.anm scripts are numbered after it.
    i32 textbox_kind;

    // 0x429b20
    GuiMsgVm(void *script);
    // 0x4264a0
    ~GuiMsgVm();
    // Moves vm (the speech bubble's tail) next to the bubble.
    void update_callout(AnmVm *vm);
    // Hide and show every face and text VM.
    void hide();
    void show();
    // Replaces the speech bubble. LTCG passes x, y and width in xmm1-3.
    HARNESS_CALLED void set_textbox(f32 x, f32 y, f32 width, i32 kind);
    // LTCG passes the width in xmm1.
    HARNESS_CALLED void set_textbox_width(f32 width, i32 kind);
    // 0x42a1d0. Runs the instructions whose time has come and advances the
    // script time; -1 once the script has ended.
    HARNESS_CALLED i32 run();

    MsgRawInstr *instr()
    {
        return (MsgRawInstr *)current_instr;
    }
};

// One of the three boss life bars.
struct GuiBossBar
{
    u8 unk_0[0x30];
    AnmId ids[7];
    i32 unk_4c;
    u8 unk_50[4];

    GuiBossBar()
    {
    }
};

// The HUD. Layout from ExpHP's th-re-data (zGui).
struct Gui
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw_1;
    AnmId life_counter_ids[8];
    AnmId bomb_counter_ids[8];
    AnmId id_4c;
    AnmId id_50;
    AnmVm *life_counter_vms[8];
    AnmVm *bomb_counter_vms[8];
    AnmVm *vm_94;
    AnmVm *vm_98;
    AnmId id_9c;
    AnmId ids_a0[10];
    AnmId id_c8;
    AnmId id_cc;
    AnmId id_d0;
    AnmId id_d4;
    // Only while a boss is on screen.
    AnmId boss_id_d8;
    AnmId boss_star_ids[9];
    AnmId id_100;
    AnmId id_104;
    AnmId difficulty_id;
    AnmId season_gauge_id;
    AnmId id_110;
    // Whether the season gauge shows a level (update_season_gauge).
    i32 season_gauge_has_level;
    u8 unk_118[0x11c - 0x118];
    AnmId ids_11c[5];
    u8 unk_130[0x14c - 0x130];
    // Set by sub_42bcf0's notices.
    i32 unk_14c;
    AnmId id_150;
    ZunTimer time_in_stage;
    UpdateFunc *on_draw_2;
    i32 unk_16c;
    // The score shown, counting up towards the real one.
    i32 current_score;
    i32 score_step;
    AnmLoaded *stage_logo_anm;
    u8 unk_17c[0x188 - 0x17c];
    i32 boss_star_count;
    u8 unk_18c[0x1ac - 0x18c];
    u32 flags_1ac;
    ZunTimer timer_1b0;
    u8 unk_1c4[0x1c8 - 0x1c4];
    // The dialogue being shown, if any.
    GuiMsgVm *msg;
    // This stage's dialogue file.
    MsgFile *msg_file;
    i32 unk_1d0;
    i32 unk_1d4;
    i32 unk_1d8;
    GuiBossBar boss_bars[3];
    AnmLoaded *front_anm;
    // Score awarded for clearing the stage (show_stage_clear_bonus).
    i32 stage_clear_bonus;
    u8 unk_2e0[0x2e4 - 0x2e0];

    Gui();
    ~Gui();
    static Gui *create();
    i32 initialize();
    // Loads the stage logo and the dialogue file.
    i32 load_stage_files();
    // Frees what load_stage_files loaded.
    void release_stage_files();
    // 0x427970. Frees the dialogue state before a stage restarts. Only
    // called through g_Gui, which LTCG put in place of this.
    HARNESS_CALLED void release_msg();
    void start_dialogue(i32 script);
    static i32 __fastcall on_tick_callback(Gui *self);
    static i32 __fastcall on_draw_1_callback(Gui *self);
    static i32 __fastcall on_draw_2_callback(Gui *self);
    i32 on_tick_body();
    i32 on_draw_2_body();

    void update_lives(i32 lives, u32 fragments);
    // 0x42c390
    void update_bombs(i32 bombs, i32 fragments);
    // Shows the boss marker unless it is already up.
    void show_boss_marker();
    void sub_42c4f0();
    // Only called through g_Gui, which LTCG put in place of this.
    HARNESS_CALLED void sub_42c1b0();
    // 0x4175d0 and 0x417650. Interrupt vm_94 and vm_98 with 2 (a spell card
    // starts) or 3 (it ends) and run them. Only called through g_Gui.
    HARNESS_CALLED void interrupt_spell_vms_2();
    HARNESS_CALLED void interrupt_spell_vms_3();
    // Creates the textbox VM (0x426780) and the stage clear bonus (0x42c070).
    static void create_vm_110();
    // 0x426d70 (ExpHP: gui_426d70_initializes_many_anms). Creates the HUD's
    // VMs for a stage.
    static void sub_426d70();
    static void show_stage_clear_bonus();
    static void sub_42c580();
    static void sub_42c5c0();
    // Counts the shown score up towards the real one.
    static void update_score();
    static i32 __fastcall textbox_on_draw(AnmVm *vm);
    // Shows a HUD notice (2: full power, 4: extend). Its callers in the
    // original keep the stack 8-byte aligned for it (LTCG moved the
    // alignment out of the callee).
    HARNESS_CALLED void sub_42bcf0(i32 unk, i32 kind);
    // 0x42c600
    static void update_season_gauge();
};

extern Gui *g_Gui;
// The dialogue file kept loaded across a stage restart.
extern MsgFile *g_msg_file_cache;

// 0x43f350 (ExpHP: sub_43f350_pause) and 0x42e150, which end a dialogue
// script that asks for it.
void pause_menu_43f350();
void stage_clear_42e150();

// ECL instruction 554: shows the stage logo.
void show_stage_logo();

// Small ANM helpers the HUD code calls.
void __fastcall anm_vm_interrupt_2_run(AnmVm *vm);
void __fastcall anm_vm_interrupt_3_run(AnmVm *vm);
void __fastcall anm_vm_interrupt_4_run(AnmVm *vm);
void __fastcall anm_vm_interrupt_4(AnmVm *vm);
void __fastcall anm_vm_interrupt_5(AnmVm *vm);
// 0x4173c0. GameThread::thread_start and Bullet::run_ex call it.
void __fastcall anm_vm_interrupt_2(AnmVm *vm);

// Debug logging, compiled out of the release build (0x42c9f0).
void debug_log(const char *fmt, ...);

// 0x42bbe0. Decodes an obfuscated dialogue or ending string into a static
// buffer.
const char *LTCG_FASTCALL decode_msg_string(const char *src);
