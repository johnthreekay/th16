#pragma once

#include "decomp.h"
#include "types.h"

// LZSS with a 13-bit window and 4-bit match lengths, used for th16.dat's
// directory and entries.

// Returns a new allocation of at most twice in_size bytes; its used size
// goes to *out_size.
u8 *LTCG_FASTCALL lzss_compress(u8 *in, i32 in_size, i32 *out_size);
// Decompresses into dest, or into a new allocation if dest is NULL.
u8 *LTCG_FASTCALL lzss_decompress(u8 *in, i32 in_size, u8 *dest, i32 out_size);
