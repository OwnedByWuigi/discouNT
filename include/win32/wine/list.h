/* Minimal Wine-compatible intrusive doubly-linked list implementation.
 * Kept in-tree so freestanding applications do not depend on host Wine
 * development headers. */
#ifndef __DISCOUNT_WINE_LIST_H
#define __DISCOUNT_WINE_LIST_H

#include <stddef.h>

struct list
{
    struct list *next;
    struct list *prev;
};

#define LIST_INIT(name) { &(name), &(name) }
static inline void list_init(struct list *head) { head->next = head; head->prev = head; }

static inline void list_add_tail(struct list *head, struct list *entry)
{
    entry->prev = head->prev;
    entry->next = head;
    head->prev->next = entry;
    head->prev = entry;
}

static inline void list_remove(struct list *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = entry->prev = entry;
}

static inline int list_empty(const struct list *head)
{
    return head->next == head;
}

static inline struct list *list_head(const struct list *head)
{
    return head->next;
}

#define LIST_ENTRY(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define LIST_FOR_EACH_ENTRY(cursor, head, type, member) \
    for ((cursor) = LIST_ENTRY((head)->next, type, member); \
         &(cursor)->member != (head); \
         (cursor) = LIST_ENTRY((cursor)->member.next, type, member))

#define LIST_FOR_EACH_ENTRY_SAFE(cursor, cursor2, head, type, member) \
    for ((cursor) = LIST_ENTRY((head)->next, type, member), \
         (cursor2) = LIST_ENTRY((cursor)->member.next, type, member); \
         &(cursor)->member != (head); \
         (cursor) = (cursor2), \
         (cursor2) = LIST_ENTRY((cursor)->member.next, type, member))

#endif
