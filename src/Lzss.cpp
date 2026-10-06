#include <stdlib.h>

#include "Lzss.h"

// Mark Nelson's LZSS from The Data Compression Book, with a 13-bit window,
// working on memory instead of files and with the bit I/O inlined.

#define LZSS_INDEX_BITS 13
#define LZSS_LENGTH_BITS 4
#define LZSS_WINDOW_SIZE (1 << LZSS_INDEX_BITS)
#define LZSS_BREAK_EVEN 2
#define LZSS_LOOK_AHEAD_SIZE ((1 << LZSS_LENGTH_BITS) + LZSS_BREAK_EVEN)
#define LZSS_TREE_ROOT LZSS_WINDOW_SIZE
#define LZSS_UNUSED 0
#define LZSS_END_OF_STREAM 0
#define LZSS_MOD_WINDOW(a) ((a) & (LZSS_WINDOW_SIZE - 1))

struct LzssTreeNode
{
    i32 parent;
    i32 smaller_child;
    i32 larger_child;
};

// GLOBAL: TH16 0x4a6f30
LzssTreeNode g_lzss_tree[LZSS_WINDOW_SIZE + 1];
// GLOBAL: TH16 0x4bef40
u8 g_lzss_window[LZSS_WINDOW_SIZE];

// FUNCTION: TH16 0x4580e0
HARNESS_CALLED void lzss_init()
{
    for (i32 i = 0; i < LZSS_WINDOW_SIZE; i++)
    {
        g_lzss_window[i] = 0;
    }
    for (i32 i = 0; i < LZSS_WINDOW_SIZE + 1; i++)
    {
        g_lzss_tree[i].parent = 0;
        g_lzss_tree[i].smaller_child = 0;
        g_lzss_tree[i].larger_child = 0;
    }
}

// FUNCTION: TH16 0x4583d0
void LTCG_FASTCALL lzss_contract_node(i32 old_node, i32 new_node)
{
    g_lzss_tree[new_node].parent = g_lzss_tree[old_node].parent;
    if (g_lzss_tree[g_lzss_tree[old_node].parent].larger_child == old_node)
    {
        g_lzss_tree[g_lzss_tree[old_node].parent].larger_child = new_node;
    }
    else
    {
        g_lzss_tree[g_lzss_tree[old_node].parent].smaller_child = new_node;
    }
    g_lzss_tree[old_node].parent = LZSS_UNUSED;
}

// FUNCTION: TH16 0x458430
void LTCG_FASTCALL lzss_replace_node(i32 old_node, i32 new_node)
{
    i32 parent = g_lzss_tree[old_node].parent;
    if (g_lzss_tree[parent].smaller_child == old_node)
    {
        g_lzss_tree[parent].smaller_child = new_node;
    }
    else
    {
        g_lzss_tree[parent].larger_child = new_node;
    }
    g_lzss_tree[new_node] = g_lzss_tree[old_node];
    g_lzss_tree[g_lzss_tree[new_node].smaller_child].parent = new_node;
    g_lzss_tree[g_lzss_tree[new_node].larger_child].parent = new_node;
    g_lzss_tree[old_node].parent = LZSS_UNUSED;
}

// FUNCTION: TH16 0x4584b0
i32 LTCG_FASTCALL lzss_find_next_node(i32 node)
{
    i32 next = g_lzss_tree[node].smaller_child;
    while (g_lzss_tree[next].larger_child != LZSS_UNUSED)
    {
        next = g_lzss_tree[next].larger_child;
    }
    return next;
}

// FUNCTION: TH16 0x458370
void LTCG_FASTCALL lzss_delete_string(i32 p)
{
    if (g_lzss_tree[p].parent == LZSS_UNUSED)
    {
        return;
    }
    if (g_lzss_tree[p].larger_child == LZSS_UNUSED)
    {
        lzss_contract_node(p, g_lzss_tree[p].smaller_child);
    }
    else if (g_lzss_tree[p].smaller_child == LZSS_UNUSED)
    {
        lzss_contract_node(p, g_lzss_tree[p].larger_child);
    }
    else
    {
        i32 replacement = lzss_find_next_node(p);
        lzss_delete_string(replacement);
        lzss_replace_node(p, replacement);
    }
}

// FUNCTION: TH16 0x458130
HARNESS_CALLED i32 lzss_add_string(i32 new_node, i32 *match_position)
{
    i32 i;
    i32 delta;
    i32 *child;

    if (new_node == LZSS_END_OF_STREAM)
    {
        return 0;
    }
    i32 test_node = g_lzss_tree[LZSS_TREE_ROOT].larger_child;
    i32 match_length = 0;
    for (;;)
    {
        for (i = 0; i < LZSS_LOOK_AHEAD_SIZE; i++)
        {
            delta = g_lzss_window[LZSS_MOD_WINDOW(new_node + i)] - g_lzss_window[LZSS_MOD_WINDOW(test_node + i)];
            if (delta != 0)
            {
                break;
            }
        }
        if (i >= match_length)
        {
            match_length = i;
            *match_position = test_node;
            if (match_length >= LZSS_LOOK_AHEAD_SIZE)
            {
                lzss_replace_node(test_node, new_node);
                return match_length;
            }
        }
        if (delta >= 0)
        {
            child = &g_lzss_tree[test_node].larger_child;
        }
        else
        {
            child = &g_lzss_tree[test_node].smaller_child;
        }
        if (*child == LZSS_UNUSED)
        {
            *child = new_node;
            g_lzss_tree[new_node].parent = test_node;
            g_lzss_tree[new_node].larger_child = LZSS_UNUSED;
            g_lzss_tree[new_node].smaller_child = LZSS_UNUSED;
            return match_length;
        }
        test_node = *child;
    }
}

// TODO: register allocation differs (the original keeps each bit-reading
// loop's result in edi and spills less).
// FUNCTION: TH16 0x457f00
u8 *LTCG_FASTCALL lzss_decompress(u8 *in, i32 in_size, u8 *dest, i32 out_size)
{
    i32 i;
    i32 c;
    i32 match_length;
    i32 match_position;
    u32 m;
    u8 mask = 0x80;
    u32 rack = 0;

    if (dest == NULL)
    {
        dest = (u8 *)malloc(out_size);
        if (dest == NULL)
        {
            return NULL;
        }
    }
    u8 *p = in;
    u8 *out = dest;
    i32 current_position = 1;
    for (;;)
    {
        // Past the end of the input, the reader sees zeros.
        if (mask == 0x80)
        {
            rack = *p;
            if (p - in >= in_size)
            {
                rack = 0;
            }
            else
            {
                p++;
            }
        }
        u32 bit = rack & mask;
        mask >>= 1;
        if (mask == 0)
        {
            mask = 0x80;
        }
        if (bit)
        {
            c = 0;
            for (m = 0x80; m != 0; m >>= 1)
            {
                if (mask == 0x80)
                {
                    rack = *p;
                    if (p - in >= in_size)
                    {
                        rack = 0;
                    }
                    else
                    {
                        p++;
                    }
                }
                if (rack & mask)
                {
                    c |= m;
                }
                mask >>= 1;
                if (mask == 0)
                {
                    mask = 0x80;
                }
            }
            g_lzss_window[current_position] = c;
            *out++ = c;
            current_position = LZSS_MOD_WINDOW(current_position + 1);
        }
        else
        {
            match_position = 0;
            for (m = 1 << (LZSS_INDEX_BITS - 1); m != 0; m >>= 1)
            {
                if (mask == 0x80)
                {
                    rack = *p;
                    if (p - in >= in_size)
                    {
                        rack = 0;
                    }
                    else
                    {
                        p++;
                    }
                }
                if (rack & mask)
                {
                    match_position |= m;
                }
                mask >>= 1;
                if (mask == 0)
                {
                    mask = 0x80;
                }
            }
            if (match_position == LZSS_END_OF_STREAM)
            {
                break;
            }
            match_length = 0;
            for (m = 1 << (LZSS_LENGTH_BITS - 1); m != 0; m >>= 1)
            {
                if (mask == 0x80)
                {
                    rack = *p;
                    if (p - in >= in_size)
                    {
                        rack = 0;
                    }
                    else
                    {
                        p++;
                    }
                }
                if (rack & mask)
                {
                    match_length |= m;
                }
                mask >>= 1;
                if (mask == 0)
                {
                    mask = 0x80;
                }
            }
            match_length += LZSS_BREAK_EVEN;
            for (i = 0; i <= match_length; i++)
            {
                c = g_lzss_window[LZSS_MOD_WINDOW(match_position + i)];
                g_lzss_window[current_position] = c;
                *out++ = c;
                current_position = LZSS_MOD_WINDOW(current_position + 1);
            }
        }
    }
    while (mask != 0x80)
    {
        mask >>= 1;
        if (mask == 0)
        {
            mask = 0x80;
        }
    }
    return dest;
}

// FUNCTION: TH16 0x457b20
u8 *LTCG_FASTCALL lzss_compress(u8 *in, i32 in_size, i32 *out_size)
{
    i32 i;
    i32 c;
    i32 look_ahead_bytes;
    i32 replace_count;
    i32 match_length;
    i32 match_position;
    u32 rack = 0;
    u8 mask = 0x80;

    u8 *out_start = (u8 *)malloc(in_size * 2);
    if (out_start == NULL)
    {
        return NULL;
    }
    u8 *out = out_start;
    *out_size = 0;
    lzss_init();

    u8 *p = in;
    i32 current_position = 1;
    for (i = 0; i < LZSS_LOOK_AHEAD_SIZE; i++)
    {
        c = p - in >= in_size ? -1 : *p++;
        if (c == -1)
        {
            break;
        }
        g_lzss_window[current_position + i] = c;
    }
    look_ahead_bytes = i;
    g_lzss_tree[LZSS_TREE_ROOT].larger_child = current_position;
    g_lzss_tree[current_position].parent = LZSS_TREE_ROOT;
    g_lzss_tree[current_position].larger_child = LZSS_UNUSED;
    g_lzss_tree[current_position].smaller_child = LZSS_UNUSED;
    match_length = 0;
    match_position = 0;
    while (look_ahead_bytes > 0)
    {
        if (match_length > look_ahead_bytes)
        {
            match_length = look_ahead_bytes;
        }
        if (match_length <= LZSS_BREAK_EVEN)
        {
            replace_count = 1;
            rack |= mask;
            mask >>= 1;
            if (mask == 0)
            {
                *out++ = rack;
                mask = 0x80;
                rack = 0;
            }
            for (u32 m = 0x80; m != 0; m >>= 1)
            {
                if (g_lzss_window[current_position] & m)
                {
                    rack |= mask;
                }
                mask >>= 1;
                if (mask == 0)
                {
                    *out++ = rack;
                    mask = 0x80;
                    rack = 0;
                }
            }
        }
        else
        {
            mask >>= 1;
            if (mask == 0)
            {
                *out++ = rack;
                mask = 0x80;
                rack = 0;
            }
            for (u32 m = 1 << (LZSS_INDEX_BITS - 1); m != 0; m >>= 1)
            {
                if (match_position & m)
                {
                    rack |= mask;
                }
                mask >>= 1;
                if (mask == 0)
                {
                    *out++ = rack;
                    mask = 0x80;
                    rack = 0;
                }
            }
            for (u32 m = 1 << (LZSS_LENGTH_BITS - 1); m != 0; m >>= 1)
            {
                if ((match_length - (LZSS_BREAK_EVEN + 1)) & m)
                {
                    rack |= mask;
                }
                mask >>= 1;
                if (mask == 0)
                {
                    *out++ = rack;
                    mask = 0x80;
                    rack = 0;
                }
            }
            replace_count = match_length;
        }
        for (i = 0; i < replace_count; i++)
        {
            lzss_delete_string(LZSS_MOD_WINDOW(current_position + LZSS_LOOK_AHEAD_SIZE));
            c = p - in >= in_size ? -1 : *p++;
            if (c == -1)
            {
                look_ahead_bytes--;
            }
            else
            {
                g_lzss_window[LZSS_MOD_WINDOW(current_position + LZSS_LOOK_AHEAD_SIZE)] = c;
            }
            current_position = LZSS_MOD_WINDOW(current_position + 1);
            if (look_ahead_bytes)
            {
                match_length = lzss_add_string(current_position, &match_position);
            }
        }
    }
    mask >>= 1;
    if (mask == 0)
    {
        *out++ = rack;
        mask = 0x80;
        rack = 0;
    }
    for (u32 m = 1 << (LZSS_INDEX_BITS - 1); m != 0; m >>= 1)
    {
        if (LZSS_END_OF_STREAM & m)
        {
            rack |= mask;
        }
        mask >>= 1;
        if (mask == 0)
        {
            *out++ = rack;
            mask = 0x80;
            rack = 0;
        }
    }
    *out_size = out - out_start;
    return out_start;
}
