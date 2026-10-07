#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Stage.h"

#include "Ecl.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "Globals.h"
#include "Rng.h"
#include "ScreenEffect.h"
#include "Spellcard.h"
#include "Supervisor.h"
#include "ZunAngle.h"

// GLOBAL: TH16 0x4a6da0
Stage *g_Stage;

// GLOBAL: TH16 0x4a6d9c
Stage *g_Stage2;

// FUNCTION: TH16 0x409490
void StageInner::set_sky_interp(i32 end_time, i32 method, CameraSky *goal)
{
    camera_sky_i.end_time = end_time;
    camera_sky_i.method = method;
    camera_sky_i.initial = camera.sky;
    camera_sky_i.goal = *goal;
    camera_sky_i.time.reset();
}

// SYNTHETIC: TH16 0x409d90
// Fog::`scalar deleting destructor'

// FUNCTION: TH16 0x409550
Fog::~Fog()
{
    delete_vm_and_clear(main_vm);
    if (buffer_14 != NULL)
    {
        free(buffer_14);
        buffer_14 = NULL;
    }
    if (buffer_18 != NULL)
    {
        free(buffer_18);
        buffer_18 = NULL;
    }
    for (i32 i = 0; i < vm_count - 1; i++)
    {
        delete_vm_inline_and_clear(vm_ids[i]);
    }
    if (vm_ids != NULL)
    {
        free(vm_ids);
        vm_ids = NULL;
    }
    if (vms != NULL)
    {
        free(vms);
        vms = NULL;
    }
}

// FUNCTION: TH16 0x409670
Stage::Stage()
{
    memset(this, 0, sizeof(Stage));
    flags |= 2;
}

// FUNCTION: TH16 0x409770
StageInner::~StageInner()
{
}

// TODO: ours saves esi/edi after the load_std check (shrink-wrapped); the
// original saves them in the prologue. Matches once GameThread::thread_start
// realigns like the original (tested with a stand-in double there).
// FUNCTION: TH16 0x4097c0
HARNESS_CALLED i32 Stage::load_data(const char *path, i32 unused)
{
    g_Stage = this;
    stage_num = g_Globals.stage_num;
    if (load_std(path) != 0)
    {
        // ステージデータが読み込めません。データが壊れています
        g_GameErrorContext.fatal("\x83X\x83" "e\x81[\x83W\x83" "f\x81[\x83^\x82\xaa\x93\xc7\x82\xdd\x8d\x9e\x82\xdf"
                                 "\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4"
                                 "\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    inner.stage = this;
    inner.camera = g_Supervisor.cameras[3];
    inner.camera.position = D3DXVECTOR3(0.0f, 0.0f, -600.0f);
    inner.camera.facing = D3DXVECTOR3(0.0f, 300.0f, 600.0f);
    inner.camera.up = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
    inner.camera.rocking_vector_1 = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    inner.camera.rocking_vector_2 = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    inner.unk_3310 = 9610000.0f;

    UpdateFunc *f = g_UpdateFuncRegistry->create_func(on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 17);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_03_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 3);
    on_draw_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_06_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 6);
    on_draw_func_2 = f;

    frame_count = 0;
    inner.time_in_stage = 0;
    stage_flags |= STAGE_FLAG_1;
    inner.camera_facing_i.end_time = 0;
    inner.camera_pos_i.end_time = 0;
    return 0;
}

// FUNCTION: TH16 0x4099a0
Stage::~Stage()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func_2);
    if (vms != NULL)
    {
        AnmVm *vm = vms;
        for (i32 i = 0; i < std->num_quads; i++, vm++)
        {
            vm->~AnmVm();
        }
        if (vms != NULL)
        {
            free(vms);
            vms = NULL;
        }
    }
    if (snapshot_vms != NULL)
    {
        AnmVm *vm = snapshot_vms;
        for (i32 i = 0; i < std->num_quads; i++, vm++)
        {
            vm->~AnmVm();
        }
        if (snapshot_vms != NULL)
        {
            free(snapshot_vms);
            snapshot_vms = NULL;
        }
    }
    for (i32 i = 0; i < 8; i++)
    {
        inner.anm_vms[i].~AnmVm();
        lolk_snapshot_inner.anm_vms[i].~AnmVm();
    }
    if (std != NULL)
    {
        free(std);
        std = NULL;
    }
    if (std_file != NULL)
    {
        free(std_file);
        std_file = NULL;
    }
    std_file = NULL;
    if (inner.fog != NULL)
    {
        delete inner.fog;
        inner.fog = NULL;
    }
    if (lolk_snapshot_inner.fog != NULL)
    {
        delete lolk_snapshot_inner.fog;
        lolk_snapshot_inner.fog = NULL;
    }
    if (!(g_Globals.flags_lo_45c & 1))
    {
        g_AnmManager->unload_anm(3 + (stage_num & 1));
    }
    if (g_Stage == this)
    {
        g_Stage = NULL;
    }
    if (g_Stage2 == this)
    {
        g_Stage2 = NULL;
    }
}

// TODO: the original reserves one more 4-byte stack slot (sub esp, 8): a
// padded frame from GameThread::thread_start's realignment, which ours lacks;
// matches once thread_start realigns (tested with a stand-in double there).
// FUNCTION: TH16 0x409db0
HARNESS_CALLED Stage *Stage::create(const char *path)
{
    Stage *stage = new Stage;
    if (stage->load_data(path, 0) != 0)
    {
        delete stage;
        return NULL;
    }
    return stage;
}

// TODO: the original aligns its frame to 8 bytes (LTCG, for a callee), and
// saves esi/edi in the prologue.
// FUNCTION: TH16 0x409e50
i32 Stage::on_tick()
{
    if (stage_flags & STAGE_DISABLED)
    {
        return 1;
    }
    if ((stage_flags & STAGE_FADING_OUT) && fade_timer.current >= 60)
    {
        return 1;
    }
    inner.camera.unk_fc.x = 0.0f;
    inner.camera.unk_fc.y = 0.0f;
    inner.camera.unk_104 = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 facing = inner.camera.facing + inner.camera.rocking_vector_2;
    D3DXVec3Normalize(&inner.camera.facing_normalized, &facing);
    inner.color_3344 = 0x808080;
    if (!(stage_flags & STAGE_FADING_OUT) || fade_timer.current < 30)
    {
        update_std_vms();
        inner.run_std();
    }
    g_Supervisor.cameras[3] = inner.camera;
    for (i32 i = 0; i < 8; i++)
    {
        inner.anm_vms[i].run();
    }
    inner.step_fog();
    frame_count++;
    return 1;
}

// FUNCTION: TH16 0x40a7a0
int __fastcall Stage::on_tick_callback(void *arg)
{
    return ((Stage *)arg)->on_tick();
}

// Applies camera 3 and makes it the current camera.
static __forceinline void stage_apply_camera_3()
{
    g_Supervisor.current_camera = &g_Supervisor.cameras[3];
    camera_apply_43c940(&g_Supervisor.cameras[3]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = 3;
}

__forceinline void Stage::use_camera()
{
    g_AnmManager->flush_sprites();
    inner.camera.unk_fc.x = g_Supervisor.cameras[3].unk_fc.x;
    inner.camera.unk_fc.y = g_Supervisor.cameras[3].unk_fc.y;
    g_Supervisor.cameras[3] = inner.camera;
    stage_apply_camera_3();
}

// Passes a color change on to the ANM manager.
static __forceinline void stage_set_anm_color(u32 color)
{
    g_AnmManager->unk_1c7fd8c = 1;
    g_AnmManager->unk_1c7fd88.d3d = color;
}

static __forceinline void stage_set_render_state(D3DRENDERSTATETYPE state, DWORD value)
{
    g_AnmManager->flush_sprites();
    g_Supervisor.d3d_device->SetRenderState(state, value);
}

// Clears camera 3's viewport.
static __forceinline void stage_clear_viewport(D3DCOLOR color)
{
    D3DRECT rect;
    D3DVIEWPORT9 &vp = g_Supervisor.cameras[3].viewport;
    rect.x1 = vp.X;
    rect.y1 = vp.Y;
    rect.x2 = vp.X + vp.Width;
    rect.y2 = vp.Y + vp.Height;
    g_Supervisor.d3d_device->Clear(1, &rect, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, color, 1.0f, 0);
}

// Sets up the camera and fog, clears the background and draws layers 0-7.
// TODO: around the inlined ScreenEffect allocation the original pops operator new's argument together with memset's and spills the effect later.
// FUNCTION: TH16 0x409f90
i32 Stage::on_draw_03()
{
    if (stage_flags & STAGE_DISABLED)
    {
        return 1;
    }
    if (!(stage_flags & STAGE_FADING_OUT) || fade_timer.current < 60)
    {
        use_camera();
        g_Supervisor.enable_zwrite_inline();
        stage_set_render_state(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        stage_set_render_state(D3DRS_FOGCOLOR, *(D3DCOLOR *)inner.camera.sky.color);
        stage_set_render_state(D3DRS_FOGSTART, *(DWORD *)&inner.camera.sky.begin_distance);
        stage_set_render_state(D3DRS_FOGEND, *(DWORD *)&inner.camera.sky.end_distance);
        if ((stage_flags & STAGE_FADING_OUT) && frame_count < 34)
        {
            stage_clear_viewport(0);
        }
        else
        {
            stage_clear_viewport(*(D3DCOLOR *)inner.camera.sky.color);
        }
    }
    if (stage_flags & STAGE_FADING_OUT)
    {
        if (fade_timer.current < 30)
        {
            ScreenEffect::create_inline(3, 30, 0, 0, 0, 10);
            stage_flags |= STAGE_FLAG_1;
            fade_timer.set_value(1);
        }
        else
        {
            stage_flags &= ~STAGE_FLAG_1;
            inner.color_changed = 0;
        }
    }
    if (inner.color_changed)
    {
        stage_set_anm_color(inner.color_3344);
        inner.color_changed = 0;
    }
    instances_drawn = 0;
    instances_culled = 0;
    quads_drawn = 0;
    if (stage_flags & STAGE_FLAG_1)
    {
        g_Supervisor.enable_d3d_fog_inline();
        draw_layer(0);
        draw_layer(1);
        draw_layer(2);
        draw_layer(3);
        draw_layer(4);
        draw_layer(5);
        draw_layer(6);
        draw_layer(7);
        g_AnmManager->flush_sprites();
    }
    g_AnmManager->unk_1c7fd8c = 0;
    g_AnmManager->unk_1c7fd88.d3d = 0x80808080;
    g_Supervisor.disable_zwrite_inline();
    stage_set_render_state(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    return 1;
}

// Draws layers 32 and 33 of the ANM manager and layers 8-11 of the stage,
// and runs the fade timer.
// TODO: the original realigns its frame through ebx and stores 0xff into the color byte after loading the flags. Matches once GameThread::thread_start realigns like the original (tested).
// FUNCTION: TH16 0x40a410
i32 Stage::on_draw_06()
{
    if (stage_flags & STAGE_DISABLED)
    {
        return 1;
    }
    if (!(stage_flags & STAGE_FADING_OUT) || fade_timer.current < 60)
    {
        use_camera();
        g_Supervisor.disable_d3d_fog_inline();
        g_Supervisor.disable_zwrite_inline();
        stage_set_render_state(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        g_AnmManager->render_layer(0x20);
        stage_set_render_state(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        g_AnmManager->render_layer(0x21);
        stage_set_render_state(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        stage_set_render_state(D3DRS_FOGCOLOR, *(D3DCOLOR *)inner.camera.sky.color);
        stage_set_render_state(D3DRS_FOGSTART, *(DWORD *)&inner.camera.sky.begin_distance);
        stage_set_render_state(D3DRS_FOGEND, *(DWORD *)&inner.camera.sky.end_distance);
    }
    if ((stage_flags & STAGE_FADING_OUT) && fade_timer.current >= 30)
    {
        inner.color_changed = 0;
    }
    if (stage_flags & STAGE_FLAG_1)
    {
        g_Supervisor.disable_zwrite_inline();
        g_Supervisor.enable_d3d_fog_inline();
        draw_layer(8);
        draw_layer(9);
        draw_layer(10);
        draw_layer(11);
        g_AnmManager->flush_sprites();
    }
    g_AnmManager->unk_1c7fd8c = 0;
    g_AnmManager->unk_1c7fd88.d3d = 0x80808080;
    if (fade_timer.current > 0)
    {
        fade_timer--;
        if (fade_timer.current <= 0)
        {
            inner.color_changed = 0xff;
            if (stage_flags & STAGE_FADING_IN)
            {
                stage_flags |= STAGE_DISABLED;
            }
            stage_flags &= ~(STAGE_FADING_IN | STAGE_FADING_OUT);
            inner.color_3344 = 0xffffff;
        }
    }
    g_Supervisor.disable_zwrite_inline();
    stage_set_render_state(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    g_Supervisor.disable_d3d_fog_inline();
    return 1;
}

// TODO: the float math and the corner stores are scheduled differently (the original reloads center.x and groups the stores by value).
// FUNCTION: TH16 0x40a7d0
HARNESS_CALLED i32 StdObject::is_culled(D3DXVECTOR3 *pos, f32 max_distance_sq, Camera *camera)
{
    D3DXVECTOR3 corners[16];
    D3DXVECTOR3 projected[16];
    D3DXMATRIX world;

    corners[0] = (center + *pos) - (camera->position + camera->rocking_vector_1);
    if (D3DXVec3LengthSq(&corners[0]) > max_distance_sq)
    {
        return 1;
    }
    f32 hx = size.x * 0.5f;
    f32 hy = size.y * 0.5f;
    f32 hz = size.z * 0.5f;
    f32 x_max = center.x + hx;
    f32 x_min = center.x - hx;
    f32 y_max = center.y + hy;
    f32 y_min = center.y - hy;
    f32 z_max = center.z + hz;
    f32 z_min = center.z - hz;
    corners[0].x = x_max;
    corners[0].y = y_max;
    corners[0].z = z_max;
    corners[1].x = x_max;
    corners[1].y = y_max;
    corners[1].z = z_min;
    corners[2].x = x_max;
    corners[2].y = y_min;
    corners[2].z = z_max;
    corners[3].x = x_max;
    corners[3].y = y_min;
    corners[3].z = z_min;
    corners[4].x = x_min;
    corners[4].y = y_max;
    corners[4].z = z_max;
    corners[5].x = x_min;
    corners[5].y = y_max;
    corners[5].z = z_min;
    corners[6].x = x_min;
    corners[6].y = y_min;
    corners[6].z = z_max;
    corners[7].x = x_min;
    corners[7].y = y_min;
    corners[7].z = z_min;
    corners[8].x = center.x;
    corners[8].y = y_min;
    corners[8].z = z_min;
    corners[9].x = center.x;
    corners[9].y = y_max;
    corners[9].z = z_min;
    corners[10].x = center.x;
    corners[10].y = y_min;
    corners[10].z = z_max;
    corners[11].x = center.x;
    corners[11].y = y_max;
    corners[11].z = z_max;
    corners[12].x = center.x;
    corners[12].y = y_min;
    corners[12].z = center.z;
    corners[13].x = center.x;
    corners[13].y = y_max;
    corners[13].z = center.z;
    corners[14].x = center.x;
    corners[14].y = y_min;
    corners[14].z = center.z - hz * 0.5f;
    corners[15].x = center.x;
    corners[15].y = y_max;
    corners[15].z = center.z + hz * 0.5f;
    D3DXMatrixIdentity(&world);
    D3DXMatrixTranslation(&world, pos->x, pos->y, pos->z);
    D3DXVec3ProjectArray(projected, sizeof(D3DXVECTOR3), corners, sizeof(D3DXVECTOR3), &camera->viewport,
                         (D3DXMATRIX *)&camera->projection_matrix, (D3DXMATRIX *)&camera->view_matrix, &world, 16);
    f32 left = (f32)g_early_arcade_offset_x;
    f32 top = (f32)g_early_arcade_offset_y;
    f32 right = left + 384.0f;
    f32 bottom = top + 448.0f;
    f32 max_x = left - 8.0f;
    f32 min_x = right + 8.0f;
    f32 max_y = top - 8.0f;
    f32 min_y = bottom + 8.0f;
    for (i32 i = 0; i < 16; i++)
    {
        if (projected[i].z >= 0.0f && 1.0f >= projected[i].z)
        {
            max_x = projected[i].x > max_x ? projected[i].x : max_x;
            min_x = projected[i].x < min_x ? projected[i].x : min_x;
            min_y = projected[i].y < min_y ? projected[i].y : min_y;
            max_y = projected[i].y > max_y ? projected[i].y : max_y;
        }
    }
    if (max_x >= (f32)g_early_arcade_offset_x && right >= min_x && max_y >= (f32)g_early_arcade_offset_y &&
        bottom >= min_y)
    {
        return 0;
    }
    return 1;
}

// Draws the instances of objects on a layer, culling those out of view.
// FUNCTION: TH16 0x40af70
i32 Stage::draw_layer(i32 layer)
{
    StdInstance *instance = instances;
    inner.draw_vms(layer);
    g_AnmManager->flush_sprites();
    g_Supervisor.enable_d3d_fog_inline();
    stage_apply_camera_3();
    g_AnmManager->render_cache_184fbb8 = 1;
    for (; instance->object_id >= 0; instance++)
    {
        StdObject *object = objects[instance->object_id];
        if (object->layer != layer)
        {
            continue;
        }
        D3DXVECTOR3 pos(instance->pos.x, instance->pos.y, instance->pos.z);
        if (object->is_culled(&pos, inner.unk_3310, &g_Supervisor.cameras[3]))
        {
            instances_culled++;
            instance->unk_2 &= 0xfffe;
            continue;
        }
        object->flags |= 2;
        for (StdQuad *quad = object->quads; quad->type >= 0; quad = (StdQuad *)((u8 *)quad + quad->size))
        {
            AnmVm *vm = &vms[quad->vm_index];
            if (quad->type != 0)
            {
                continue;
            }
            if ((vm->flags_lo & (0x1f << ANM_VM_RENDER_MODE_SHIFT)) >= (4 << ANM_VM_RENDER_MODE_SHIFT))
            {
                vm->entity_pos.x = quad->pos.x + instance->pos.x;
                vm->entity_pos.y = quad->pos.y + instance->pos.y;
                vm->entity_pos.z = quad->pos.z + instance->pos.z;
                if (quad->width != 0.0f)
                {
                    vm->scale.x = quad->width /
                                  g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_width;
                    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
                }
                if (quad->height != 0.0f)
                {
                    vm->scale.y = quad->height /
                                  g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id].sprite_height;
                    vm->flags_lo |= ANM_VM_SCALE_CHANGED;
                }
            }
            u32 mode = vm->flags_lo & (0x1f << ANM_VM_RENDER_MODE_SHIFT);
            if (mode == (8 << ANM_VM_RENDER_MODE_SHIFT) || mode == (24 << ANM_VM_RENDER_MODE_SHIFT))
            {
                g_Supervisor.enable_d3d_fog_inline();
            }
            else
            {
                g_Supervisor.disable_d3d_fog_inline();
            }
            if (vm->flags_lo & 0x2000)
            {
                g_Supervisor.disable_zwrite_inline();
            }
            else
            {
                g_Supervisor.enable_zwrite_inline();
            }
            g_AnmManager->draw_vm(vm);
            quads_drawn++;
        }
        instance->unk_2 |= 1;
        instances_drawn++;
    }
    g_Supervisor.disable_zwrite_inline();
    return 0;
}

// FUNCTION: TH16 0x40c280
void StageInner::draw_vms(i32 layer)
{
    for (i32 i = 0; i < 8; i++)
    {
        AnmVm *vm = &anm_vms[i];
        if (&g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id] == NULL || unk_32f0[i] != layer)
        {
            continue;
        }
        g_Supervisor.current_camera = &g_Supervisor.cameras[3];
        if (g_AnmManager != NULL)
        {
            g_AnmManager->flush_sprites();
        }
        g_Supervisor.d3d_device->SetTransform(D3DTS_VIEW, &g_Supervisor.cameras[3].view_matrix);
        g_Supervisor.d3d_device->SetTransform(D3DTS_PROJECTION, &g_Supervisor.cameras[3].projection_matrix);
        if (g_AnmManager != NULL)
        {
            g_AnmManager->camera_unk_fc.x = g_Supervisor.cameras[3].unk_fc.x;
            g_AnmManager->camera_unk_fc.y = g_Supervisor.cameras[3].unk_fc.y;
        }
        g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
        g_Supervisor.current_camera_index = 3;
        g_Supervisor.disable_d3d_fog_inline();
        g_AnmManager->flush_sprites();
        g_Supervisor.disable_zwrite_inline();
        if (&g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id] != NULL)
        {
            g_AnmManager->draw_vm(vm);
        }
        g_Supervisor.enable_zwrite_inline();
        stage_apply_camera_3();
    }
}

// TODO: register and stack slot allocation differ (the original keeps 255.0f in memory and adds d.x to pos.x the other way round).
// Moves the fog mesh: kind 1 waves the bottom of the screen while no spell
// card is active, kind 2 bulges a disc around the center of the game area
// whose radius shrinks towards unk_3318.
// FUNCTION: TH16 0x40c4a0
void StageInner::step_fog()
{
    if (fog == NULL)
    {
        return;
    }
    // The angles are ZunAngles in ZUN's struct: copied as such.
    ZunAngle angle_a = *(ZunAngle *)&unk_3338;
    ZunAngle angle_b = *(ZunAngle *)&unk_333c;
    D3DXVECTOR3 *point = (D3DXVECTOR3 *)fog->buffer_18;
    D3DXVECTOR3 d;
    if (fog_kind == 1)
    {
        if (g_Spellcard != NULL && (g_Spellcard->flags & 1))
        {
            goto tick;
        }
        fog->set_rect(-192.0f, 320.0f, 384.0f, 128.0f);
        f32 amplitude;
        if (fog_timer.current_f < 60.0f)
        {
            amplitude = fog_timer.current_f * 6.0f / 60.0f;
        }
        else
        {
            amplitude = 6.0f;
        }
        FogVertex *vertex = (FogVertex *)fog->buffer_14;
        for (i32 i = 0; i < fog->vm_count; i++)
        {
            for (i32 j = 0; j < fog->unk_4; j++)
            {
                ((u8 *)&vertex->diffuse)[3] = 0x80;
                f32 t = j * amplitude / (fog->unk_4 - 1);
                d.x = sinf(angle_a.value) * t;
                d.y = sinf(angle_b.value) * t;
                if (i != 0 && j != 0 && i != fog->vm_count - 1 && j != fog->unk_4 - 1)
                {
                    vertex->pos.x = vertex->pos.x + d.x;
                    vertex->pos.y = vertex->pos.y + d.y;
                    vertex->pos.z = 0.0f;
                    point->z = 0.0f;
                }
                angle_a.value = wrap_angle(angle_a.value + 0.66842401f);
                vertex++;
                point++;
            }
            angle_b.value = wrap_angle(angle_b.value - ZUN_PI * 10.0f / 21.0f);
        }
        unk_3338 = wrap_angle(unk_3338 + ZUN_PI / 64);
        unk_333c = wrap_angle(unk_333c + ZUN_PI / 80);
    }
    else if (fog_kind == 2)
    {
        f32 radius = unk_331c;
        if (radius > unk_3318)
        {
            unk_331c = radius - 2.0f;
        }
        fog->set_rect(-radius, 224.0f - radius, radius + radius, radius + radius);
        FogVertex *vertex = (FogVertex *)fog->buffer_14;
        for (i32 i = 0; i < fog->vm_count; i++)
        {
            for (i32 j = 0; j < fog->unk_4; j++)
            {
                d = D3DXVECTOR3(point->x - 224.0f, point->y - 240.0f, point->z - radius * radius);
                f32 t = radius * radius - (d.x * d.x + d.y * d.y);
                if (t >= 0.0f)
                {
                    t /= radius * radius;
                    vertex->diffuse = 0xffffffff;
                    ((u8 *)&vertex->diffuse)[3] = 0x60;
                    ((u8 *)&vertex->diffuse)[2] = 255.0f - (255 - ((u8 *)&vertex->diffuse)[2]) * t;
                    ((u8 *)&vertex->diffuse)[1] = 255.0f - (255 - ((u8 *)&vertex->diffuse)[1]) * t;
                    ((u8 *)&vertex->diffuse)[0] = 255.0f - (255 - ((u8 *)&vertex->diffuse)[0]) * t;
                    f32 scale = t * 32.0f;
                    D3DXVec3Normalize(&d, &d);
                    d *= scale;
                    d.x += sinf(angle_a.value) * t * 8.0f;
                    d.y += sinf(angle_b.value) * t * 8.0f;
                    vertex->pos.x += d.x;
                    vertex->pos.y += d.y;
                    vertex->pos.z = 0.0f;
                    point->z = 0.0f;
                }
                else
                {
                    ((u8 *)&vertex->diffuse)[3] = 0;
                }
                angle_a.value = wrap_angle(angle_a.value + ZUN_PI / 2);
                angle_b.value = wrap_angle(angle_b.value - ZUN_PI * 2 / 9);
                vertex++;
                point++;
            }
        }
        unk_3338 = wrap_angle(unk_3338 + ZUN_PI / 64);
        unk_333c = wrap_angle(g_replay_unsafe_rng.randf_0_to_1() * ZUN_PI / 40.0f + ZUN_PI / 80 + unk_333c);
    }
tick:
    fog_timer.tick_in_place();
}

// FUNCTION: TH16 0x40a7b0
int __fastcall Stage::on_draw_03_callback(void *arg)
{
    return ((Stage *)arg)->on_draw_03();
}

// FUNCTION: TH16 0x40a7c0
int __fastcall Stage::on_draw_06_callback(void *arg)
{
    return ((Stage *)arg)->on_draw_06();
}

// TODO: ours saves esi/edi late (shrink-wrapped) and merges the stack
// cleanups of malloc/memcpy/memset. Matches once GameThread::thread_start
// realigns like the original (tested with a stand-in double there).
// FUNCTION: TH16 0x40ac30
i32 Stage::load_std(const char *path)
{
    if (std_file == NULL)
    {
        strcpy(g_ecl_path, "");
        strcat(g_ecl_path, path);
        std_file = file_read_all(g_ecl_path, &std_file_size, 0);
        if (std_file == NULL)
        {
            return -1;
        }
    }
    std = (StdHeader *)malloc(std_file_size);
    memcpy(std, std_file, std_file_size);
    stage_anm = AnmManager::preload_anm(3 + (stage_num & 1), std->anm_path);
    if (stage_anm == NULL)
    {
        // ステージデータが見つかりません。データが壊れています
        g_GameErrorContext.log("\x83X\x83" "e\x81[\x83W\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8"
                               "\x82\xdc\x82\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4"
                               "\x82\xa2\x82\xdc\x82\xb7\r\n");
        return -1;
    }
    objects = std->objects;
    instances = (StdInstance *)((u8 *)std + std->instances_offset);
    script = (StdInstr *)((u8 *)std + std->script_offset);
    for (i32 i = 0; i < std->num_objects; i++)
    {
        objects[i] = (StdObject *)((u8 *)objects[i] + (u32)std);
    }
    vms = (AnmVm *)malloc(std->num_quads * sizeof(AnmVm));
    memset(vms, 0, std->num_quads * sizeof(AnmVm));
    snapshot_vms = (AnmVm *)malloc(std->num_quads * sizeof(AnmVm));
    memset(snapshot_vms, 0, std->num_quads * sizeof(AnmVm));
    return 0;
}

// Starts a VM for every quad of every object.
// FUNCTION: TH16 0x40add0
HARNESS_CALLED void Stage::start_std_vms()
{
    i32 vm_index = 0;
    on_tick_func->flags |= UPDATE_FUNC_ACTIVE;
    on_draw_func->flags |= UPDATE_FUNC_ACTIVE;
    on_draw_func_2->flags |= UPDATE_FUNC_ACTIVE;
    for (i32 i = 0; i < std->num_objects; i++)
    {
        objects[i]->flags = 1;
        for (StdQuad *quad = objects[i]->quads; quad->type >= 0; quad = (StdQuad *)((u8 *)quad + quad->size))
        {
            stage_anm->copy_vm_and_run(&vms[vm_index], quad->script);
            quad->vm_index = vm_index++;
        }
    }
    inner.cur_instr_offset = 0;
}

// Runs the VMs of objects still marked as running; unmarks objects whose
// VMs have all finished.
// TODO: ours saves ebx/edi after the loop guard (shrink-wrapped). Matches
// once GameThread::thread_start realigns like the original (tested).
// FUNCTION: TH16 0x40aed0
i32 Stage::update_std_vms()
{
    for (i32 i = 0; i < std->num_objects; i++)
    {
        StdObject *object = objects[i];
        if (!(object->flags & 1))
        {
            continue;
        }
        i32 running = 0;
        StdQuad *quad = object->quads;
        while (quad->type >= 0)
        {
            AnmVm *vm = &vms[quad->vm_index];
            vm->run();
            quad = (StdQuad *)((u8 *)quad + quad->size);
            if (vm->instr_offset >= 0)
            {
                running++;
            }
        }
        if (!running)
        {
            object->flags &= ~1;
        }
    }
    return 0;
}

// Jumps the script to the label instruction (opcode 16) with this number.
// FUNCTION: TH16 0x40c040
HARNESS_CALLED void Stage::jump_to_label(i32 label)
{
    for (StdInstr *instr = script; instr->time >= 0; instr = (StdInstr *)((u8 *)instr + instr->size))
    {
        if (instr->opcode == 16 && instr->args[0] == label)
        {
            inner.cur_instr_offset = (u8 *)instr - (u8 *)script;
            inner.time_in_stage = ((StdInstr *)((u8 *)script + inner.cur_instr_offset))->time;
            return;
        }
    }
}

// Fades the screen in over 30 frames while the stage starts.
// FUNCTION: TH16 0x40c0d0
HARNESS_CALLED void Stage::start_fade_in()
{
    ScreenEffect::create_inline(SCREEN_EFFECT_FADE_OUT, 30, 0, 0, 0, 10);
    fade_timer = 30;
    stage_flags |= STAGE_FADING_IN;
}

// FUNCTION: TH16 0x40c210
HARNESS_CALLED void Stage::start_fade_out()
{
    fade_timer = 60;
    stage_flags |= STAGE_FADING_OUT;
}

// TODO: ours gets a /GS cookie (the CameraSky temporaries), which shifts every stack slot; the original also shares one return path per result.
// FUNCTION: TH16 0x40cd10
CameraSky InterpCameraSky::step()
{
    if (end_time > 0)
    {
        time.tick();
        if (time.current >= end_time)
        {
            time.set(end_time);
            end_time = 0;
            if (method == 7 || method == 17)
            {
                return initial;
            }
            return goal;
        }
    }
    else if (end_time == 0)
    {
        if (method == 7 || method == 17)
        {
            return initial;
        }
        return goal;
    }
    if (method == 7)
    {
        CameraSky tmp = initial;
        initial = tmp.add_inline(goal);
        current = initial;
    }
    else if (method == 17)
    {
        CameraSky tmp = initial;
        initial = tmp + bezier_2;
        bezier_2 = bezier_2 + goal;
        current = initial;
    }
    else if (method == 8)
    {
        f32 t = time.current_f / (f32)end_time;
        f32 c_initial = (t - 1.0f) * (t - 1.0f) * (2.0f * t + 1.0f);
        f32 c_goal = t * t * (3.0f - 2.0f * t);
        f32 c_bezier_1 = (1.0f - t) * (1.0f - t) * t;
        f32 c_bezier_2 = (t - 1.0f) * t * t;
        current = initial * c_initial + goal * c_goal + bezier_1 * c_bezier_1 + bezier_2 * c_bezier_2;
    }
    else
    {
        f32 x = interp_common_methods(method, time.current_f, (f32)end_time);
        current = (goal - initial) * x + initial;
    }
    return current;
}

// FUNCTION: TH16 0x40d370
HARNESS_CALLED CameraSky CameraSky::operator+(const CameraSky &other) const
{
    CameraSky result;
    result.begin_distance = begin_distance + other.begin_distance;
    result.end_distance = end_distance + other.end_distance;
    result.color_components[0] = color_components[0] + other.color_components[0];
    result.color_components[1] = color_components[1] + other.color_components[1];
    result.color_components[2] = color_components[2] + other.color_components[2];
    result.color_components[3] = color_components[3] + other.color_components[3];
    for (i32 i = 0; i < 4; i++)
    {
        result.color[i] = result.color_components[i];
    }
    return result;
}

// FUNCTION: TH16 0x40d400
HARNESS_CALLED CameraSky::CameraSky(f32 begin_distance, f32 end_distance, f32 c0, f32 c1, f32 c2, f32 c3)
{
    this->begin_distance = begin_distance;
    this->end_distance = end_distance;
    color_components[0] = c0;
    color_components[1] = c1;
    color_components[2] = c2;
    color_components[3] = c3;
    for (i32 i = 0; i < 4; i++)
    {
        color[i] = color_components[i];
    }
}

// TODO: the original frame has 4 more (unused) bytes: padding for the
// known alignment run_std's realignment gives it (run_std does not realign
// in ours).
// FUNCTION: TH16 0x40b2f0
void Stage::interrupt_vms(i32 n)
{
    if (vms != NULL)
    {
        AnmVm *vm = vms;
        for (i32 i = 0; i < std->num_quads; i++, vm++)
        {
            vm->interrupt(n);
            vm->run();
        }
    }
    for (i32 i = 0; i < 8; i++)
    {
        inner.anm_vms[i].interrupt(n);
        inner.anm_vms[i].run();
    }
}

// The stage script (STD) and the camera rocking patterns. The rocking code
// calls the out-of-line sinf and cosf (0x405510, 0x4054f0), which LTCG
// keeps out of line here (this function has an EH frame).
// TODO: the original realigns its frame (and esp, -8 with an ebx frame),
// which moves every stack slot; its callees that realign (AnmVm::run) are
// stubs here.
// FUNCTION: TH16 0x40b3b0
i32 StageInner::run_std()
{
    StdInstr *ins = (StdInstr *)((u8 *)stage->script + cur_instr_offset);
    while (ins->time <= time_in_stage.current)
    {
        switch (ins->opcode)
        {
        // stop: the script waits here for good.
        case 0:
            goto stopped;
        // jmp: offset, new time
        case 1:
            time_in_stage.set_inline(ins->args[1]);
            cur_instr_offset = ins->args[0];
            ins = (StdInstr *)((u8 *)stage->script + cur_instr_offset);
            continue;
        // pos: also keeps how far the camera moved.
        case 2:
            camera.unk_104 = camera.position;
            camera.position.x = *(f32 *)&ins->args[0];
            camera.position.y = *(f32 *)&ins->args[1];
            camera.position.z = *(f32 *)&ins->args[2];
            camera.unk_104 = camera.position - camera.unk_104;
            break;
        // posTime
        case 3:
        {
            Float3 goal(*(f32 *)&ins->args[2], *(f32 *)&ins->args[3], *(f32 *)&ins->args[4]);
            camera_pos_i.end_time = ins->args[0];
            camera_pos_i.method = ins->args[1];
            camera_pos_i.initial = camera.position;
            camera_pos_i.goal = goal;
            camera_pos_i.reset_timer();
            break;
        }
        // facing
        case 4:
            camera.facing.x = *(f32 *)&ins->args[0];
            camera.facing.y = *(f32 *)&ins->args[1];
            camera.facing.z = *(f32 *)&ins->args[2];
            break;
        // facingTime
        case 5:
        {
            Float3 goal(*(f32 *)&ins->args[2], *(f32 *)&ins->args[3], *(f32 *)&ins->args[4]);
            camera_facing_i.end_time = ins->args[0];
            camera_facing_i.method = ins->args[1];
            camera_facing_i.initial = camera.facing;
            camera_facing_i.goal = goal;
            camera_facing_i.reset_timer();
            break;
        }
        // up
        case 6:
            camera.up.x = *(f32 *)&ins->args[0];
            camera.up.y = *(f32 *)&ins->args[1];
            camera.up.z = *(f32 *)&ins->args[2];
            break;
        // upTime
        case 18:
        {
            Float3 goal(*(f32 *)&ins->args[2], *(f32 *)&ins->args[3], *(f32 *)&ins->args[4]);
            camera_up_i.end_time = ins->args[0];
            camera_up_i.method = ins->args[1];
            camera_up_i.initial = camera.up;
            camera_up_i.goal = goal;
            camera_up_i.reset_timer();
            break;
        }
        // fov
        case 7:
            camera.field_of_view = *(f32 *)&ins->args[0];
            break;
        // fog: color, begin and end distance
        case 8:
            *(i32 *)camera.sky.color = ins->args[0];
            camera.sky.color_components[0] = camera.sky.color[0];
            camera.sky.color_components[1] = camera.sky.color[1];
            camera.sky.color_components[2] = camera.sky.color[2];
            camera.sky.color_components[3] = camera.sky.color[3];
            camera.sky.begin_distance = *(f32 *)&ins->args[1];
            camera.sky.end_distance = *(f32 *)&ins->args[2];
            break;
        // fogTime
        case 9:
        {
            CameraSky goal;
            goal.begin_distance = *(f32 *)&ins->args[3];
            goal.end_distance = *(f32 *)&ins->args[4];
            goal.color_components[0] = ((u8 *)&ins->args[2])[0];
            goal.color_components[1] = ((u8 *)&ins->args[2])[1];
            goal.color_components[2] = ((u8 *)&ins->args[2])[2];
            goal.color_components[3] = ((u8 *)&ins->args[2])[3];
            for (i32 i = 0; i < 4; i++)
            {
                goal.color[i] = goal.color_components[i];
            }
            set_sky_interp(ins->args[0], ins->args[1], &goal);
            break;
        }
        // posBezier, facingBezier: initial, bezier_1, goal, bezier_2.
        case 10:
        {
            Float3 bezier_1(*(f32 *)&ins->args[2], *(f32 *)&ins->args[3], *(f32 *)&ins->args[4]);
            Float3 goal(*(f32 *)&ins->args[5], *(f32 *)&ins->args[6], *(f32 *)&ins->args[7]);
            Float3 bezier_2(*(f32 *)&ins->args[8], *(f32 *)&ins->args[9], *(f32 *)&ins->args[10]);
            camera_pos_i.end_time = ins->args[0];
            camera_pos_i.initial = camera.position;
            camera_pos_i.bezier_1 = bezier_1;
            camera_pos_i.goal = goal;
            camera_pos_i.bezier_2 = bezier_2;
            camera_pos_i.method = 8;
            camera_pos_i.reset_timer();
            break;
        }
        case 11:
        {
            Float3 bezier_1(*(f32 *)&ins->args[2], *(f32 *)&ins->args[3], *(f32 *)&ins->args[4]);
            Float3 goal(*(f32 *)&ins->args[5], *(f32 *)&ins->args[6], *(f32 *)&ins->args[7]);
            Float3 bezier_2(*(f32 *)&ins->args[8], *(f32 *)&ins->args[9], *(f32 *)&ins->args[10]);
            camera_facing_i.end_time = ins->args[0];
            camera_facing_i.initial = camera.facing;
            camera_facing_i.bezier_1 = bezier_1;
            camera_facing_i.goal = goal;
            camera_facing_i.bezier_2 = bezier_2;
            camera_facing_i.method = 8;
            camera_facing_i.reset_timer();
            break;
        }
        // rockMode
        case 12:
            rocking_mode() = *(u8 *)&ins->args[0];
            if (rocking_mode() == 0)
            {
                camera.rocking_vector_1.x = 0.0f;
                camera.rocking_vector_1.y = 0.0f;
                camera.rocking_vector_1.z = 0.0f;
            }
            timer_1c.reset_inline();
            if (rocking_mode() == 2)
            {
                timer_1c.set_value(0x200);
            }
            break;
        // bgColor
        case 13:
            g_Supervisor.background_color = ins->args[0];
            break;
        // sprite: replaces one of the eight extra VMs (-1 stops it, -2
        // hides it).
        case 14:
        {
            i32 script = ins->args[1];
            if (script >= 0)
            {
                AnmVm *vm = &anm_vms[ins->args[0]];
                stage->stage_anm->copy_vm(vm, script);
                vm->unk_5b0 = NULL;
                vm->parent = NULL;
                vm->run();
            }
            else if (script == -2)
            {
                anm_vms[ins->args[0]].flags_lo &= ~1;
            }
            else if (script == -1)
            {
                anm_vms[ins->args[0]].instr_offset = script;
                anm_vms[ins->args[0]].flags_lo &= ~1;
            }
            unk_32f0[ins->args[0]] = ins->args[2];
            break;
        }
        // Replaces the fog effect.
        case 17:
            if (fog != NULL)
            {
                delete fog;
            }
            fog = NULL;
            unk_3318 = 112.0f;
            unk_331c = 192.0f;
            unk_3320 = -1;
            unk_3338 = 0;
            unk_333c = 0;
            fog_timer.reset_inline();
            fog_kind = ins->args[0];
            if (unk_3318 > 0.0f)
            {
                if (fog_kind == 1)
                {
                    fog = new Fog(0, 7, 0);
                }
                else
                {
                    fog = new Fog(0, 0x11, 0);
                }
            }
            break;
        case 20:
            unk_3310 = *(f32 *)&ins->args[0] * *(f32 *)&ins->args[0];
            break;
        // interrupt
        case 19:
            stage->interrupt_vms(ins->args[0] + 7);
            break;
        }
        cur_instr_offset += ins->size;
        ins = (StdInstr *)((u8 *)stage->script + cur_instr_offset);
    }
    time_in_stage.tick();
stopped:
    if (camera_facing_i.end_time != 0)
    {
        camera.facing = camera_facing_i.step();
    }
    if (camera_pos_i.end_time != 0)
    {
        camera.position = camera_pos_i.step();
    }
    if (camera_sky_i.end_time != 0)
    {
        camera.sky = camera_sky_i.step();
    }
    if (camera_up_i.end_time != 0)
    {
        camera.up = camera_up_i.step();
    }
    if (rocking_mode() != 0)
    {
        switch (rocking_mode())
        {
        case 1:
        {
            f32 angle = normalize_angle(timer_1c.current_f * ZUN_PI * 2.0f / 512.0f);
            f32 s = sinf(angle);
            f32 x = s * -20.0f;
            camera.rocking_vector_1.x = x;
            f32 z = sinf(normalize_angle(angle * 2.0f)) * -10.0f;
            camera.rocking_vector_1.z = z;
            camera.up.x = s * -0.01f;
            camera.rocking_vector_2.x = x * -0.5f;
            camera.rocking_vector_2.z = z * -0.5f;
            timer_1c++;
            if (timer_1c.current >= 0x200)
            {
                timer_1c.set_value(0);
            }
            break;
        }
        case 2:
        {
            f32 angle = normalize_angle(timer_1c.current_f * ZUN_PI * 2.0f / 3072.0f);
            camera.up.x = -sinf(angle);
            camera.up.z = cosf(angle);
            timer_1c++;
            if (timer_1c.current >= 0xc00)
            {
                timer_1c.set_value(0);
            }
            break;
        }
        case 3:
        {
            f32 angle = normalize_angle(timer_1c.current_f * ZUN_PI * 2.0f / 2048.0f);
            f32 s = sinf(angle);
            f32 v = s * 50.0f;
            camera.rocking_vector_1.x = v;
            camera.rocking_vector_1.y = v;
            camera.rocking_vector_1.z = sinf(normalize_angle(angle * 2.0f)) * -100.0f;
            camera.rocking_vector_2.x = -v;
            camera.rocking_vector_2.y = -v;
            camera.up.x = s * -0.05f;
            timer_1c++;
            if (timer_1c.current >= 0x800)
            {
                timer_1c.set_value(0);
            }
            break;
        }
        case 4:
        {
            f32 angle = normalize_angle(timer_1c.current_f * ZUN_PI * 2.0f / 3072.0f);
            camera.up.x = -sinf(angle);
            camera.up.z = -cosf(angle);
            timer_1c++;
            if (timer_1c.current >= 0xc00)
            {
                timer_1c.set_value(0);
            }
            break;
        }
        case 7:
        {
            f32 angle = timer_1c.current_f * ZUN_PI * 2.0f / 2048.0f - ZUN_PI;
            f32 s = sinf(angle);
            camera.rocking_vector_1.x = s * 70.0f;
            camera.rocking_vector_1.z = sinf(normalize_angle(angle + angle)) * 200.0f;
            camera.up.x = s * -0.1f;
            timer_1c++;
            if (timer_1c.current >= 0x800)
            {
                timer_1c.set_value(0);
            }
            break;
        }
        case 8:
        {
            f32 s = sinf(timer_1c.current_f * ZUN_PI * 2.0f / 1024.0f - ZUN_PI);
            camera.rocking_vector_1.x = s * -50.0f;
            camera.up.x = s * -0.1f;
            timer_1c++;
            if (timer_1c.current >= 0x400)
            {
                timer_1c.set_value(0);
            }
            break;
        }
        case 9:
        {
            f32 s = sinf(timer_1c.current_f * ZUN_PI * 2.0f / 512.0f - ZUN_PI);
            camera.up.x = s * -0.01f;
            camera.rocking_vector_1.x = s * -15.0f;
            timer_1c++;
            if (timer_1c.current >= 0x200)
            {
                timer_1c.set_value(0);
            }
            break;
        }
        }
    }
    return 0;
}
