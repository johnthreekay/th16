#pragma once

#include "AnmManager.h"
#include "AnmVm.h"
#include "MenuHelper.h"
#include "UpdateFunc.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// A dialogue script file (stN<character>.msg): a count, then one entry per
// script whose first word is the script's offset from the start of the file.
struct MsgFile
{
    i32 script_count;
    struct
    {
        i32 offset;
        // Not read by the game.
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

// Dialogue (MSG) instruction opcodes. Names from truth's TH11+ msgmap where
// it has one.
enum MsgOpcode
{
    MSG_END = 0,
    // Shows the player's face (argument 0) or the alternative one.
    MSG_PLAYER_SHOW = 1,
    // Shows the face of the boss with the given index.
    MSG_BOSS_SHOW = 2,
    // A no-op since TH128.
    MSG_TEXTBOX_SHOW = 3,
    MSG_PLAYER_HIDE = 4,
    MSG_BOSS_HIDE = 5,
    MSG_TEXTBOX_HIDE = 6,
    MSG_SPEAKER_PLAYER = 7,
    MSG_SPEAKER_BOSS = 8,
    MSG_SPEAKER_NONE = 9,
    MSG_SKIPPABLE = 10,
    // Waits for the given time or a key.
    MSG_TEXT_PAUSE = 11,
    // Lets the ECL script waiting in dialogWait go on.
    MSG_ECL_RESUME = 12,
    MSG_PLAYER_FACE = 13,
    MSG_BOSS_FACE = 14,
    MSG_TEXT_LINE_1 = 15,
    MSG_TEXT_LINE_2 = 16,
    MSG_TEXT_ADD = 17,
    MSG_TEXT_CLEAR = 18,
    MSG_MUSIC_BOSS = 19,
    MSG_INTRO = 20,
    MSG_STAGE_END = 21,
    MSG_MUSIC_END = 22,
    MSG_PLAYER_SHAKE = 23,
    MSG_BOSS_SHAKE = 24,
    MSG_TEXT_OFFSET_Y = 25,
    // Unnamed in truth: the text uses the second font (until the speaker
    // changes).
    MSG_TEXT_FONT_2 = 26,
    MSG_MUSIC_FADE = 27,
    MSG_BUBBLE_POS = 28,
    MSG_BUBBLE_TYPE = 29,
    // Great Fairy Wars only; a no-op here.
    MSG_ROUTE_SELECT = 30,
    // Unnamed in truth: shows the second boss's face (MSG_BOSS_SHOW 1).
    MSG_BOSS_SHOW_SECOND = 31,
    // Unnamed in truth: makes the given side the speaker without changing
    // the faces.
    MSG_SPEAKER_SIDE = 32,
    MSG_PORTRAIT_DARKEN = 33,
    MSG_PORTRAIT_HIGHLIGHT = 34,
    MSG_LIGHTS_OUT = 35,
};

// Masks of GuiMsgVm::flags.
enum GuiMsgVmFlagMask
{
    // MSG_SKIPPABLE: holding shot or skip fast-forwards the script.
    MSG_FLAG_SKIPPABLE = 1 << 0,
    // MSG_TEXT_FONT_2.
    MSG_FLAG_FONT_2 = 1 << 1,
    // Shot or skip was pressed since the script started: skipping only
    // starts then, so a key still held from before does not skip.
    MSG_FLAG_KEY_PRESSED = 1 << 6,
    MSG_FLAG_CAN_SKIP = MSG_FLAG_SKIPPABLE | MSG_FLAG_KEY_PRESSED,
};

// The bitfields of GuiMsgVm::flags that dialogue instructions assign; the
// assignments compile to xor/and/xor.
struct GuiMsgVmFlags
{
    // MSG_SKIPPABLE: skipping is allowed.
    u32 skippable : 1;
    // MSG_TEXT_FONT_2: the text uses the second font.
    u32 font : 1;
    // MSG_BUBBLE_TYPE: the speech bubble type.
    u32 textbox_type : 4;
    // MSG_FLAG_KEY_PRESSED.
    u32 key_pressed : 1;
    u32 unused : 25;
};

// A running dialogue script. Layout from ExpHP's th-re-data (zGuiMsgVm).
struct GuiMsgVm
{
    i32 script_num;
    // Frames the dialogue has been up (ticked by Gui::on_tick_body).
    ZunTimer time_alive;
    ZunTimer time_in_script;
    // Counts down the wait of MSG_TEXT_PAUSE.
    ZunTimer pause_timer;
    AnmId player_face;
    AnmId enemy_faces[4];
    // Never created in TH16; the speaker instructions interrupt it (2 when
    // the player speaks, 3 otherwise).
    AnmId id_54;
    AnmId text_line_1;
    AnmId text_line_2;
    AnmId furigana_1;
    AnmId furigana_2;
    // The boss's name and title (MSG_INTRO).
    AnmId intro;
    // The speech bubble.
    AnmId textbox;
    // Never created in TH16; only hidden and shown with the rest.
    AnmId id_70;
    // A menu of an earlier game's dialogue, unused here (ExpHP).
    i32 menu_time;
    i32 menu_state;
    MenuHelper menu;
    // Zeroed by the constructor; not otherwise used.
    i32 unk_154;
    void *current_instr;
    // Where MSG_SPEAKER_NONE puts the text, per side (indexed by
    // active_side as an array; all four are (16, 0, 0)).
    Float3 side_text_pos_0;
    Float3 side_text_pos_1;
    Float3 side_text_pos_2;
    Float3 side_text_pos_3;
    // Set to 1 by MSG_ECL_RESUME and counted down each frame: while it is
    // nonzero, the ECL script waiting in dialogWait goes on.
    i32 ecl_resume_timer;
    // GuiMsgVmFlagMask and GuiMsgVmFlags.
    u32 flags;
    // MSG_TEXT_ADD writes line 1 (0) or line 2 (1) next.
    i32 next_text_line;
    // Whether MSG_TEXT_ADD has cleared the old text for the current pair of
    // lines.
    i32 text_cleared;
    // 0: player, 1: enemy.
    i32 active_side;
    // Text color per side (indexed by active_side as an array; all zero).
    i32 side_text_color_0;
    i32 side_text_color_1;
    i32 side_text_color_2;
    i32 side_text_color_3;
    // The speech bubble's position (MSG_BUBBLE_POS, read as a Float3) and
    // the width of its longest line so far.
    f32 bubble_x;
    f32 bubble_y;
    f32 bubble_z;
    f32 bubble_width;
    // Zeroed by MSG_BOSS_SHOW; never read.
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
    // Matching workaround: safebuffers drops the /GS cookie ours gets for
    // the Float3 copies of the bubble position kept in memory; the original
    // has none, for reasons not understood yet (docs/findings.md). Remove it
    // once the cause is known.
    __declspec(safebuffers) HARNESS_CALLED i32 run();

    MsgRawInstr *instr()
    {
        return (MsgRawInstr *)current_instr;
    }
};

// A marker on a boss life bar (ECL lifeMarker).
struct GuiBossLifeMarker
{
    // Fraction of the bar.
    f32 position;
    u32 color;
};

// One of the three boss life bars: a ring around the boss.
struct GuiBossBar
{
    // The fraction of the bar shown, catching up with fill.
    f32 shown;
    // The boss's life as a fraction of its maximum.
    f32 fill;
    i32 life;
    u8 unk_0[0x10 - 0xc];
    GuiBossLifeMarker life_markers[4];
    // The ring, two more parts and the four life markers.
    AnmId ids[7];
    // Whether ids hold the bar's VMs.
    i32 vms_created;
    // Whether the bar is faded out for the player standing close to the
    // boss.
    i32 faded;

    GuiBossBar()
    {
    }
};

// front.anm scripts the HUD uses.
enum FrontAnmScript
{
    // The HUD frame.
    FRONT_ANM_HUD_FRAME = 0x00,
    // 8 pieces each.
    FRONT_ANM_LIFE_COUNTER = 0x1e,
    FRONT_ANM_BOMB_COUNTER = 0x26,
    // MSG_LIGHTS_OUT.
    FRONT_ANM_LIGHTS_OUT = 0x3c,
    // Gui::show_notice's notices.
    FRONT_ANM_SPELL_BONUS = 0x3d,
    FRONT_ANM_BONUS_FAILED = 0x3e,
    FRONT_ANM_FULL_POWER = 0x3f,
    FRONT_ANM_HISCORE = 0x40,
    FRONT_ANM_EXTEND = 0x41,
    FRONT_ANM_NOTICE_6 = 0x42,
    // Shown at the start of a fresh game (stage 1, no continue, not a
    // replay).
    FRONT_ANM_GAME_START = 0x45,
    // 10 of them.
    FRONT_ANM_BOSS_STARS = 0x46,
    // Plus the difficulty.
    FRONT_ANM_DIFFICULTY_2 = 0x51,
    FRONT_ANM_DIFFICULTY = 0x57,
    // Behind the spell card bonus.
    FRONT_ANM_SPELL_BONUS_BACK = 0x60,
    // The boss's position below the game area.
    FRONT_ANM_ENEMY_MARKER = 0x70,
    // The season gauge and the children Gui looks up.
    FRONT_ANM_SEASON_GAUGE = 0x71,
    FRONT_ANM_SEASON_GAUGE_BAR = 0x73,
    FRONT_ANM_SEASON_GAUGE_LEVEL = 0x74,
    FRONT_ANM_SEASON_GAUGE_ICON = 0x75,
    FRONT_ANM_SEASON_GAUGE_RELEASE = 0x76,
    // "Demo play".
    FRONT_ANM_DEMO_PLAY = 0x77,
    FRONT_ANM_STAGE_CLEAR_BONUS = 0x78,
    // Plus StageBoss::marker_script.
    FRONT_ANM_BOSS_MARKER = 0xa4,
    // Plus GuiMsgVm::textbox_kind: the bubble's body and edge (children of
    // the bubble) and the bubble itself.
    FRONT_ANM_BUBBLE_BODY = 0xb4,
    FRONT_ANM_BUBBLE_EDGE = 0xd4,
    FRONT_ANM_BUBBLE = 0xe4,
    // A boss life bar: the ring, two more parts and a life marker.
    FRONT_ANM_BOSS_BAR = 0xf4,
    FRONT_ANM_BOSS_BAR_2 = 0xf5,
    FRONT_ANM_BOSS_BAR_3 = 0xf6,
    FRONT_ANM_BOSS_BAR_MARKER = 0xf7,
};

// Sprites of the front.anm season gauge icon, plus the subseason.
#define FRONT_ANM_SPRITE_SUBSEASON 0x52

// ascii.anm scripts and sprites the HUD uses.
enum AsciiAnmHud
{
    // The boss timer's two digits.
    ASCII_ANM_BOSS_TIMER_DIGITS = 2,
    // The spell card bonus: 8 digits and two commas.
    ASCII_ANM_BONUS_DIGITS = 4,
    ASCII_ANM_BONUS_COMMA_1 = 0xc,
    ASCII_ANM_BONUS_COMMA_2 = 0xd,
    // Sprites: digits 0-9 from here, and a comma.
    ASCII_ANM_SPRITE_DIGIT_0 = 0xef,
    ASCII_ANM_SPRITE_COMMA = 0xfd,
};

// The notices of Gui::show_notice.
enum GuiNotice
{
    // The spell card bonus, with its amount.
    GUI_NOTICE_SPELL_BONUS = 0,
    GUI_NOTICE_BONUS_FAILED = 1,
    GUI_NOTICE_FULL_POWER = 2,
    GUI_NOTICE_HISCORE = 3,
    GUI_NOTICE_EXTEND = 4,
    // Not used by any caller (front.anm script 0x42).
    GUI_NOTICE_6 = 6,
};

// Masks of Gui::hud_flags.
enum GuiHudFlags
{
    // The season gauge has moved out of the way of the player.
    GUI_SEASON_GAUGE_AWAY = 1 << 0,
    // The enemy marker's warning level (0, 2, 4 or 6) for the boss's life
    // left in its attack.
    GUI_ENEMY_MARKER_LEVEL_MASK = 3 << 1,
    GUI_ENEMY_MARKER_LEVEL_1 = 1 << 1,
    GUI_ENEMY_MARKER_LEVEL_2 = 1 << 2,
    // Set when the last stage is cleared; not read.
    GUI_FLAG_GAME_CLEARED = 1 << 4,
    // Set by release_stage_files; not read.
    GUI_FLAGS_STAGE_RELEASED = 0xe0,
    // The stage clear bonus is up (notice_timer counts its time).
    GUI_STAGE_CLEAR_BONUS = 1 << 8,
    // The boss timer has moved out of the way of the player, or is hidden.
    GUI_BOSS_TIMER_AWAY = 1 << 9,
    GUI_BOSS_TIMER_HIDDEN = 1 << 10,
    GUI_BOSS_TIMER_STATE_MASK = GUI_BOSS_TIMER_AWAY | GUI_BOSS_TIMER_HIDDEN,
    // The two phases of the chapter result (TH15's; nothing in TH16 starts
    // it): counting the bonus up, then showing the total.
    GUI_CHAPTER_RESULT_COUNTING = 1 << 11,
    GUI_CHAPTER_RESULT_DONE = 1 << 12,
    GUI_CHAPTER_RESULT_MASK = GUI_CHAPTER_RESULT_COUNTING | GUI_CHAPTER_RESULT_DONE,
};

// Indices of Gui::overlay_ids.
enum GuiOverlay
{
    // The stage clear bonus.
    GUI_OVERLAY_STAGE_CLEAR_BONUS = 1,
    // Deleted and hidden with the stage clear bonus; never created in TH16.
    GUI_OVERLAY_2 = 2,
    // Behind the spell card bonus notice.
    GUI_OVERLAY_SPELL_BONUS_BACK = 3,
    // The chapter result (never created in TH16).
    GUI_OVERLAY_CHAPTER_RESULT = 4,
};

// The HUD: lives, bombs, score and the other counters, the season gauge,
// the boss's life bars, timer, stars and markers, notices, and the
// dialogue. Layout from ExpHP's th-re-data (zGui).
struct Gui
{
    u32 flags;
    UpdateFunc *on_tick;
    UpdateFunc *on_draw_1;
    AnmId life_counter_ids[8];
    AnmId bomb_counter_ids[8];
    // The boss timer's two digits (ascii.anm), with the VMs below. Code
    // indexes the pairs as arrays.
    AnmId boss_timer_tens_id;
    AnmId boss_timer_ones_id;
    AnmVm *life_counter_vms[8];
    AnmVm *bomb_counter_vms[8];
    AnmVm *boss_timer_tens_vm;
    AnmVm *boss_timer_ones_vm;
    // The boss's position below the game area.
    AnmId enemy_marker_id;
    // The spell card bonus: 8 digits and two commas.
    AnmId bonus_digit_ids[10];
    // The spell card bonus notices (GUI_NOTICE_SPELL_BONUS and
    // GUI_NOTICE_BONUS_FAILED) and the other notices.
    AnmId spell_notice_id;
    AnmId notice_id;
    // Not used.
    AnmId id_d0;
    AnmId id_d4;
    // The boss marker (the stage table's marker_script). Only while a boss
    // is on screen.
    AnmId boss_marker_id;
    // The boss's remaining spell card stars: ten, the loop over them running
    // on into boss_star_id_9.
    AnmId boss_star_ids[9];
    AnmId boss_star_id_9;
    // A second difficulty display (front.anm 0x51 + difficulty), created
    // when a game starts and sent interrupt 3 at once.
    AnmId id_104;
    AnmId difficulty_id;
    AnmId season_gauge_id;
    // MSG_LIGHTS_OUT's darkness, until the next stage.
    AnmId lights_out_id;
    // Whether the season gauge shows a level (update_season_gauge).
    i32 season_gauge_has_level;
    // Whether the season gauge glows because a release is possible.
    i32 release_ready;
    // Indexed by GuiOverlay.
    AnmId overlay_ids[5];
    // The chapter result's numbers (TH15's; nothing in TH16 sets them):
    // shown by on_draw_2_body. During GUI_CHAPTER_RESULT_COUNTING the bonus
    // (chapter_bonus) goes up by chapter_bonus_step once per point of
    // chapter_percent left; the final values follow.
    i32 chapter_result_count;
    f32 chapter_percent;
    f32 chapter_percent_final;
    i32 chapter_bonus_final;
    i32 chapter_bonus;
    i32 chapter_bonus_step;
    i32 chapter_result_count_2;
    // The spell card bonus notice is up (with its background).
    i32 spell_bonus_shown;
    AnmId hud_frame_id;
    ZunTimer time_in_stage;
    UpdateFunc *on_draw_2;
    // Not used.
    i32 unk_16c;
    // The score shown, counting up towards the real one.
    i32 current_score;
    i32 score_step;
    AnmLoaded *stage_logo_anm;
    u8 unk_17c[0x188 - 0x17c];
    i32 boss_star_count;
    u8 unk_18c[0x1ac - 0x18c];
    // GuiHudFlags.
    u32 hud_flags;
    // Time of the stage clear bonus and the chapter result.
    ZunTimer notice_timer;
    // How long the chapter result stays up.
    i32 chapter_result_duration;
    // The dialogue being shown, if any.
    GuiMsgVm *msg;
    // This stage's dialogue file.
    MsgFile *msg_file;
    // The boss's time left in its attack (set by the boss's ECL), in
    // seconds (-1: none) and hundredths, and the seconds last shown.
    i32 boss_timer_seconds;
    i32 boss_timer_hundredths;
    i32 boss_timer_shown_seconds;
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
    // ECL dialogRead: starts dialogue script n, or -1/-3 (boss or stage
    // music), -2 (end of the stage, or the game over menu).
    void start_dialogue(i32 script);
    static i32 __fastcall on_tick_callback(Gui *self);
    static i32 __fastcall on_draw_1_callback(Gui *self);
    static i32 __fastcall on_draw_2_callback(Gui *self);
    DECOMP_NOINLINE i32 on_tick_body();
    DECOMP_NOINLINE i32 on_draw_2_body();

    // Shows lives and the pieces of the next one in the counter.
    void update_lives(i32 lives, u32 fragments);
    // 0x42c390
    void update_bombs(i32 bombs, i32 fragments);
    // Shows the boss marker unless it is already up.
    void show_boss_marker();
    // Ends the chapter result: hides it and returns to the counting phase.
    void hide_chapter_result();
    // 0x42c1b0. Hides the stage clear bonus. Only called through g_Gui,
    // which LTCG put in place of this.
    HARNESS_CALLED void hide_stage_clear_bonus();
    // 0x4175d0 and 0x417650. Interrupt the boss timer's digits with 2 (a
    // spell card starts) or 3 (it ends) and run them. Only called through
    // g_Gui.
    HARNESS_CALLED void boss_timer_on_spell_start();
    HARNESS_CALLED void boss_timer_on_spell_end();
    // MSG_LIGHTS_OUT (0x426780); its inlined AnmLoaded::create_vm is also
    // the shape of show_stage_clear_bonus (0x42c070).
    static void show_lights_out();
    // 0x426d70 (ExpHP: gui_426d70_initializes_many_anms). Creates the HUD's
    // VMs for a stage.
    static void setup_stage_hud();
    // Shows the stage clear bonus and awards it.
    static void show_stage_clear_bonus();
    // The pause menu hides and shows the chapter result.
    static void hide_chapter_result_vm();
    static void show_chapter_result_vm();
    // Counts the shown score up towards the real one.
    static void update_score();
    // on_draw of the speech bubble's tail.
    static i32 __fastcall textbox_on_draw(AnmVm *vm);
    // 0x42bcf0. Shows a notice (GuiNotice; bonus is the spell card bonus).
    // Its callers in the original keep the stack 8-byte aligned for it
    // (LTCG moved the alignment out of the callee).
    HARNESS_CALLED void show_notice(i32 bonus, i32 kind);
    // 0x42c600
    static void update_season_gauge();
    // ECL lifeMarker: puts marker index of the boss's life bar at position
    // (a fraction of the bar).
    __forceinline void set_boss_life_marker(i32 boss, i32 index, f32 position, u32 color)
    {
        boss_bars[boss].life_markers[index].position = position;
        boss_bars[boss].life_markers[index].color = color;
    }
};

static_assert(sizeof(GuiBossBar) == 0x54, "GuiBossBar size");
static_assert(offsetof(Gui, chapter_percent) == 0x134, "Gui layout");
static_assert(offsetof(Gui, chapter_result_duration) == 0x1c4, "Gui layout");
static_assert(offsetof(Gui, boss_bars) == 0x1dc, "Gui layout");
static_assert(offsetof(Gui, front_anm) == 0x2d8, "Gui layout");
static_assert(sizeof(Gui) == 0x2e4, "Gui size");
static_assert(offsetof(GuiMsgVm, ecl_resume_timer) == 0x18c, "GuiMsgVm layout");
static_assert(offsetof(GuiMsgVm, bubble_x) == 0x1b0, "GuiMsgVm layout");
static_assert(sizeof(GuiMsgVm) == 0x1c8, "GuiMsgVm size");

extern Gui *g_Gui;
// The dialogue file kept loaded across a stage restart.
extern MsgFile *g_msg_file_cache;

// What start_dialogue(-2) and MSG_STAGE_END end with: the pause menu's
// game over state (0x43f350, in PauseMenu.cpp; ExpHP: sub_43f350_pause)
// and the end of the stage (0x42e150, in GameThread.cpp).
void open_game_over_menu();
i32 stage_clear();

// ECL instruction 554: shows the stage logo.
void show_stage_logo();

// Small ANM helpers the HUD code calls: interrupt a VM and maybe run it.
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
