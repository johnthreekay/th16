#include <stdlib.h>
#include <string.h>

#include "Crypt.h"

// Each block is filled from both ends towards the middle: the first half of
// the ciphertext lands on every other byte from the end of the block, the
// second half on the bytes in between. A trailing partial block shorter
// than a quarter block, and an odd final byte, stay unencrypted.

// TODO: 75%; the original keeps the copy size in eax (spilled) and the tail
// in ebx, and gives out and the block end separate stack slots.
// FUNCTION: TH16 0x402220
u8 *LTCG_FASTCALL zun_decrypt(u8 *data, i32 size, u8 key, u8 step, i32 block, i32 limit)
{
    i32 tail = size % block;
    if (tail >= block / 4)
    {
        tail = 0;
    }
    u8 *out = data;
    i32 copy_size = limit > size ? size : limit;
    u8 *tmp = (u8 *)malloc(copy_size);
    if (tmp == NULL)
    {
        return data;
    }
    i32 remaining = (size & ~1) - tail;
    memcpy(tmp, data, copy_size);

    u8 *in = tmp;
    while (remaining > 0 && limit > 0)
    {
        if (remaining < block)
        {
            block = remaining;
        }
        u8 *end = out + block;
        out = end;
        u8 *p = end - 1;
        for (i32 i = (block + 1) / 2; i > 0; i--)
        {
            *p = *in ^ key;
            p -= 2;
            key += step;
            in++;
        }
        p = end - 2;
        for (i32 i = block / 2; i > 0; i--)
        {
            *p = *in ^ key;
            p -= 2;
            key += step;
            in++;
        }
        remaining -= block;
        limit -= block;
    }
    free(tmp);
    return data;
}

// TODO: 81%; register and stack slot allocation differ as in zun_decrypt.
// FUNCTION: TH16 0x402330
u8 *LTCG_FASTCALL zun_encrypt(u8 *data, i32 size, u8 key, u8 step, i32 block, i32 limit)
{
    i32 tail = size % block;
    if (tail >= block / 4)
    {
        tail = 0;
    }
    u8 *out = data;
    i32 copy_size = limit > size ? size : limit;
    u8 *tmp = (u8 *)malloc(copy_size);
    if (tmp == NULL)
    {
        return data;
    }
    i32 remaining = (size & ~1) - tail;
    memcpy(tmp, data, copy_size);

    u8 *in = tmp;
    while (remaining > 0 && limit > 0)
    {
        if (remaining < block)
        {
            block = remaining;
        }
        u8 *p = in + block - 1;
        in += block;
        for (i32 i = (block + 1) / 2; i > 0; i--)
        {
            *out = *p ^ key;
            p -= 2;
            key += step;
            out++;
        }
        p = in - 2;
        for (i32 i = block / 2; i > 0; i--)
        {
            *out = *p ^ key;
            p -= 2;
            key += step;
            out++;
        }
        remaining -= block;
        limit -= block;
    }
    free(tmp);
    return data;
}
