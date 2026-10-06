#include <stdlib.h>
#include <string.h>

#include "Crypt.h"

// Each block is filled from both ends towards the middle: the first half of
// the ciphertext lands on every other byte from the end of the block, the
// second half on the bytes in between. A trailing partial block shorter
// than a quarter block, and an odd final byte, stay unencrypted.

// FUNCTION: TH16 0x402220
u8 *LTCG_FASTCALL zun_decrypt(u8 *data, i32 size, u8 key, u8 step, i32 block, i32 limit)
{
    u8 *out = data;
    i32 tail = size % block;
    i32 copy_size = limit > size ? size : limit;
    u8 *tmp = (u8 *)malloc(copy_size);
    if (tmp == NULL)
    {
        return data;
    }
    memcpy(tmp, data, copy_size);
    if (tail >= block / 4)
    {
        tail = 0;
    }
    i32 remaining = (size & ~1) - tail;

    u8 *in = tmp;
    while (remaining > 0 && limit > 0)
    {
        if (remaining < block)
        {
            block = remaining;
        }
        u8 *p = out + block - 1;
        out += block;
        for (i32 i = (block + 1) / 2; i > 0; i--)
        {
            *p = *in++ ^ key;
            p -= 2;
            key += step;
        }
        p = out - 2;
        for (i32 i = block / 2; i > 0; i--)
        {
            *p = *in++ ^ key;
            p -= 2;
            key += step;
        }
        remaining -= block;
        limit -= block;
    }
    free(tmp);
    return data;
}

// FUNCTION: TH16 0x402330
u8 *LTCG_FASTCALL zun_encrypt(u8 *data, i32 size, u8 key, u8 step, i32 block, i32 limit)
{
    u8 *out = data;
    i32 tail = size % block;
    i32 copy_size = limit > size ? size : limit;
    u8 *tmp = (u8 *)malloc(copy_size);
    if (tmp == NULL)
    {
        return data;
    }
    memcpy(tmp, data, copy_size);
    if (tail >= block / 4)
    {
        tail = 0;
    }
    i32 remaining = (size & ~1) - tail;

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
            *out++ = *p ^ key;
            p -= 2;
            key += step;
        }
        p = in - 2;
        for (i32 i = block / 2; i > 0; i--)
        {
            *out++ = *p ^ key;
            p -= 2;
            key += step;
        }
        remaining -= block;
        limit -= block;
    }
    free(tmp);
    return data;
}
