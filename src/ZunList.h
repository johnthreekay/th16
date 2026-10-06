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

    void init(T *e)
    {
        entry = e;
        next = NULL;
        prev = NULL;
        unk_c = NULL;
    }

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

    // unlink, written out where the original has its own copy of it.
    void unlink_inline()
    {
        if (next != NULL)
        {
            next->prev = prev;
        }
        if (prev != NULL)
        {
            prev->next = next;
        }
        next = NULL;
        prev = NULL;
    }

    // Takes the node out of its list. Only ZunList<void> defines it
    // (ZunList.cpp): the original has one copy (0x405610) for every list,
    // and ZunList<void> stands for ExpHP's untyped zLinkedList.
    void unlink();
};

template <> void ZunList<void>::unlink();
