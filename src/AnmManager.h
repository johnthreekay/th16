#pragma once

#include <string.h>

#include "AnmVm.h"
#include "CriticalSections.h"
#include "Thread.h"
#include "ZunMath.h"
#include "decomp.h"
#include "types.h"

// The ANM loader and runtime: loaded files, their sprites and textures, the
// VM pool and lists, and the Direct3D drawing state. Layouts from ExpHP's
// th-re-data.

// The AnmManager::loaded_anms slot each .anm file is loaded into.
enum AnmSlot
{
    ANM_SLOT_TEXT = 0,
    ANM_SLOT_SIG = 1,
    // ascii.anm, ascii_960.anm or ascii_1280.anm by window size.
    ANM_SLOT_ASCII = 2,
    // The stage's own file: 3 for odd stages, 4 for even ones.
    ANM_SLOT_STAGE = 3,
    ANM_SLOT_FRONT = 5,
    ANM_SLOT_STAGE_LOGO = 6,
    ANM_SLOT_BULLET = 7,
    ANM_SLOT_EFFECT = 8,
    ANM_SLOT_PLAYER = 9,
    // The stage's enemy files (SptInf), from 10 on.
    ANM_SLOT_ENEMY_FIRST = 10,
    ANM_SLOT_TITLE = 0x10,
    ANM_SLOT_TITLE_V = 0x11,
    ANM_SLOT_HELP = 0x13,
    // The ending's files, from 20 on.
    ANM_SLOT_ENDING_FIRST = 20,
    ANM_SLOT_SUBSEASON = 0x1e,
    ANM_SLOT_COUNT = 0x1f,
};

// A sprite of a loaded .anm file (ExpHP: zAnmLoadedSprite).
struct AnmLoadedSprite
{
    // The AnmLoaded slot of the file.
    i32 anm_slot;
    // The entry (texture) it is on, in its file and as slot * 256 + entry.
    i32 image_file_num_in_anm;
    i32 image_file_num_in_all;
    // Its rectangle in texture pixels.
    Float2 start_pixel_inclusive;
    Float2 end_pixel_exclusive;
    // The size of the texture as created.
    f32 bitmap_height;
    f32 bitmap_width;
    Float2 uv_start;
    Float2 uv_end;
    // Its size in the entry's own pixels.
    f32 sprite_height;
    f32 sprite_width;
    // Texture pixels per entry pixel (ExpHP: __unknown__usually_1_1): not 1
    // when the texture was created at another size than the entry says.
    Float2 pixel_scale;
};

// AnmLoadedD3D::flags.
enum AnmLoadedD3DFlags
{
    // The texture is a render target (an "@R" entry), released before a
    // device reset and created again after it.
    ANM_D3D_RENDER_TARGET = 1 << 0,
};

// The texture of one entry of a loaded .anm file (ExpHP: zAnmLoadedD3D).
struct AnmLoadedD3D
{
    IDirect3DTexture9 *texture;
    // The image file read by AnmLoaded::load_entry, kept for reloading.
    void *src_data;
    u32 src_data_size;
    i32 bytes_per_pixel;
    // The AnmRawEntry.
    void *entry;
    // AnmLoadedD3DFlags.
    i32 flags;

    // 0x46f490. Fills the top level of the texture with zeroes.
    void clear_texture();
    // Creates the texture as a render target of the given size. The inlined
    // copy of AnmManager::create_render_target (0x46cd80), which LTCG
    // inlined into create_d3d_textures_for_loaded_anms.
    void create_render_target(i32 width, i32 height);
};

// The header of one entry of an .anm file (ExpHP: zAnmRawEntry); the
// pointers are offsets from the header.
struct AnmRawEntry
{
    u32 version;
    u16 num_sprites;
    u16 num_scripts;
    u16 unk_8;
    u16 width;
    u16 height;
    u16 format;
    u32 image_path;
    u16 offset_x;
    u16 offset_y;
    u32 memory_priority;
    u32 texture;
    u8 has_data;
    u8 unk_21;
    u8 low_res_scale;
    u8 unk_23;
    u32 offset_to_next;
    u32 unused[6];
};

// One loaded ANM file: its entries (textures), sprites and scripts, and one
// prototype VM per script that new VMs are copied from.
struct AnmLoaded
{
    i32 slot_num;
    char name[0x104];
    void *anm_file;
    // One prototype VM per script.
    AnmVm *vms;
    i32 entry_count;
    i32 script_count;
    i32 sprite_count;
    AnmLoadedSprite *sprites;
    u8 **scripts;
    // One per entry.
    AnmLoadedD3D *d3d;
    // Nonzero while the textures are still being created: the index (plus
    // one) of the next entry to set up.
    i32 load_wait;
    // Set to have service_pending_loads unload the file.
    i32 unload_requested;
    // Bytes of texture memory the file's textures take.
    i32 texture_memory;
    // Counts VMs created from this file.
    i32 vm_count;
    // Freed by release; nothing in TH16 sets it.
    void *unk_138;

    AnmLoaded()
    {
        memset(this, 0, sizeof(AnmLoaded));
    }

    // Points the VM at a sprite: UVs, size and texture matrices. -1 if the
    // file is not loaded.
    i32 set_sprite(AnmVm *vm, i32 sprite);
    // Resets the VM and points it at a script without running it; -1 (with
    // the VM zeroed) if the script does not exist.
    i32 init_script_vm(AnmVm *vm, i32 script);
    // Starts a script on the VM and runs its first frame; zeroes the VM if
    // the script does not exist or the file is still loading.
    void set_vm_script(AnmVm *vm, i32 script);
    // 0x407b20
    void copy_vm(AnmVm *vm, i32 script);
    // 0x40d460
    void copy_vm_and_run(AnmVm *vm, i32 script);
    // 0x40e5c0. Creates a VM running the script at pos (entity_pos), with
    // the given z rotation, on the given layer unless negative.
    HARNESS_CALLED AnmId create_vm(i32 script, Float3 *pos, f32 rotation, i32 layer, i32 unused);
    // create_vm's body, for the callers LTCG inlined it into (0x426780).
    __forceinline AnmId create_vm_inline(i32 script, Float3 *pos, f32 rotation, i32 layer);
    // 0x406380. Creates a VM running the script at the origin, on the given
    // layer unless negative; also stores the VM in *out if out is not NULL.
    AnmId create_effect(i32 script, i32 layer, AnmVm **out);
    // 0x42c920. Like create_effect, for the UI list.
    HARNESS_CALLED AnmId create_ui_effect(i32 script, i32 unused, AnmVm **out);
    // 0x42efb0. Like create_vm at rotation 0, for the UI list. HelpManual,
    // its only user, passes a constant for unused, which LTCG folds.
    HARNESS_CALLED AnmId create_ui_vm(i32 script, D3DXVECTOR3 *pos, i32 unused);
    // 0x418fe0. create_ui_effect without the out pointer, at the origin.
    // Every caller passes the same unused second argument.
    HARNESS_CALLED AnmId create_ui_vm_at_origin(i32 script, i32 unused);
    // 0x426160. Like create_vm at the origin, but inserted at the front of
    // the world list.
    // Every caller passes 0 for unused, which LTCG folded.
    HARNESS_CALLED AnmId create_vm_front(i32 script, i32 layer, i32 unused);
    // 0x46ed60. A child of parent; mode bits 1 and 2 pick the list (see
    // AnmVm::mode_of_create_child).
    AnmId create_managed_child(i32 script, AnmVm *parent, i32 mode);
    // 0x46eea0. A root VM placed like the given one.
    // AnmVm::run is the only caller, which LTCG folds unused into.
    HARNESS_CALLED AnmId create_managed_root(i32 script, AnmVm *like, i32 unused);

    void init_vm_with_sprite(AnmVm *vm, i32 sprite)
    {
        vm->wipe();
        vm->anm_loaded_index = slot_num;
        set_sprite(vm, sprite);
    }

    // Frees what the file owns (ExpHP: AnmLoaded::destructor). Not a real
    // destructor: callers reload the pointer for the delete that follows,
    // and that delete has no null check of its own.
    void release();
    // 0x46cdd0 (ExpHP: AnmManager::do_load_anm). Reads the file and sizes
    // the tables; 0 on success.
    i32 load(const char *path);
    // 0x46d0c0 (ExpHP: load_one_script). Checks an entry and reads its
    // image file unless the texture is embedded.
    i32 load_entry(i32 index, AnmRawEntry *entry);
    // 0x46d8a0. Stores a sprite and works out its UVs and size.
    void load_sprite(i32 index, AnmLoadedSprite *sprite);
};

// A sprite as stored in an .anm entry.
struct AnmRawSprite
{
    i32 id;
    f32 x;
    f32 y;
    f32 width;
    f32 height;
};

// An embedded texture (THTX), the image of entries with has_data set.
struct AnmRawTexture
{
    char magic[4];
    u16 unk_4;
    i16 format;
    i16 width;
    i16 height;
    u32 size;
    u8 data[1];
};

// Bytes per pixel and Direct3D format of each ANM texture format.
extern i32 g_anm_format_bpp[9];
extern D3DFORMAT g_anm_d3d_formats[9];

// A VM from the manager's preallocated pool (ExpHP: zAnmFastVm).
struct AnmFastVm
{
    AnmVm vm;
    ZunList<AnmFastVm> freelist_node;
    bool is_alive;
    u8 unk_60d[3];
    // Index in the pool; the low 13 bits of the VM's id.
    i32 fast_id;

    // 0x46b770 and 0x46b790, which AnmManager's constructor and destructor
    // pass to the vector constructor and destructor iterators.
    AnmFastVm();
    ~AnmFastVm();
};

// Vertex formats of the batched sprites and primitives (ExpHP:
// zRenderVertex144, zRenderVertex044).
struct RenderVertex144
{
    D3DXVECTOR4 pos;
    D3DCOLOR diffuse;
    Float2 uv;
};

struct RenderVertex044
{
    D3DXVECTOR4 pos;
    D3DCOLOR diffuse;
};

// Untransformed vertices: the 3D sprite quads in the vertex buffer
// (D3DFVF_XYZ | D3DFVF_TEX1) and the vertices of render modes 24 and 25
// (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1).
struct RenderVertexXyzTex
{
    D3DXVECTOR3 pos;
    Float2 uv;
};

struct RenderVertexXyzDiffuseTex
{
    D3DXVECTOR3 pos;
    D3DCOLOR diffuse;
    Float2 uv;
};

// Transformed vertices without a color (D3DFVF_XYZRHW | D3DFVF_TEX1).
struct RenderVertexXyzrhwTex
{
    D3DXVECTOR4 pos;
    Float2 uv;
};

// A request to copy part of the back buffer into the texture of a loaded
// .anm entry (the pause menu's snapshot of the game screen; TH06:
// AnmManager::RequestScreenshot). anm_slot < 0 marks a free entry.
struct AnmScreenCopy
{
    i32 anm_slot;
    i32 entry;
    i32 src_x;
    i32 src_y;
    i32 src_width;
    i32 src_height;
    i32 dst_x;
    i32 dst_y;
    i32 dst_width;
    i32 dst_height;
};

// AnmManager::last_vertex_setup: the vertex format and texture stage
// arguments the drawing code last set up.
enum AnmVertexSetup
{
    // Reset by AnmManager's constructor.
    ANM_VERTEX_SETUP_UNSET = 0,
    // Texture stage arguments from the vertex color (the sprite batch and
    // the primitives).
    ANM_VERTEX_SETUP_DIFFUSE = 1,
    // The vertex buffer's 3D quads (D3DFVF_XYZ | D3DFVF_TEX1), color from
    // the texture factor (draw_3d).
    ANM_VERTEX_SETUP_3D_QUAD = 2,
    // Screen-space textured vertices (D3DFVF_XYZRHW | D3DFVF_DIFFUSE |
    // D3DFVF_TEX1), for draw_vertex_strip and draw_vertex_fan.
    ANM_VERTEX_SETUP_SCREEN_TEXTURED = 3,
    // Untransformed colored vertices (D3DFVF_XYZ | D3DFVF_DIFFUSE |
    // D3DFVF_TEX1), for draw_3d_vertex_strip.
    ANM_VERTEX_SETUP_3D_STRIP = 5,
    // Set by code that draws on its own, so that the next draw sets
    // everything up again.
    ANM_VERTEX_SETUP_NONE = 0xff,
};

// AnmManager::last_color_op: the texture stage color and alpha operations.
enum AnmColorOp
{
    // The vertex color alone (untextured primitives).
    ANM_COLOR_OP_DIFFUSE = 0,
    // Texture times vertex color.
    ANM_COLOR_OP_MODULATE = 1,
    ANM_COLOR_OP_NONE = 0xff,
};

// The flags of AnmManager::render_sprite_2d.
enum AnmSpriteDrawFlags
{
    // Round the corners to pixel centers.
    ANM_SPRITE_SNAP_TO_PIXELS = 1 << 0,
    // Keep the vertex colors already in g_sprite_temp_buffer (fog).
    ANM_SPRITE_KEEP_COLORS = 1 << 1,
};

// Loads the ANM files and runs and draws their VMs: a pool of 0x1fff VMs
// (plus heap ones beyond that), a world list and a UI list ticked every
// frame, per-layer draw lists, and the batching state of the Direct3D
// drawing code. Layout from ExpHP's th-re-data (zAnmManager).
struct AnmManager
{
    ThreadInf thread;
    u8 unk_1c[0x20 - 0x1c];
    // Requests to copy part of the back buffer into a texture, served by
    // take_screenshots.
    AnmScreenCopy screen_copies[4];
    // Per-frame statistics, cleared every frame by GameThread's on_draw:
    // scripts started (AnmLoaded::set_vm_script), render state checks
    // (setup_render_state_for_vm) and draw calls. unk_c4 is only cleared.
    i32 stat_scripts_started;
    i32 unk_c4;
    i32 stat_render_state_setups;
    i32 stat_draw_calls;
    // The active camera's 2D offset (Camera::shake_offset),
    // added to every 2D sprite. Copied by Supervisor::swap_transform_matrices
    // and the stage's draw code.
    Float2 camera_2d_offset;
    // VMs put into the draw lists by the last tick.
    i32 useless_count;
    // VMs created for the game world and for the UI, in tick order.
    ZunList<AnmVm> *world_list_head;
    ZunList<AnmVm> *world_list_tail;
    ZunList<AnmVm> *ui_list_head;
    ZunList<AnmVm> *ui_list_tail;
    AnmFastVm fast_array[0x1fff];
    // Snapshots of VMs (ExpHP: __lolk_*), kept apart from the live ones.
    i32 next_snapshot_fast_id;
    i32 next_snapshot_discriminator;
    ZunList<AnmVm> snapshot_list_head;
    AnmFastVm snapshot_fast_array[0x1fff];
    // Unused entries of fast_array.
    ZunList<AnmFastVm> freelist_head;
    u8 unk_184f4ec[4];
    // Indexed by the slot given to preload_anm.
    AnmLoaded *loaded_anms[0x1f];
    // The world matrix build_world_matrix last made (render modes 5 and 7).
    D3DMATRIX current_world_matrix;
    // Never used (ExpHP: __vm_184f5ac).
    AnmVm unused_vm;
    u8 unk_184fba8[0x184fbac - 0x184fba8];
    // The D3DRS_TEXTUREFACTOR the 3D sprite code last set.
    D3DCOLOR last_texture_factor;
    // The D3D state the sprite code last set, compared before setting it
    // again (so that it only changes, and flushes the batch, when needed).
    // Code that draws without the sprite code resets these so the next
    // sprite sets everything.
    // The texture as AnmLoadedSprite::image_file_num_in_all: the
    // loaded_anms slot times 256 plus the entry. -1 for none.
    i32 last_texture_id;
    // AnmBlendMode.
    u8 last_blend_mode;
    // Reset with the others but never read: leftovers of TH06's color op,
    // z write and vertex shader caches.
    u8 render_cache_184fbb5;
    // AnmVertexSetup.
    u8 last_vertex_setup;
    u8 render_cache_184fbb7;
    u8 render_cache_184fbb8;
    u8 unk_184fbb9;
    u8 last_filter_point;
    // AnmColorOp.
    u8 last_color_op;
    u8 last_address_u;
    u8 last_address_v;
    u8 unk_184fbbe[2];
    // The AnmLoadedSprite (as an integer) whose texture matrix draw_3d last
    // set.
    iptr last_texture_matrix_sprite;
    IDirect3DVertexBuffer9 *vertex_buffer;
    // A unit quad (one corner per entry) that draw_sprite_fog transforms
    // with current_world_matrix to work out each corner's fog.
    RenderVertexXyzTex fog_unit_quad[4];
    // Sprites waiting for flush_sprites, six vertices each (ExpHP:
    // zAnmVertexBuffers).
    i32 unrendered_sprite_count;
    RenderVertex144 sprite_vertex_data[0x20000];
    RenderVertex144 *sprite_write_cursor;
    RenderVertex144 *sprite_render_cursor;
    i32 unrendered_primitive_count;
    RenderVertex044 primitive_vertex_data[0x8000];
    RenderVertex044 *primitive_write_cursor;
    RenderVertex044 *primitive_render_cursor;
    // Heads of the per-layer draw lists (linked through next_in_layer),
    // rebuilt by every tick: 0-35 by tick_world, 36-42 by tick_ui.
    AnmVm layer_list_dummy_heads[0x2b];
    // The upper 19 bits of the next VM id.
    volatile i32 last_discriminator;
    // A color every sprite's color is multiplied with (0x80 meaning 1.0)
    // while global_tint_enabled is set; the stage sets it. Reset to 0x80808080
    // and off by Supervisor::on_draw_01 every frame.
    ZunColor global_tint;
    i32 global_tint_enabled;

    // 0x46a3a0. Clears everything, fills the VM pool's free list and
    // registers the tick and per-layer draw callbacks.
    AnmManager();
    // 0x46b7d0. Destroys every VM still alive.
    ~AnmManager();
    // 0x46b900. Fills the vertex buffer with the unit quad for each of the
    // nine anchorings (draw_3d). Goes through g_AnmManager.
    static void setup_vertex_buffer();

    // Never inlined in the original (over 100 call sites).
    DECOMP_NOINLINE void flush_sprites();
    // 0x468490. Draws a VM by its render mode; -1 if it is not drawn.
    HARNESS_CALLED i32 draw_vm(AnmVm *vm);
    // Sets blending, filtering and texture addressing for a VM, flushing
    // the batch first when they change.
    void setup_render_state_for_vm(AnmVm *vm);
    // Adds a quad (as two triangles) to the sprite batch; 1 if it is full.
    i32 write_sprite(RenderVertex144 *vertices);
    // Empties both vertex batches.
    HARNESS_CALLED void reset_vertex_buffers();
    // 0x466f00. Rebuilds the VM's world matrix (scale, then rotation)
    // unless ANM_VM_KEEP_WORLD_MATRIX, and puts it, moved to the VM's
    // position, in current_world_matrix.
    void build_world_matrix(AnmVm *vm);
    // 0x465280. Draws the quad in g_sprite_temp_buffer for a VM (flags:
    // AnmSpriteDrawFlags).
    i32 render_sprite_2d(AnmVm *vm, i32 flags);
    // 0x4671b0. Render mode 5: build_world_matrix, then the quad already in
    // g_sprite_temp_buffer as a 2D sprite.
    i32 draw_mode_5(AnmVm *vm);
    // 0x466390 (ExpHP: write_sprite_corners__mode_4). Render mode 4:
    // projects the VM's position and writes a camera-facing quad into
    // g_sprite_temp_buffer; -1 if it is outside the depth range. Does not
    // use this.
    static i32 __stdcall write_billboard_corners(AnmVm *vm);
    // 0x466820. Render mode 6: the billboard with distance fog.
    i32 draw_billboard_fog(AnmVm *vm);
    // 0x467200. Render mode 7: the 2D quad with distance fog worked out
    // per corner from current_world_matrix.
    i32 draw_sprite_fog(AnmVm *vm);
    // 0x467410. Render mode 8 (and 15, with fog): a 3D sprite drawn from
    // the vertex buffer with the VM's world and texture matrices.
    i32 draw_3d(AnmVm *vm);
    // 0x467d00 (ExpHP: draw_vm__mode_X__textureArc3D). Render modes 24 and
    // 25: the VM's own vertices as a 3D triangle strip.
    i32 draw_3d_vertex_strip(AnmVm *vm, RenderVertexXyzDiffuseTex *vertices, i32 vertex_count);
    // 0x468350. Draws vertex_count vertices (a triangle fan, in screen
    // space) with the VM's texture and blending.
    i32 draw_vertex_fan(AnmVm *vm, RenderVertex144 *vertices, i32 vertex_count);
    // 0x4681f0 (ExpHP: draw_vm__mode_9__textureCircle). The same as a
    // triangle strip, for visible VMs only.
    i32 draw_vertex_strip(AnmVm *vm, RenderVertex144 *vertices, i32 vertex_count);
    // 0x46efa0
    AnmVm *get_vm_with_id(AnmId id);
    // 0x46f1c0. Marks the VM and its children for deletion. Reaches the
    // manager through g_AnmManager, so LTCG drops the unused this (ExpHP:
    // anm_unload_46f1c0).
    HARNESS_CALLED void delete_vm(AnmId id);
    // 0x46f220. Marks the VM and its descendants for deletion (ExpHP:
    // AnmBehemoth::sub_46f220_recursive). Callers keep values in registers
    // across it, so LTCG has to see the body; never inlined.
    HARNESS_CALLED void mark_tree_for_delete(AnmVm *vm);

    // delete_vm's body. LTCG inlined delete_vm into some callers, with the
    // manager pointer loaded once for several of them.
    void delete_vm_inline(AnmId id)
    {
        AnmVm *vm = get_vm_with_id(id);
        if (vm != NULL && !(vm->flags_hi & ANM_VM_IS_SNAPSHOT))
        {
            vm->mark_for_deletion();
            ZunList<AnmVm> *node = &vm->list_of_children;
            while ((node = node->next) != NULL)
            {
                mark_tree_for_delete(node->entry);
            }
        }
    }

    // 0x440c60. Queues a copy of the back buffer rectangle into an entry's
    // texture; dropped when all four entries are in use.
    DECOMP_NOINLINE i32 queue_screen_copy(i32 anm_slot, i32 entry, i32 src_x, i32 src_y, i32 src_width, i32 src_height,
                          i32 dst_x, i32 dst_y, i32 dst_width, i32 dst_height);
    // 0x440cd0. The same into the sprite a VM shows. Every caller goes
    // through g_AnmManager.
    HARNESS_CALLED i32 copy_screen_to_sprite(AnmId id, i32 src_x, i32 src_y, i32 src_width, i32 src_height);
    // Release the render target textures before the device is reset, and
    // create them again afterwards. Every caller goes through g_AnmManager.
    HARNESS_CALLED void release_textures();
    HARNESS_CALLED void create_d3d_textures_for_loaded_anms();
    // 0x459700. Serves the queued screen copies, once per frame.
    HARNESS_CALLED void take_screenshots();

    // 0x46f270 (ExpHP: AnmBehemoth::disable_vms_from_anm_file). Marks every
    // VM running a script of the file for deletion, before it is unloaded.
    void disable_vms_from_anm_file(AnmLoaded *anm);

    // Members that do not use this; LTCG dropped it (ret N, no ecx).
    // 0x46f0b0. Sends an interrupt to the VM and its direct children.
    DECOMP_NOINLINE static void __stdcall interrupt_tree(AnmId id, i32 interrupt);
    // 0x46f130. Like interrupt_tree, also running each VM once.
    DECOMP_NOINLINE static void __stdcall interrupt_tree_and_run(AnmId id, i32 interrupt);
    // 0x46d020. Loads an .anm file into a slot (or returns the one already
    // there) and waits for the loading thread to create its textures.
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
    // 0x46d990. Renders printf-style text into the VM's texture (the
    // ending and dialogue lines). Variadic, so __cdecl with this pushed
    // first.
    void draw_text(AnmVm *vm, D3DCOLOR color, i32 shadow_color, i32 font, i32 x, i32 spacing, const char *fmt, ...);
    // 0x46dab0 and 0x46dc20. The same right-aligned (ExpHP: draw_rtext) and
    // centered in the sprite; spacing as in draw_text's last argument.
    void draw_text_right(AnmVm *vm, D3DCOLOR color, D3DCOLOR shadow_color, i32 font, i32 spacing,
                         const char *fmt, ...);
    void draw_text_centered(AnmVm *vm, D3DCOLOR color, D3DCOLOR shadow_color, i32 font, i32 spacing,
                            const char *fmt, ...);
    // 0x46cf80. Loads a file into a slot without waiting for its textures.
    AnmLoaded *do_preload_anm(i32 slot, const char *path);
    // 0x46d1c0. Creates the textures of the next entry, or the prototype
    // VMs once all are done.
    static AnmLoaded *__stdcall load_next_entry(AnmLoaded *anm);
    // 0x46d3b0. Creates the texture, sprites and script table of one entry.
    // A member that ignores this, kept alive by load_next_entry rather than
    // /INCLUDE so that it inherits load_next_entry's stack alignment.
    HARNESS_CALLED i32 setup_entry(AnmLoaded *anm, i32 index, i32 first_sprite, i32 first_script,
                                   AnmRawEntry *entry);
    // Texture creation for setup_entry. They return the bytes the texture
    // takes (0 for render targets), or a negative value on failure.
    // 0x46cd80. A render target ("@R" entries).
    static i32 __stdcall create_render_target(AnmLoadedD3D *d3d, i32 width, i32 height);
    // 0x46cd30. An empty texture ("@" entries).
    static i32 __stdcall create_empty_texture(AnmLoadedD3D *d3d, i32 width, i32 height, i32 format);
    // 0x46c920. From the image file read by AnmLoaded::load_entry, cropped
    // to the entry's size. The third argument is the same at every call
    // site; LTCG folded it. A member that does not use this (LTCG dropped
    // it), kept alive by its real caller rather than /INCLUDE: only then does
    // LTCG know its stack is 8-aligned (setup_entry's chain provides it), so
    // convert_texture's needs do not make it realign. As a static it gets
    // register arguments instead.
    HARNESS_CALLED i32 load_texture_from_file(AnmLoadedD3D *d3d, i32 format, i32 unused, i32 width, i32 height,
                                              i32 offset_x, i32 offset_y);
    // 0x46cb60. From a texture embedded in the .anm file.
    static i32 __stdcall load_texture_from_data(AnmLoadedD3D *d3d, AnmRawTexture *raw, i32 format, i32 width,
                                                i32 height);
    // 0x46c0d0. Fixes up the pixels of a freshly loaded texture.
    static void __stdcall convert_texture(IDirect3DTexture9 *texture);
    // Frees ANM files marked for unloading; nonzero while one is still busy.
    // Every caller goes through g_AnmManager (see the list inserts).
    HARNESS_CALLED i32 service_pending_loads();
    // 0x46f600. A VM from the pool, or a new one when the pool is used up.
    // Every caller goes through g_AnmManager (see the list inserts).
    HARNESS_CALLED AnmVm *allocate_vm();
    // 0x46f720. The same for snapshots; hands out the snapshot's id.
    AnmVm *allocate_snapshot_vm(i32 *id);
    // 0x46f810. Copies the VM and its children into snapshots.
    HARNESS_CALLED AnmId store_snapshot_of_vm(AnmVm *vm, AnmVm *parent, i32 unused);
    // 0x46f8f0. Brings a stored snapshot back to life as a new VM tree.
    // Every caller goes through g_AnmManager (see the list inserts).
    HARNESS_CALLED AnmId restore_snapshot(AnmId id);
    // 0x46f970. Copies a snapshot and its children back into live VMs.
    AnmId restore_snapshot_vm(AnmVm *snapshot, AnmVm *parent);
    // 0x46fac0. Writes a VM, its extra data and its children to dst, adding
    // the bytes used to *size.
    HARNESS_CALLED void save_vm_tree(AnmVm *dst, AnmVm *src, i32 *size);
    // 0x46fc30. Reads a tree written by save_vm_tree back into snapshot VMs.
    HARNESS_CALLED AnmId load_vm_tree(AnmVm *src, AnmVm *parent, i32 *size);
    // 0x46e7d0 and the next three. Every caller goes through g_AnmManager,
    // so LTCG replaced this with a load of the global (and kept its stack
    // slot). They hand out the VM's new id.
    HARNESS_CALLED AnmId insert_in_world_list_back(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_world_list_front(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_ui_list_back(AnmVm *vm);
    HARNESS_CALLED AnmId insert_in_ui_list_front(AnmVm *vm);
    // get_vm_with_id for snapshots.
    HARNESS_CALLED AnmVm *get_snapshot_vm_with_id(AnmId id);
    // 0x469890. Draws a triangle fan of count points around center, each
    // offset by offsets[i] and colored colors[i]. Every caller goes through
    // g_AnmManager, so LTCG dropped this.
    HARNESS_CALLED void draw_triangle_fan(i32 count, Float3 *center, Float2 *offsets, ZunColor *colors);
    // 0x469a00. A circle outline of count segments around (x, y), from
    // angle on. draw_vm passes x, y and radius in xmm registers.
    HARNESS_CALLED i32 draw_circle_outline(f32 x, f32 y, f32 radius, f32 angle, i32 count, D3DCOLOR color);
    // 0x468c70. A rotated rectangle at (x, y), anchored by anchor_x and
    // anchor_y (0 center, 1 left/top, 2 right/bottom).
    HARNESS_CALLED i32 draw_rect(f32 x, f32 y, f32 width, f32 height, f32 angle, D3DCOLOR color_1, D3DCOLOR color_2,
                                 i32 anchor_x, i32 anchor_y);
    // 0x468fc0. The outline of draw_rect's rectangle.
    HARNESS_CALLED i32 draw_rect_outline(f32 x, f32 y, f32 width, f32 height, f32 angle, D3DCOLOR color_1,
                                         D3DCOLOR color_2, i32 anchor_x, i32 anchor_y);
    // 0x469570. draw_rect over a half-transparent one pixel border.
    HARNESS_CALLED i32 draw_rect_bordered(f32 x, f32 y, f32 width, f32 height, f32 angle, D3DCOLOR color_1,
                                          D3DCOLOR color_2, i32 anchor_x, i32 anchor_y);
    // 0x469330. A line through (x, y) at angle (anchor: 0 center, 1 start,
    // 2 end). The last argument is the same at every call site; LTCG folded
    // it.
    HARNESS_CALLED i32 draw_line(f32 x, f32 y, f32 length, f32 angle, D3DCOLOR color_1, D3DCOLOR color_2, i32 anchor,
                                 i32 unused);
    // 0x469640. A filled circle of count segments around (x, y), fading
    // from center_color to edge_color.
    HARNESS_CALLED i32 draw_circle(f32 x, f32 y, f32 radius, f32 angle, i32 count, D3DCOLOR center_color,
                                   D3DCOLOR edge_color);
    // 0x469bd0. A ring of count segments, width wide, around (x, y).
    HARNESS_CALLED i32 draw_ring(f32 x, f32 y, f32 radius, f32 width, f32 angle, i32 count, D3DCOLOR color);

    // get_snapshot_vm_with_id as LTCG inlined it into the ANM callbacks.
    AnmVm *get_snapshot_vm_with_id_inline(AnmId id)
    {
        if (id.id == 0)
        {
            return NULL;
        }
        AnmVm *vm = NULL;
        i32 fast_id = id.id & 0x1fff;
        if (fast_id == 0x1fff)
        {
            for (ZunList<AnmVm> *node = &snapshot_list_head; node != NULL; node = node->next)
            {
                if (node->entry->id.id == id.id)
                {
                    vm = node->entry;
                    break;
                }
            }
        }
        else
        {
            vm = &snapshot_fast_array[fast_id].vm;
        }
        return vm;
    }
    // UpdateFunc callbacks that run the VMs of each list and rebuild the
    // per-layer draw lists.
    DECOMP_NOINLINE static i32 __fastcall tick_world(AnmManager *mgr);
    DECOMP_NOINLINE static i32 __fastcall tick_ui(AnmManager *mgr);
    // The tick callbacks (priorities 0x21 and 9): tick_world, skipped while
    // the game is paused with the world frozen, and tick_ui.
    static i32 __fastcall on_tick_21_world(AnmManager *mgr);
    static i32 __fastcall on_tick_09_ui(AnmManager *mgr);
    // Moves the VM and its children onto delete_list, once each.
    void remove_tree(AnmVm *vm, ZunList<AnmVm> *delete_list);
    // 0x46eab0. Unlinks a VM and returns it to the pool or frees it.
    i32 destroy_possibly_managed_vm(AnmVm *vm);
    // 0x46ec90. The same for snapshots.
    i32 destroy_possibly_managed_snapshot_vm(AnmVm *vm);
    // 0x46e750. Draws the VMs of one layer; returns 1 for the callbacks.
    i32 render_layer(i32 layer);
    // UpdateFunc callbacks that draw one layer each, named after their
    // priority.
    static int __fastcall on_draw_05_layer_00(AnmManager *mgr);
    static int __fastcall on_draw_0a_layer_03(AnmManager *mgr);
    static int __fastcall on_draw_2d_layer_20(AnmManager *mgr);
    static int __fastcall on_draw_3a_layer_24(AnmManager *mgr);
    static int __fastcall on_draw_40_layer_28(AnmManager *mgr);
    static int __fastcall on_draw_37_layer_36(AnmManager *mgr);
    static int __fastcall on_draw_41_layer_39(AnmManager *mgr);
    static int __fastcall on_draw_07_layer_01(AnmManager *mgr);
    static int __fastcall on_draw_09_layer_02(AnmManager *mgr);
    static int __fastcall on_draw_0b_layer_04(AnmManager *mgr);
    static int __fastcall on_draw_0d_layer_05(AnmManager *mgr);
    static int __fastcall on_draw_10_layer_06(AnmManager *mgr);
    static int __fastcall on_draw_12_layer_07(AnmManager *mgr);
    static int __fastcall on_draw_14_layer_08(AnmManager *mgr);
    static int __fastcall on_draw_15_layer_09(AnmManager *mgr);
    static int __fastcall on_draw_16_layer_10(AnmManager *mgr);
    static int __fastcall on_draw_18_layer_11(AnmManager *mgr);
    static int __fastcall on_draw_1c_layer_13(AnmManager *mgr);
    static int __fastcall on_draw_1f_layer_14(AnmManager *mgr);
    static int __fastcall on_draw_20_layer_15(AnmManager *mgr);
    static int __fastcall on_draw_22_layer_16(AnmManager *mgr);
    static int __fastcall on_draw_24_layer_17(AnmManager *mgr);
    static int __fastcall on_draw_27_layer_18(AnmManager *mgr);
    static int __fastcall on_draw_1b_layer_12(AnmManager *mgr);
    static int __fastcall on_draw_2a_layer_19(AnmManager *mgr);
    static int __fastcall on_draw_2e_layer_21(AnmManager *mgr);
    static int __fastcall on_draw_34_layer_22(AnmManager *mgr);
    static int __fastcall on_draw_36_layer_23(AnmManager *mgr);
    static int __fastcall on_draw_4f_layer_30(AnmManager *mgr);
    static int __fastcall on_draw_52_layer_31(AnmManager *mgr);
    static int __fastcall on_draw_4d_layer_29(AnmManager *mgr);
    static int __fastcall on_draw_3d_layer_26(AnmManager *mgr);
    static int __fastcall on_draw_3e_layer_27(AnmManager *mgr);
    static int __fastcall on_draw_3b_layer_25(AnmManager *mgr);
    static int __fastcall on_draw_3c_layer_37(AnmManager *mgr);
    static int __fastcall on_draw_3f_layer_38(AnmManager *mgr);
    static int __fastcall on_draw_4e_layer_40(AnmManager *mgr);
    static int __fastcall on_draw_50_layer_41(AnmManager *mgr);
    static int __fastcall on_draw_53_layer_42(AnmManager *mgr);

    // 0x46d720. unload_anm as LTCG kept it out of line (an ECL instruction
    // and the ending, which checks for a negative slot itself).
    void unload_anm_out_of_line(i32 slot);
    // 0x46c8b0. Loads an image file in memory into the top level of an
    // existing texture. The last three arguments are the same at every call
    // site; LTCG folded them. Does not use this.
    HARNESS_CALLED i32 reload_texture(AnmLoadedD3D *d3d, void *data, u32 size, i32 unused_3, i32 unused_4,
                                       i32 unused_5);

    // Frees the ANM file in a slot, if one is loaded there.
    void unload_anm(i32 slot)
    {
        if (slot < 0 || slot >= sizeof(loaded_anms) / sizeof(loaded_anms[0]))
        {
            return;
        }
        if (loaded_anms[slot] != NULL)
        {
            loaded_anms[slot]->release();
            delete loaded_anms[slot];
            loaded_anms[slot] = NULL;
        }
    }
};

extern AnmManager *g_AnmManager;

__forceinline AnmId AnmLoaded::create_vm_inline(i32 script, Float3 *pos, f32 rotation, i32 layer)
{
    ENTER_CS(CS_ANM_MANAGER);
    vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (layer >= 0)
    {
        vm->layer = layer;
        if (layer <= ANM_LAYER_HUD_LAST)
        {
            vm->flags_hi &= ~ANM_VM_ORIGIN_HUD;
            vm->flags_hi |= ANM_VM_ORIGIN_GAME;
        }
    }
    if (pos == NULL)
    {
        vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        vm->entity_pos = *pos;
    }
    vm->rotation.z = rotation;
    vm->run();
    vm->mode_of_create_child = ANM_CREATE_WORLD_BACK;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_back(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// The quad being built by the draw functions.
extern RenderVertex144 g_sprite_temp_buffer[4];
// The unit quads AnmManager's constructor and setup_vertex_buffer fill in
// (TH06: g_PrimitivesToDrawVertexBuf, g_PrimitivesToDrawUnknown).
extern RenderVertexXyzrhwTex g_unit_quad_rhw[4];
extern RenderVertexXyzDiffuseTex g_unit_quad_xyz[4];

// Deletes the VM (if still alive) and forgets the id.
inline void delete_vm_and_clear(AnmId &id)
{
    g_AnmManager->delete_vm(id);
    id.id = 0;
}

// The same with delete_vm inlined, as LTCG does in some loops.
inline void delete_vm_inline_and_clear(AnmId &id)
{
    g_AnmManager->delete_vm_inline(id);
    id.id = 0;
}

// Looks the VM up and forgets the id if it is gone.
// The VM with the id, or NULL; the id is kept either way. Like
// get_vm_or_clear, an inline node between the caller and get_vm_with_id,
// which keeps /GS cookies away from callers (README).
inline AnmVm *get_vm(AnmId id)
{
    return g_AnmManager->get_vm_with_id(id);
}

inline AnmVm *get_vm_or_clear(AnmId &id)
{
    AnmVm *vm = g_AnmManager->get_vm_with_id(id);
    if (vm == NULL)
    {
        id.id = 0;
    }
    return vm;
}

#ifdef TH16_PORT
// MSVC binds the non-const reference above to a temporary id
// (get_vm_or_clear(find_child_id(...))); standard C++ needs this overload.
inline AnmVm *get_vm_or_clear(AnmId &&id)
{
    return get_vm_or_clear(id);
}
#endif

// The first descendant of the VM running the script, or NULL if the VM is
// gone (forgetting the id then).
inline AnmVm *find_child_of(AnmId &id, i32 script)
{
    if (get_vm_or_clear(id) == NULL)
    {
        return NULL;
    }
    return get_vm_or_clear(id)->search_children(script, 0);
}
