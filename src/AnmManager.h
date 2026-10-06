#pragma once

#include "AnmVm.h"
#include "decomp.h"
#include "types.h"

// A loaded .anm file. Layout from ExpHP (zAnmLoaded); mostly unknown here.
struct AnmLoaded
{
    i32 slot_num;
    char name[0x104];
    void *anm_file;
    AnmVm *vms;
    i32 entry_count;
    i32 script_count;
    i32 sprite_count;
    void *sprites;
    u8 **scripts;
    void *d3d;
    i32 load_wait;
    u8 unk_12c[0x13c - 0x12c];

    // Frees the file and everything made from it. ExpHP calls this the
    // destructor, but delete runs it as an ordinary call first.
    void release();
};

// Owns every loaded .anm file and the VMs created from them.
struct AnmManager
{
    u8 unk_0[0x184f4f0];
    // Files loaded by preload_anm, by slot.
    AnmLoaded *preloaded[31];
    u8 unk_184f56c[0x1c7fd90 - 0x184f56c];

    // Reaches the manager through g_AnmManager; callers push only the
    // arguments and the callee pops them.
    static AnmLoaded *__stdcall preload_anm(i32 slot, const char *path);
    // Marks the VM and its children for deletion. ExpHP: anm_unload_46f1c0.
    static void __stdcall unload_vm(AnmId id);

    // ExpHP: AnmBehemoth::disable_vms_from_anm_file
    void disable_vms_from_anm_file(AnmLoaded *anm);
    void draw_vm(AnmVm *vm);
    AnmVm *get_vm_with_id(AnmId id);

    void unload_anm(i32 slot)
    {
        if (preloaded[slot] != NULL)
        {
            preloaded[slot]->release();
            delete preloaded[slot];
            preloaded[slot] = NULL;
        }
    }
};

extern AnmManager *g_AnmManager;
