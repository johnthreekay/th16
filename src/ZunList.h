#pragma once

#include <stddef.h>

// Intrusive doubly linked list node, the same shape as UpdateFuncList.
// ExpHP: zLinkedList.
template <class T> struct ZunList
{
    T *entry;
    ZunList<T> *next;
    ZunList<T> *prev;
    ZunList<T> *unk_c;

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

    void append(ZunList<T> *node)
    {
        ZunList<T> *last = this;
        while (last->next != NULL)
        {
            last = last->next;
        }
        last->insert_after(node);
    }
};
