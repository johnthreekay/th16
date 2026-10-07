#pragma once

#include <stdlib.h>

#include <d3d9.h>
#include <d3dx9math.h>

#include "Interp.h"
#include "ZunList.h"
#include "ZunMath.h"
#include "ZunTimer.h"
#include "decomp.h"
#include "types.h"

struct AnmVm;

// Handle to a running VM, 0 when none. A class in ZUN's code: it is returned
// through a hidden pointer and constructed to 0 before its owner's memset.
struct AnmId
{
    i32 id;

    AnmId()
    {
        id = 0;
    }

    // 0x46f2e0 (ExpHP: anm_find_existing_or_clear_id). Looks the VM up and
    // forgets the id if it is gone.
    AnmVm *find_or_clear();
    // 0x46f300 and 0x46f340. AnmVm::set/clear_flag_lo_2_tree.
    void set_flag_lo_2_tree();
    void clear_flag_lo_2_tree();
    // 0x46f3e0
    void set_entity_pos(D3DXVECTOR3 *pos);
    // 0x46f440. Stops the VM and replaces it with a new effect VM running
    // the given script of the same file.
    void replace_with_effect(i32 script);
    // 0x46f5a0. The id of a descendant found by AnmVm::search_children.
    HARNESS_CALLED AnmId search_children(i32 script, i32 nth);
};

// A D3DCOLOR whose channels can be reached one by one.
union ZunColor
{
    D3DCOLOR d3d;
    struct
    {
        u8 b;
        u8 g;
        u8 r;
        u8 a;
    };
};

enum AnmVmFlagsLo
{
    ANM_VM_VISIBLE = 1 << 0,
    // Set and cleared for a whole tree by ANM instruction 316 (ExpHP).
    ANM_VM_FLAG_LO_2 = 1 << 1,
    // Rotation or scale changed; the matrix needs a rebuild.
    ANM_VM_ROTATION_CHANGED = 1 << 2,
    ANM_VM_SCALE_CHANGED = 1 << 3,
    ANM_VM_UV_SCALE_CHANGED = 1 << 4,
    // Four bits of blend mode (AnmManager::setup_render_state_for_vm).
    ANM_VM_BLEND_MODE_SHIFT = 5,
    ANM_VM_BLEND_MODE_MASK = 0xf << 5,
    // pos_i moves pos_2 instead of pos.
    ANM_VM_POS_I_TO_POS_2 = 1 << 10,
    ANM_VM_FLAG_LO_800 = 1 << 11,
    ANM_VM_FLAG_LO_1000 = 1 << 12,
    // Two bits: which of color_1/color_2 to draw with (set_rgb2_time and
    // set_alpha2_time switch to 1, color_2).
    ANM_VM_COLOR_MODE_MASK = 3 << 17,
    ANM_VM_COLOR_MODE_1 = 1 << 17,
    // Five bits of render mode (how the sprite is projected and drawn).
    ANM_VM_RENDER_MODE_SHIFT = 25,
    // Two bits of texture addressing along v: wrap, clamp, mirror.
    ANM_VM_ADDRESS_V_SHIFT = 30,
};

enum AnmVmFlagsHi
{
    // Two bits of texture addressing along u: wrap, clamp, mirror.
    ANM_VM_ADDRESS_U_MASK = 3 << 0,
    // Point instead of linear filtering.
    ANM_VM_FILTER_POINT_SHIFT = 11,
    // Rotate the sprite to the owner's movement angle.
    ANM_VM_AUTO_ROTATE = 1 << 7,
    ANM_VM_CREATED_BY_GAME = 1 << 10,
    ANM_VM_FLAG_HI_4000 = 1 << 14,
    ANM_VM_FLAG_HI_8000 = 1 << 15,
    // world_pos and get_slowdown_factor stop walking up the parents at a VM
    // with this.
    ANM_VM_NO_PARENT_POS = 1 << 16,
    // Two bits of layer kind: LAYER_SET for layers 3-19, LAYER_UI for
    // layers 20-23, neither for the rest.
    ANM_VM_LAYER_SET = 1 << 18,
    ANM_VM_LAYER_UI = 1 << 19,
    ANM_VM_LAYER_KIND_MASK = ANM_VM_LAYER_SET | ANM_VM_LAYER_UI,
    // Three bits; set_layer sets it to 1 for layers 20-31 and 36-42.
    ANM_VM_COORD_MODE_MASK = 7 << 20,
    ANM_VM_COORD_MODE_1 = 1 << 20,
    ANM_VM_ROTATE_WITH_PARENT = 1 << 23,
    // Marked for deletion: the manager frees it on its next pass.
    ANM_VM_DELETE_PENDING = 1 << 5,
    ANM_VM_FLAG_HI_40 = 1 << 6,
    // Inherited from the parent by managed children.
    ANM_VM_FLAG_HI_2000000 = 1 << 25,
    // A copy kept by AnmManager::store_snapshot_of_vm, not a live VM;
    // deleting it does nothing.
    ANM_VM_FLAG_HI_4000000 = 1 << 26,
};

// Variable numbers in ANM script arguments (names after ExpHP's truth).
enum AnmVar
{
    ANM_VAR_I0 = 10000,
    ANM_VAR_I1 = 10001,
    ANM_VAR_I2 = 10002,
    ANM_VAR_I3 = 10003,
    ANM_VAR_F0 = 10004,
    ANM_VAR_F1 = 10005,
    ANM_VAR_F2 = 10006,
    ANM_VAR_F3 = 10007,
    ANM_VAR_I4 = 10008,
    ANM_VAR_I5 = 10009,
    ANM_VAR_RANDRAD_UNSAFE = 10010,
    ANM_VAR_RANDF_UNSAFE = 10011,
    ANM_VAR_RANDF2_UNSAFE = 10012,
    ANM_VAR_POS_X = 10013,
    ANM_VAR_POS_Y = 10014,
    ANM_VAR_POS_Z = 10015,
    ANM_VAR_CAMERA_X = 10016,
    ANM_VAR_CAMERA_Y = 10017,
    ANM_VAR_CAMERA_Z = 10018,
    ANM_VAR_CAMERA_FACING_X = 10019,
    ANM_VAR_CAMERA_FACING_Y = 10020,
    ANM_VAR_CAMERA_FACING_Z = 10021,
    ANM_VAR_RAND_UNSAFE = 10022,
    ANM_VAR_ROT_X = 10023,
    ANM_VAR_ROT_Y = 10024,
    ANM_VAR_ROT_Z = 10025,
    // z rotation including every parent's.
    ANM_VAR_TOTAL_ROT_Z = 10026,
    ANM_VAR_RAND_SCALE_ONE = 10027,
    ANM_VAR_RAND_SCALE_PI = 10028,
    ANM_VAR_NUM_CYCLES = 10029,
    ANM_VAR_RANDRAD_SAFE = 10030,
    ANM_VAR_RANDF_SAFE = 10031,
    ANM_VAR_RANDF2_SAFE = 10032,
    ANM_VAR_F4 = 10033,
    ANM_VAR_F5 = 10034,
    ANM_VAR_F6 = 10035,
};

struct AnmVm;

// Script callbacks, selected per VM by the index_of_* fields.
typedef i32(__fastcall *AnmVmSwitchFunc)(AnmVm *vm, i32 interrupt);
extern AnmVmSwitchFunc g_anm_on_switch_funcs[4];
typedef i32(__fastcall *AnmVmFunc)(AnmVm *vm);
extern AnmVmFunc g_anm_on_destroy_funcs[4];
// Called with the copy, the original and an extra argument when a VM with
// extra data is copied (ExpHP: ANM_ON_COPY_FUNC_2).
typedef i32(__fastcall *AnmVmCopyFunc)(AnmVm *vm, const AnmVm *other, i32 arg);
extern AnmVmCopyFunc g_anm_on_copy_funcs[2];

// One running ANM script (layout: ExpHP's zAnmVm, flattened, 0x5fc bytes).
struct AnmVm
{
    ZunTimer interrupt_return_time;
    i32 interrupt_return_offset;
    i32 layer;
    i32 anm_loaded_index;
    i32 sprite_id;
    i32 script_id;
    i32 instr_offset;
    Float3 pos;
    Float3 rotation;
    Float3 angular_velocity;
    Float2 scale;
    Float2 scale_2;
    Float2 scale_growth;
    Float2 uv_scale;
    Float2 sprite_size;
    Float2 uv_scroll_pos;
    Float2 anchor_offset;
    u8 unk_88[4];
    InterpFloat3 pos_i;
    InterpInt3 rgb1_i;
    InterpInt alpha1_i;
    InterpFloat3 rotate_i;
    InterpAngle rotate_2d_i;
    InterpFloat2 scale_i;
    InterpFloat2 op_434_i;
    InterpFloat2 uv_scale_i;
    InterpInt3 rgb2_i;
    InterpInt alpha2_i;
    InterpFloat u_vel_i;
    InterpFloat v_vel_i;
    Float2 uv_quad_of_sprite[4];
    Float2 uv_scroll_vel;
    D3DXMATRIX matrix_3d0;
    D3DXMATRIX matrix_410;
    D3DXMATRIX matrix_450;
    i32 pending_interrupt;
    i32 time_of_last_sprite_set;
    i32 unk_498;
    i16 unk_49c;
    u8 unk_49e[2];
    // ANM script variables (ExpHP: int_script_vars, float_script_vars).
    i32 int_vars[4];
    f32 float_vars[4];
    Float3 script_vars_33_34_35;
    i32 script_var_8;
    i32 script_var_9;
    f32 rand_scale_one;
    f32 rand_scale_pi;
    i32 num_cycles_in_texture;
    Float3 pos_2;
    Float3 last_rendered_quad_in_surface_space[4];
    i32 mode_of_create_child;
    ZunColor color_1;
    ZunColor color_2;
    ZunColor mixed_inherited_color;
    u8 font_dims[2];
    u8 unk_52e[2];
    u32 flags_lo;
    u32 flags_hi;
    // Suffix (ExpHP: zAnmVmSuffix).
    AnmId id;
    u32 fast_id;
    ZunTimer script_time;
    ZunTimer timer_1c;
    ZunList<AnmVm> node_in_global_list;
    ZunList<AnmVm> node_as_child;
    ZunList<AnmVm> list_of_children;
    ZunList<AnmVm> unk_list_598;
    AnmVm *next_in_layer;
    // ExpHP: __root_vm__or_maybe_not.
    AnmVm *parent;
    // ExpHP: parent_vm.
    AnmVm *unk_5b0;
    f32 slowdown;
    // Allocated by ANM instruction 508.
    void *ins_508_extra_data;
    u32 ins_508_extra_data_size;
    i32 index_of_on_wait;
    i32 index_of_on_tick;
    i32 index_of_on_draw;
    i32 index_of_on_destroy;
    i32 index_of_on_interrupt;
    i32 index_of_on_copy_1;
    i32 index_of_on_copy_2;
    i32 index_of_sprite_mapping_func;
    // Position of the game object the VM belongs to.
    Float3 entity_pos;
    void *associated_game_entity;
    Float3 rotation_related;

    // Callers compile with EH cleanup for this, which LTCG then removes
    // because it sees the body cannot throw.
    DECOMP_NOINLINE AnmVm();
    // Inlined into every owner's destructor; the original also has an
    // out-of-line copy at 0x4093b0.
    ~AnmVm();
    // Resets the VM, keeping layer, fast_id and entity_pos (ExpHP:
    // AnmVm::initialize).
    void wipe();
    // 0x40e490. Position including entity_pos and every parent's.
    Float3 world_pos();
    // 0x45f980. Nonzero once the script has ended (anm_effect_1_on_tick
    // counts on it).
    i32 run();
    HARNESS_CALLED f32 get_slowdown_factor();
    void alloc_extra_data(u32 size);
    void set_layer(i32 layer);
    DECOMP_NOINLINE void set_alpha1_time(i32 end_time, i32 method, u8 initial, u8 goal);
    // Clears the suffix except for the fields that identify the VM.
    void wipe_suffix();
    // Switches to another sprite of the same file, changing only the UVs.
    void set_sprite_uvs(i32 sprite);
    // ECL's anm instructions: interpolate from the current value to a goal.
    // 0x425e70
    void fade_alpha1(i32 end_time, i32 method, u8 goal);
    // 0x425dd0
    void fade_alpha2(i32 end_time, i32 method, u8 goal);
    // 0x425f10
    void fade_rgb1(i32 end_time, i32 method, ZunColor *goal);
    // 0x426020. LTCG passes x in xmm3.
    HARNESS_CALLED void scale_to(i32 end_time, i32 method, f32 x, f32 y);
    // 0x406a70. Scales a position by the screen scale and applies the
    // parents' rotation.
    Float3 *transform_coords(Float3 *pos);
    // 0x406c40
    void get_own_transformed_pos(Float3 *out);
    // 0x46f510. The nth descendant (depth first) running the given script
    // (unk_49c; -1 for any).
    AnmVm *search_children(i32 script, i32 nth);
    // 0x46f380 and 0x46f3b0 (ExpHP: set/clear_ins_316_flag_recursively).
    HARNESS_CALLED void set_flag_lo_2_tree();
    HARNESS_CALLED void clear_flag_lo_2_tree();

    // The two above with their first level inlined, as LTCG did in some
    // callers.
    void set_flag_lo_2_tree_inline()
    {
        flags_lo |= ANM_VM_FLAG_LO_2;
        for (ZunList<AnmVm> *node = list_of_children.next; node != NULL; node = node->next)
        {
            node->entry->set_flag_lo_2_tree();
        }
    }

    void clear_flag_lo_2_tree_inline()
    {
        flags_lo &= ~ANM_VM_FLAG_LO_2;
        for (ZunList<AnmVm> *node = list_of_children.next; node != NULL; node = node->next)
        {
            node->entry->clear_flag_lo_2_tree();
        }
    }

    // 0x406240. Starts moving pos_i along a bezier curve.
    void set_pos_bezier(i32 end_time, Float3 *initial, Float3 *bezier_1, Float3 *goal, Float3 *bezier_2);
    // 0x447550. Starts interpolating the scale from initial to goal.
    void set_scale_interp(i32 end_time, i32 method, D3DXVECTOR2 *initial, D3DXVECTOR2 *goal);
    // Script argument lookups: a variable number (AnmVar) gives the
    // variable, anything else is returned as is.
    HARNESS_CALLED f32 get_float_var(f32 value);
    i32 get_int_var(i32 value);
    f32 *get_float_var_ptr(f32 *value);
    i32 *get_int_var_ptr(i32 *value);
    // Rotation plus every parent's, in rotation_related. Wraps this VM's
    // own rotation into [-pi, pi] on the way.
    Float3 *get_total_rotation();
    // Start interpolators (ExpHP's names; rgb1/rgb2 are swapped there).
    void set_uv_scale_time(i32 end_time, i32 method, Float2 *initial, Float2 *goal);
    void set_434_time(i32 end_time, i32 method, Float2 *initial, Float2 *goal);
    void set_alpha2_time(i32 end_time, i32 method, u8 initial, u8 goal);
    void set_rgb2_time(i32 end_time, i32 method, ZunColor *initial, ZunColor *goal);
    void set_rgb1_time(i32 end_time, i32 method, ZunColor *initial, ZunColor *goal);
    // Advances every running interpolator and applies its value.
    void step_interpolators();
    // Applies angular velocity, scale growth and UV scrolling for one frame
    // (ExpHP: leaf_4630f0__flag_534_24_only).
    void step_velocities();
    // Screen positions of the sprite's corners, by render mode.
    void write_sprite_corners(Float3 *corners);
    // 0x465c40, 0x4660b0
    static void __stdcall write_sprite_corners__without_rot(AnmVm *vm, Float3 *a, Float3 *b, Float3 *c,
                                                            Float3 *d);
    static void __stdcall write_sprite_corners__with_z_rot(AnmVm *vm, Float3 *a, Float3 *b, Float3 *c, Float3 *d);

    void interrupt(i32 n)
    {
        if (index_of_on_interrupt != 0)
        {
            g_anm_on_switch_funcs[index_of_on_interrupt](this, n);
        }
        pending_interrupt = n;
    }

    // 0x4174d0. interrupt, as LTCG kept it out of line for most callers
    // (bullets, the HUD, the music room).
    DECOMP_NOINLINE void interrupt_out_of_line(i32 n);

    void mark_for_deletion()
    {
        flags_hi &= ~ANM_VM_FLAG_HI_40;
        flags_hi |= ANM_VM_DELETE_PENDING;
    }

    // 0x46f410. set_sprite through the file the VM came from.
    void set_sprite(i32 sprite);
    // 0x46fd50 (ExpHP: AnmVm::constructor(const AnmVm&, int)). Copies
    // another VM's state, but not its place in any list.
    void copy_from(const AnmVm &other, i32 arg);
};

// out = in / (640, 480), clamped at 0.
void LTCG_FASTCALL divide_vec2_by_640_480(Float2 *out, Float2 *in);
