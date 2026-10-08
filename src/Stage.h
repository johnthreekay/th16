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

// Interpolated camera fog (ExpHP: zInterpCameraSky). Methods are the
// interpolation modes of InterpFloat3, with 7 (add goal each frame), 8
// (Bezier) and 17 (accelerate) handled here.
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

    // 0x40cd10. Advances the interpolation and returns the current value.
    CameraSky step();
};

// One ANM quad of an STD object. vm_index is filled in at load time.
struct StdQuad
{
    // Negative ends the list; only type 0 is drawn.
    i16 type;
    i16 size;
    i16 script;
    i16 vm_index;
    // Offset from the instance, and the size to scale the sprite to (0
    // keeps the sprite's own).
    D3DXVECTOR3 pos;
    f32 width;
    f32 height;
};

// An object of the STD file: a list of quads with a bounding box.
struct StdObject
{
    u8 unk_0[2];
    // Stage::draw_layer draws the object with its layer.
    i8 layer;
    // Bit 0: its VMs are still running. Bit 1: drawn at least once.
    u8 flags;
    // The bounding box: its center and size.
    D3DXVECTOR3 center;
    D3DXVECTOR3 size;
    StdQuad quads[1];

    // 0x40a7d0. Whether the object, placed at pos, is too far from the
    // camera or projects entirely outside the arcade region.
    HARNESS_CALLED i32 is_culled(D3DXVECTOR3 *pos, f32 max_distance_sq, Camera *camera);
};

// A placement of an object; a negative object_id ends the list.
struct StdInstance
{
    i16 object_id;
    // Bit 0: drawn this frame.
    u16 flags;
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

// STD instruction opcodes. Names from truth's TH14-TH17 stdmap where it has
// one.
enum StdOpcode
{
    // The script stops here for good.
    STD_STOP = 0,
    // Offset, new time.
    STD_JMP = 1,
    STD_POS = 2,
    STD_POS_TIME = 3,
    STD_FACING = 4,
    STD_FACING_TIME = 5,
    STD_UP = 6,
    STD_FOV = 7,
    // Color, begin and end distance.
    STD_FOG = 8,
    STD_FOG_TIME = 9,
    STD_POS_BEZIER = 10,
    STD_FACING_BEZIER = 11,
    // StageInner::rocking_mode().
    STD_ROCKING_MODE = 12,
    STD_BG_COLOR = 13,
    // Replaces one of the eight extra VMs (-1 stops it, -2 hides it).
    STD_SPRITE = 14,
    // A label for Stage::jump_to_label; a no-op when run.
    STD_INTERRUPT_LABEL = 16,
    // Replaces the distortion mesh at the bottom of the screen.
    STD_DISTORTION = 17,
    STD_UP_TIME = 18,
    // Unnamed in truth: sends interrupt 7 + n to every VM of the stage.
    STD_INTERRUPT = 19,
    // Unnamed in truth: how far objects are drawn.
    STD_DRAW_DISTANCE = 20,
};

// The STD file's header; the objects' offsets follow.
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
    // Its low byte is the camera rocking pattern (rocking_mode()).
    i32 rocking_mode_word;
    // Time in the rocking pattern's cycle.
    ZunTimer rocking_timer;
    InterpFloat3 camera_facing_i;
    InterpFloat3 camera_pos_i;
    InterpFloat3 camera_up_i;
    InterpCameraSky camera_sky_i;
    Camera camera;
    Stage *stage;
    // The extra VMs of STD_SPRITE, and the layer each one draws on.
    AnmVm anm_vms[8];
    i32 anm_vm_layers[8];
    // The squared distance beyond which objects are not drawn
    // (STD_DRAW_DISTANCE; 3100 squared at load time).
    f32 draw_distance_sq;
    // The distortion mesh of STD_DISTORTION.
    Fog *fog;
    // STD_DISTORTION kind 2's radius shrinks from 192 to 112, 2 per frame.
    f32 distortion_min_radius;
    f32 distortion_radius;
    // Set to -1 by STD_DISTORTION; not read.
    i32 unk_3320;
    ZunTimer fog_timer;
    // Angles that step_fog advances to wave the mesh.
    f32 wave_angle_a;
    f32 wave_angle_b;
    // STD_DISTORTION's argument: 1 waves the bottom of the screen (7 points
    // per strip), 2 bulges a disc in the middle (17).
    i32 fog_kind;
    // Color passed to the ANM manager; the top byte flags a new value.
    union
    {
        u32 anm_color;
        struct
        {
            u8 anm_color_rgb[3];
            u8 color_changed;
        };
    };

    // Only destroys the VMs; Stage's unwind code calls it out of line.
    ~StageInner();

    // 0x409490. Starts interpolating the fog from its current value.
    void set_sky_interp(i32 end_time, i32 method, CameraSky *goal);
    // 0x40b3b0. Runs the instructions whose time has come, advances the
    // time, steps the camera interpolators and rocks the camera.
    i32 run_std();
    // The camera rocking pattern (STD_ROCKING_MODE): 0 for none.
    u8 &rocking_mode()
    {
        return *(u8 *)&rocking_mode_word;
    }
    // 0x40c4a0
    void step_fog();
    // 0x40c280. Draws the VMs set for a layer with camera 3, without fog
    // and depth writes.
    void draw_vms(i32 layer);
};

// Stage::stage_flags. When the game goes on to the next stage, the old
// stage exits (the screen fades out over 30 frames, then it is disabled
// and deleted) while the new one enters (hidden for 30 frames, then
// fading in).
enum StageFlags
{
    // The objects are drawn (not while a spell card's background is up).
    STAGE_VISIBLE = 1 << 0,
    // Exiting (fade_timer counts down from 30).
    STAGE_EXITING = 1 << 1,
    // Entering (fade_timer counts down from 60).
    STAGE_ENTERING = 1 << 2,
    // Stops ticking and drawing.
    STAGE_DISABLED = 1 << 3,
};

// The stage background and its script.
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
    // Counted by draw_layer each frame: instances drawn, instances culled
    // and quads drawn.
    i32 instances_drawn;
    i32 instances_culled;
    i32 quads_drawn;
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
    DECOMP_NOINLINE i32 on_draw_03();
    DECOMP_NOINLINE i32 on_draw_06();
    i32 update_std_vms();
    // 0x40af70
    i32 draw_layer(i32 layer);
    // Makes camera 3 a copy of the stage's camera (keeping camera 3's
    // shake offset) and applies it.
    void use_camera();
    // 0x40b2f0. Sends interrupt n to every quad VM and the stage's own VMs
    // and runs them (STD_INTERRUPT).
    void interrupt_vms(i32 n);

    // These reach the stage through g_Stage.
    HARNESS_CALLED static void start_std_vms();
    HARNESS_CALLED void jump_to_label(i32 label);
    HARNESS_CALLED void start_exit();
    HARNESS_CALLED void start_enter();

    static int __fastcall on_tick_callback(void *arg);
    static int __fastcall on_draw_03_callback(void *arg);
    static int __fastcall on_draw_06_callback(void *arg);
};

extern Stage *g_Stage;
// The previous stage while the next one starts (ExpHP: "ANOTHER_STAGE_PTR").
extern Stage *g_Stage2;
