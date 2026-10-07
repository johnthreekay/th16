// AnmManager's per-layer draw callbacks. Each draws one layer's VMs at its
// own UpdateFunc priority (the hex number in the name); some first switch
// the camera or render state for the layers that follow.
#include "AnmManager.h"
#include "CriticalSections.h"
#include "Supervisor.h"

// 0x43c780. Recomputes a camera's matrices and viewport.
void __stdcall camera_update_43c780(Camera *camera);

// Makes one of the Supervisor's cameras the current one.
inline void use_camera(i32 index)
{
    g_Supervisor.current_camera = &g_Supervisor.cameras[index];
    g_Supervisor.swap_transform_matrices(&g_Supervisor.cameras[index]);
    g_Supervisor.d3d_device->SetViewport(&g_Supervisor.current_camera->viewport);
    g_Supervisor.current_camera_index = index;
}

// Starts drawing a layer that ignores the depth buffer.
inline void disable_depth_test()
{
    g_Supervisor.disable_zwrite();
    g_AnmManager->flush_sprites();
    g_Supervisor.d3d_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
}

// FUNCTION: TH16 0x46e750
i32 AnmManager::render_layer(i32 layer)
{
    ENTER_CS(CS_ANM_MANAGER);
    for (AnmVm *vm = layer_list_dummy_heads[layer].next_in_layer; vm != NULL; vm = vm->next_in_layer)
    {
        if (!(vm->flags_hi & (ANM_VM_DELETE_PENDING | ANM_VM_IN_DELETE_LIST)))
        {
            draw_vm(vm);
        }
    }
    LEAVE_CS(CS_ANM_MANAGER);
    return 1;
}

// FUNCTION: TH16 0x46dd90
int __fastcall AnmManager::on_draw_05_layer_00(AnmManager *mgr)
{
    return mgr->render_layer(0);
}

// FUNCTION: TH16 0x46dda0
int __fastcall AnmManager::on_draw_07_layer_01(AnmManager *mgr)
{
    return mgr->render_layer(1);
}

// FUNCTION: TH16 0x46ddb0
int __fastcall AnmManager::on_draw_09_layer_02(AnmManager *mgr)
{
    return mgr->render_layer(2);
}

// FUNCTION: TH16 0x46ddc0
int __fastcall AnmManager::on_draw_0b_layer_04(AnmManager *mgr)
{
    return mgr->render_layer(4);
}

// FUNCTION: TH16 0x46de60
int __fastcall AnmManager::on_draw_0d_layer_05(AnmManager *mgr)
{
    return mgr->render_layer(5);
}

// FUNCTION: TH16 0x46de70
int __fastcall AnmManager::on_draw_10_layer_06(AnmManager *mgr)
{
    return mgr->render_layer(6);
}

// FUNCTION: TH16 0x46de80
int __fastcall AnmManager::on_draw_12_layer_07(AnmManager *mgr)
{
    return mgr->render_layer(7);
}

// FUNCTION: TH16 0x46de90
int __fastcall AnmManager::on_draw_14_layer_08(AnmManager *mgr)
{
    return mgr->render_layer(8);
}

// FUNCTION: TH16 0x46dea0
int __fastcall AnmManager::on_draw_15_layer_09(AnmManager *mgr)
{
    return mgr->render_layer(9);
}

// FUNCTION: TH16 0x46deb0
int __fastcall AnmManager::on_draw_16_layer_10(AnmManager *mgr)
{
    return mgr->render_layer(10);
}

// FUNCTION: TH16 0x46dec0
int __fastcall AnmManager::on_draw_18_layer_11(AnmManager *mgr)
{
    return mgr->render_layer(11);
}

// FUNCTION: TH16 0x46ded0
int __fastcall AnmManager::on_draw_1c_layer_13(AnmManager *mgr)
{
    return mgr->render_layer(13);
}

// FUNCTION: TH16 0x46dee0
int __fastcall AnmManager::on_draw_1f_layer_14(AnmManager *mgr)
{
    return mgr->render_layer(14);
}

// FUNCTION: TH16 0x46def0
int __fastcall AnmManager::on_draw_20_layer_15(AnmManager *mgr)
{
    return mgr->render_layer(15);
}

// FUNCTION: TH16 0x46df00
int __fastcall AnmManager::on_draw_22_layer_16(AnmManager *mgr)
{
    return mgr->render_layer(16);
}

// FUNCTION: TH16 0x46df10
int __fastcall AnmManager::on_draw_24_layer_17(AnmManager *mgr)
{
    return mgr->render_layer(17);
}

// FUNCTION: TH16 0x46df20
int __fastcall AnmManager::on_draw_27_layer_18(AnmManager *mgr)
{
    return mgr->render_layer(18);
}

// FUNCTION: TH16 0x46df30
int __fastcall AnmManager::on_draw_1b_layer_12(AnmManager *mgr)
{
    return mgr->render_layer(12);
}

// FUNCTION: TH16 0x46df40
int __fastcall AnmManager::on_draw_2a_layer_19(AnmManager *mgr)
{
    return mgr->render_layer(19);
}

// FUNCTION: TH16 0x46df50
int __fastcall AnmManager::on_draw_2e_layer_21(AnmManager *mgr)
{
    return mgr->render_layer(21);
}

// FUNCTION: TH16 0x46df60
int __fastcall AnmManager::on_draw_34_layer_22(AnmManager *mgr)
{
    return mgr->render_layer(22);
}

// FUNCTION: TH16 0x46df70
int __fastcall AnmManager::on_draw_36_layer_23(AnmManager *mgr)
{
    return mgr->render_layer(23);
}

// FUNCTION: TH16 0x46df80
int __fastcall AnmManager::on_draw_4f_layer_30(AnmManager *mgr)
{
    return mgr->render_layer(30);
}

// FUNCTION: TH16 0x46df90
int __fastcall AnmManager::on_draw_52_layer_31(AnmManager *mgr)
{
    return mgr->render_layer(31);
}

// FUNCTION: TH16 0x46dfa0
int __fastcall AnmManager::on_draw_4d_layer_29(AnmManager *mgr)
{
    return mgr->render_layer(29);
}

// FUNCTION: TH16 0x46dfb0
int __fastcall AnmManager::on_draw_3d_layer_26(AnmManager *mgr)
{
    return mgr->render_layer(26);
}

// FUNCTION: TH16 0x46dfc0
int __fastcall AnmManager::on_draw_3e_layer_27(AnmManager *mgr)
{
    return mgr->render_layer(27);
}

// FUNCTION: TH16 0x46dfd0
int __fastcall AnmManager::on_draw_3b_layer_25(AnmManager *mgr)
{
    return mgr->render_layer(25);
}

// FUNCTION: TH16 0x46dfe0
int __fastcall AnmManager::on_draw_3c_layer_37(AnmManager *mgr)
{
    return mgr->render_layer(37);
}

// FUNCTION: TH16 0x46dff0
int __fastcall AnmManager::on_draw_3f_layer_38(AnmManager *mgr)
{
    return mgr->render_layer(38);
}

// FUNCTION: TH16 0x46e000
int __fastcall AnmManager::on_draw_4e_layer_40(AnmManager *mgr)
{
    return mgr->render_layer(40);
}

// FUNCTION: TH16 0x46e010
int __fastcall AnmManager::on_draw_50_layer_41(AnmManager *mgr)
{
    return mgr->render_layer(41);
}

// FUNCTION: TH16 0x46e020
int __fastcall AnmManager::on_draw_53_layer_42(AnmManager *mgr)
{
    return mgr->render_layer(42);
}

// FUNCTION: TH16 0x46ddd0
int __fastcall AnmManager::on_draw_0a_layer_03(AnmManager *mgr)
{
    camera_update_43c780(&g_Supervisor.cameras[3]);
    use_camera(3);
    g_Supervisor.disable_d3d_fog_inline();
    return mgr->render_layer(3);
}

// FUNCTION: TH16 0x46e030
int __fastcall AnmManager::on_draw_2d_layer_20(AnmManager *mgr)
{
    use_camera(1);
    disable_depth_test();
    return mgr->render_layer(20);
}

// FUNCTION: TH16 0x46e0d0
int __fastcall AnmManager::on_draw_3a_layer_24(AnmManager *mgr)
{
    use_camera(2);
    disable_depth_test();
    g_AnmManager->camera_2d_offset.x = 0.0f;
    g_AnmManager->camera_2d_offset.y = 0.0f;
    return mgr->render_layer(24);
}

// FUNCTION: TH16 0x46e180
int __fastcall AnmManager::on_draw_40_layer_28(AnmManager *mgr)
{
    use_camera(0);
    int result = mgr->render_layer(28);
    use_camera(2);
    return result;
}

// FUNCTION: TH16 0x46e210
int __fastcall AnmManager::on_draw_37_layer_36(AnmManager *mgr)
{
    use_camera(2);
    disable_depth_test();
    return mgr->render_layer(36);
}

// FUNCTION: TH16 0x46e2b0
int __fastcall AnmManager::on_draw_41_layer_39(AnmManager *mgr)
{
    use_camera(0);
    int result = mgr->render_layer(39);
    use_camera(2);
    return result;
}
