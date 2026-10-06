#include <stdlib.h>
#include <string.h>

#include "Stage.h"

#include "Ecl.h"
#include "FileSystem.h"
#include "GameErrorContext.h"
#include "Globals.h"
#include "Supervisor.h"

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

// FUNCTION: TH16 0x409550
Fog::~Fog()
{
    delete_vm_and_clear(anm_id);
    if (unk_14 != NULL)
    {
        free(unk_14);
        unk_14 = NULL;
    }
    if (unk_18 != NULL)
    {
        free(unk_18);
        unk_18 = NULL;
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
    if (unk_10 != NULL)
    {
        free(unk_10);
        unk_10 = NULL;
    }
}

// FUNCTION: TH16 0x409670
Stage::Stage()
{
    memset(this, 0, sizeof(Stage));
    flags |= 2;
}

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

// FUNCTION: TH16 0x40a7b0
int __fastcall Stage::on_draw_03_callback(void *arg)
{
    return ((Stage *)arg)->on_draw_03();
}

// FUNCTION: TH16 0x40a7c0
int __fastcall Stage::on_draw_06_callback(void *arg)
{
    ((Stage *)arg)->on_draw_06();
    return 1;
}

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

// FUNCTION: TH16 0x40c210
HARNESS_CALLED void Stage::start_fade_out()
{
    fade_timer = 60;
    stage_flags |= STAGE_FADING_OUT;
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
