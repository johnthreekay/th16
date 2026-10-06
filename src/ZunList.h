#pragma once

// Doubly linked list node that ZUN embeds in many objects. Layout from
// ExpHP's th-re-data (zLinkedList).
struct ZunList
{
    void *entry;
    ZunList *next;
    ZunList *prev;
    ZunList *unk_c;

    void unlink();
};
