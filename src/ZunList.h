#pragma once

#include <stddef.h>

// Intrusive doubly linked list node used all over the engine; entry points
// back at the owner (NULL in list heads). Same shape as UpdateFuncList.
// ExpHP: zLinkedList and its per-type copies (zBulletList, zAnmVmList, ...).
template <class T> struct ZunList
{
    T *entry;
    ZunList<T> *next;
    ZunList<T> *prev;
    ZunList<T> *unk_c;

    // Insert node right after this one.
    void insert_after(ZunList<T> *node)
    {
        if (next != NULL)
        {
            node->next = next;
            next->prev = node;
        }
        if (unk_c != NULL)
        {
            unk_c = node;
        }
        next = node;
        node->prev = this;
    }
};
