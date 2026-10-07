#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "EnemyManager.h"
#include "PosVel.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

struct BombReimuAOrbs;

// The player's bomb (main, one class per character) and season release
// (sub, one class per subseason). One of each exists while a stage runs
// (g_MainBomb, g_SubseasonBomb); the classes differ only in their virtual
// functions. Layout from ExpHP's zBomb.
//
// A bomb runs from activate until on_tick returns nonzero, cancelling
// bullets through cancel_bullets and hurting enemies through damage
// sources. A release then cools down for 45 frames (timer counts up from
// -45) and shows the PIV its cancels earned.
//
// VTABLE: TH16 0x491e58
class BombInf
{
  public:
    // The constructor sets 2; nothing else uses it.
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    // Nothing in TH16 uses unk_10, unk_24, timer_48, unk_6c, unk_74,
    // timer_94 or timer_a8.
    void *unk_10;
    // Where the bomb is centered (the player's position when it started,
    // or following them).
    D3DXVECTOR3 pos;
    // Aya's bomb: how far it moves each frame.
    f32 speed;
    u8 unk_24[0x2c - 0x24];
    // The direction of the bomb's rectangle or beam.
    f32 angle;
    i32 in_use;
    // Counts frames while in use; negative while a release cools down.
    ZunTimer timer;
    ZunTimer timer_48;
    // The bomb's main VM (a release: its inner circle, whose scale is the
    // damage and cancel radius). The bomb ends with it.
    AnmId anm_id;
    // Deleted by the destructor; nothing in TH16 creates it.
    AnmId anm_id_60;
    // A second VM: an effect around the player (bombs) or the release's
    // outer ring.
    AnmId anm_id_secondary;
    // Set when a bomb starts during a spell card that has run a second
    // (nothing reads it).
    i32 started_during_spell;
    i32 unk_6c;
    // Reimu's bomb: her eight orbs (malloc'd by begin).
    BombReimuAOrbs *reimu_orbs;
    u8 unk_74[0x20];
    ZunTimer timer_94;
    ZunTimer timer_a8;
    // Deleted by the destructor; nothing in TH16 creates them.
    AnmId anm_id_bc;
    AnmId anm_id_c0;
    AnmId anm_id_c4;
    u8 unk_c8[8];
    // Freed by the destructor; nothing in TH16 allocates it.
    void *unk_d0;
    // 0 for the bomb, 1 for the release.
    i32 is_season;
    // The season level a release started at.
    i32 season_level;
    // PIV bonus of the last release and the value on display.
    f32 release_bonus;
    f32 release_bonus_shown;
    ZunTimer release_bonus_timer;
    // Season level of the last release; picks the bonus text color.
    i32 release_bonus_level;
    D3DXVECTOR3 release_bonus_pos;

    DECOMP_NOINLINE BombInf();
    // 0x40d710. Deletes the VMs and unregisters the update functions.
    ~BombInf();

    // Starts the bomb: VMs, sound, invincibility, screen shake.
    virtual i32 begin();
    // Runs a frame; nonzero ends the bomb.
    virtual i32 on_tick();
    virtual i32 on_draw();
    // The bomb's own damage to an enemy at enemy_pos (with enemy_size for
    // a rectangle; both Float3 pointers): 0 for every bomb, which hurt
    // through damage sources instead.
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    // Cancels the bullets and lasers the bomb covers this frame.
    virtual i32 cancel_bullets();
    // Ends a bomb still running when the stage is cleared (only Reimu's
    // needs to).
    virtual void end_at_stage_clear();

    // 0x40d600. Registers the update functions; 0 on success.
    i32 initialize(i32 is_season);
    // 0x40dd00. The on_tick function: the release cooldown, then the
    // bomb's on_tick while in use.
    i32 update();
    // 0x40de30. Shows the last release's PIV bonus above the player.
    void draw();
    // 0x40db20. Uses up a bomb (or the release's season power), plays the
    // sound and begins. -1 if already in use.
    i32 activate();
    // 0x40dda0. Whether the bomb or release can start: one in stock (or
    // season level 1 and no cooldown), neither running, no dialogue, and
    // the stage's enemies running.
    i32 can_activate();
    // 0x40e040. A release ended: show its bonus and cool down for 45
    // frames.
    void start_release_cooldown();
    // 0x42f090. Whether the bomb is running and younger than time frames.
    i32 is_active_before(i32 time);

    // 0x40da60, 0x40da70
    static int __fastcall on_tick_callback(void *arg);
    static int __fastcall on_draw_callback(void *arg);

    // 0x40d890
    static BombInf *create();
    // 0x40da90. Deletes both bombs.
    static void destroy_all();
};

// One of Reimu's homing orbs.
struct BombReimuAOrb
{
    AnmId anm_id;
    union
    {
        struct
        {
            D3DXVECTOR3 pos;
            u8 unk_10[0x38 - 0x10];
            // Where the orb was launched from.
            D3DXVECTOR3 start_pos;
            u8 unk_44[0x48 - 0x44];
        };
        // The orb's motion: pos is its first field.
        PosVel motion;
    };
    u8 unk_48[0xa0 - 0x48];
    i32 active;
    ZunTimer timer;
    // How far the orb moved last frame.
    D3DXVECTOR3 move;
    // The enemy it homes in on.
    EnemyRef target;
    class EnemyInf *target_enemy;
    // Which of the eight orbs this is.
    i32 index;
    // Index plus one of the orb's damage source, 0 for none.
    i32 damage_source;
    // Set once the orb has exploded; it is then just deleted.
    i32 done;

    // 0x4109d0. Launches the orb from pos.
    void start(i32 index, D3DXVECTOR3 *pos);
    // 0x410ae0
    void finish();
    // 0x410550. Circles the player, then flies off and homes in on the
    // closest enemy.
    void update();
};

struct BombReimuAOrbs
{
    BombReimuAOrb orbs[8];

    // 0x410bb0. Ends every orb.
    void finish_all();
};

// Reimu's bomb: eight orbs circle her, then home in on enemies and burst.
// VTABLE: TH16 0x491e3c
class BombReimuAInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 cancel_bullets();
    virtual void end_at_stage_clear();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
};

// Cirno's bomb: a circle of ice that grows around where she bombed.
// VTABLE: TH16 0x491e04
class BombCirnoAInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 cancel_bullets();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual void end_at_stage_clear();
};

// Aya's bomb: a wide band of wind that sweeps sideways across the screen.
// VTABLE: TH16 0x491de8
class BombAyaAInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 cancel_bullets();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual void end_at_stage_clear();
};

// Marisa's bomb: a master spark that slows her down and stops her shot.
// VTABLE: TH16 0x491e20
class BombMarisaAInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 cancel_bullets();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual void end_at_stage_clear();
};

// Spring release.
// VTABLE: TH16 0x491dcc
class BombReimuSubInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual i32 cancel_bullets();
    virtual void end_at_stage_clear();
};

// Summer release.
// VTABLE: TH16 0x491d94
class BombCirnoSubInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual i32 cancel_bullets();
    virtual void end_at_stage_clear();
};

// Autumn release.
// VTABLE: TH16 0x491d78
class BombAyaSubInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual i32 cancel_bullets();
    virtual void end_at_stage_clear();
};

// Winter release.
// VTABLE: TH16 0x491db0
class BombMarisaSubInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual i32 cancel_bullets();
    virtual void end_at_stage_clear();
};

// Doyou (all seasons) release.
// VTABLE: TH16 0x491d5c
class BombAllSubInf : public BombInf
{
  public:
    virtual i32 begin();
    virtual i32 on_tick();
    virtual i32 on_draw();
    virtual i32 compute_damage(i32 enemy_pos, i32 enemy_size);
    virtual i32 cancel_bullets();
    virtual void end_at_stage_clear();
};

extern BombInf *g_MainBomb;
extern BombInf *g_SubseasonBomb;
