// Stand-in callers for unit 8b (0x469000-0x474c00) functions whose shape
// depends on how the rest of the game calls them.
#include "../AnmManager.h"
#include "../Ecl.h"
#include "../Enemy.h"

// Like AnmLoaded::create_managed_child (0x46ed60), which picks the list by
// its mode flags. Every caller goes through g_AnmManager.
AnmId harness_unit8b_insert(AnmVm *vm, i32 mode)
{
    if ((mode & 6) == 6)
    {
        return g_AnmManager->insert_in_ui_list_front(vm);
    }
    if (mode & 4)
    {
        return g_AnmManager->insert_in_ui_list_back(vm);
    }
    if (mode & 2)
    {
        return g_AnmManager->insert_in_world_list_front(vm);
    }
    return g_AnmManager->insert_in_world_list_back(vm);
}

// Supervisor creates the manager and stores it here.
void harness_unit8b_set_anm_manager(AnmManager *anm)
{
    g_AnmManager = anm;
}

// Like the manager's tick, which marks finished trees.
void harness_unit8b_mark_tree(AnmVm *vm)
{
    g_AnmManager->mark_tree_for_delete(vm);
}

// The callers of AnmId::search_children (ANM and ECL instructions) always
// pass n = 0, and the recursive AnmVm helpers are called from several
// places in the original.
AnmId harness_unit8b_search_children(AnmId *id, i32 unk)
{
    return id->search_children(unk, 0);
}

void harness_unit8b_ins_316(AnmVm *vm, i32 set)
{
    if (set)
    {
        vm->show_tree();
    }
    else
    {
        vm->hide_tree();
    }
}

// VMs that did not come from the pool are deleted in several places
// (0x43b900 has many callers), so LTCG keeps the scalar deleting
// destructor out of line.
void harness_unit8b_delete_vm_1(AnmVm *vm)
{
    delete vm;
}

void harness_unit8b_delete_vm_2(AnmVm *vm)
{
    delete vm;
}

void harness_unit8b_delete_vm_3(AnmVm *vm)
{
    delete vm;
}

// ECL instruction handlers read their arguments through these. The pop
// variants are only used for argument 0; the given-value ones get varied
// arguments.
f32 harness_unit8b_ecl_args(EclRunContext *ctx, i32 index, i32 i, f32 f)
{
    f32 result = (f32)ctx->pop_int_arg(0) + ctx->pop_float_arg(0);
    result += (f32)ctx->get_int_arg_given_value(index, i);
    result += ctx->get_float_arg_given_value(index, f);
    result += (f32)ctx->pop_int_arg_given_value(index, i);
    result += ctx->pop_float_arg_given_value(index, f);
    return result;
}

// ECL instructions read float arguments through EnemyData (index varies);
// the int pointer getter is only ever asked for argument 0.
f32 harness_unit8b_enemy_args(EnemyData *enemy, EclRunContext *ctx, i32 index)
{
    *enemy->get_int_arg_ptr(0) = 1;
    *ctx->get_int_arg_ptr(0) = 2;
    return enemy->get_float_arg(index) + ctx->get_float_arg(index + 1);
}

// ANM instructions and the pause menu restore snapshots by id.
AnmId harness_unit8b_restore_snapshot(AnmId id)
{
    return g_AnmManager->restore_snapshot(id);
}

// EnemyInf::on_tick runs its VM at the game speed (varies).
i32 harness_unit8b_run_ecl(SptInf *vm, f32 speed)
{
    return vm->run_ecl(speed);
}

// ECL's call instructions (0x472030) start the arguments at index 0.
i32 harness_unit8b_call_sub(EclRunContext *ctx, EclRunContext *dest)
{
    return ctx->call_sub(dest, 0, 0);
}
