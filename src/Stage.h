#pragma once

#include <d3dx9math.h>

#include "AnmManager.h"
#include "AnmVm.h"
#include "Camera.h"
#include "Fog.h"
#include "Interp.h"
#include "UpdateFunc.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

// The 3D stage background: an STD file of objects made of ANM quads, a
// script that moves the camera, and fog. Layouts from ExpHP (zStage,
// zStageInner, zFog, zInterpCameraSky); the STD file format follows thtk.

// Interpolated camera fog (ExpHP: zInterpCameraSky).
struct InterpCameraSky
{
    CameraSky initial;
    CameraSky goal;
    CameraSky bezier_1;
    CameraSky bezier_2;
    CameraSky current;
    ZunTimer time;
    i32 end_time;
    i32 method;

    // 0x40cd10
    CameraSky step();
};

// One ANM quad of an STD object. vm_index is filled in at load time.
struct StdQuad
{
    // Negative ends the list.
    i16 type;
    i16 size;
    i16 script;
    i16 vm_index;
};

struct StdObject
{
    u8 unk_0[3];
    // Bit 0: its VMs are still running.
    u8 flags;
    u8 unk_4[0x1c - 0x4];
    StdQuad quads[1];
};

struct StdInstance
{
    i16 object_id;
    i16 unk_2;
    D3DXVECTOR3 pos;
};

// A script instruction; the arguments follow.
struct StdInstr
{
    // Negative ends the script.
    i32 time;
    i16 opcode;
    i16 size;
    i32 args[1];
};

struct StdHeader
{
    i16 num_objects;
    i16 num_quads;
    u32 instances_offset;
    u32 script_offset;
    u32 unk_c;
    char anm_path[0x80];
    // Offsets from the header, turned into pointers at load time.
    StdObject *objects[1];
};

struct Stage;

// What a Stage keeps twice: the live state and a snapshot of it.
struct StageInner
{
    ZunTimer time_in_stage;
    // Offset of the next script instruction from the script start.
    i32 cur_instr_offset;
    i32 unk_18;
    ZunTimer timer_1c;
    InterpFloat3 camera_facing_i;
    InterpFloat3 camera_pos_i;
    InterpFloat3 camera_up_i;
    InterpCameraSky camera_sky_i;
    Camera camera;
    Stage *stage;
    AnmVm anm_vms[8];
    u8 unk_32f0[0x3310 - 0x32f0];
    // A squared distance (3100 squared at load time).
    f32 unk_3310;
    Fog *fog;
    u8 unk_3318[0x3324 - 0x3318];
    ZunTimer fog_timer;
    u8 unk_3338[0x3344 - 0x3338];
    // Color passed to the ANM manager; the top byte flags a new value.
    u32 color_3344;

    // Only destroys the VMs; Stage's unwind code calls it out of line.
    ~StageInner();

    // 0x409490. Starts interpolating the fog from its current value.
    void set_sky_interp(i32 end_time, i32 method, CameraSky *goal);
    // 0x40b3b0
    void run_std();
    // 0x40c4a0
    void step_fog();
};

enum StageFlags
{
    STAGE_FLAG_1 = 1 << 0,
    // Fading in (timer_66c8 counts the fade).
    STAGE_FADING_IN = 1 << 1,
    // Fading out.
    STAGE_FADING_OUT = 1 << 2,
    // Stops ticking and drawing.
    STAGE_DISABLED = 1 << 3,
};

struct Stage
{
    u32 flags;
    UpdateFunc *on_tick_func;
    UpdateFunc *on_draw_func;
    StageInner inner;
    StageInner lolk_snapshot_inner;
    // One VM per STD quad, and a snapshot of them.
    AnmVm *vms;
    AnmVm *snapshot_vms;
    // A private copy of the STD file.
    StdHeader *std;
    StdObject **objects;
    StdInstance *instances;
    StdInstr *script;
    AnmLoaded *stage_anm;
    u8 unk_66b8[0x66c4 - 0x66b8];
    // StageFlags.
    u32 stage_flags;
    ZunTimer fade_timer;
    i32 stage_num;
    // Frames ticked.
    i32 frame_count;
    // The STD file as read from disk, kept for later stages.
    void *std_file;
    i32 std_file_size;
    UpdateFunc *on_draw_func_2;

    Stage();
    ~Stage();

    // 0x409db0. Creates the stage and loads the STD file at path.
    static HARNESS_CALLED Stage *create(const char *path);
    // Every caller passes 0 for unused, which LTCG folds.
    HARNESS_CALLED i32 load_data(const char *path, i32 unused);
    i32 load_std(const char *path);
    i32 on_tick();
    i32 on_draw_03();
    void on_draw_06();
    i32 update_std_vms();
    // 0x40af70
    void draw_layer(i32 layer);

    // These reach the stage through g_Stage.
    HARNESS_CALLED void start_std_vms();
    HARNESS_CALLED void jump_to_label(i32 label);
    HARNESS_CALLED void start_fade_in();
    HARNESS_CALLED void start_fade_out();

    static int __fastcall on_tick_callback(void *arg);
    static int __fastcall on_draw_03_callback(void *arg);
    static int __fastcall on_draw_06_callback(void *arg);
};

extern Stage *g_Stage;
// Another stage that is destroyed the same way (ExpHP: "ANOTHER_STAGE_PTR").
extern Stage *g_Stage2;
