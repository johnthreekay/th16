#pragma once

#include "decomp.h"
#include "types.h"

// The XOR scheme used for THDAT archive entries; thtk implements the same.
u8 *LTCG_FASTCALL zun_decrypt(u8 *data, i32 size, u8 key, u8 step, i32 block, i32 limit);
u8 *LTCG_FASTCALL zun_encrypt(u8 *data, i32 size, u8 key, u8 step, i32 block, i32 limit);
