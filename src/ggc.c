#include "ggc.h"

/* We don't need <unistd.h> on Windows and, in fact, the standard
   MinGW headers don't declare sbrk at all which caused a compiler error.
   Use conditional compilation to pull in the header only on POSIX systems. */
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

/* The allocator maintains a linked list of blocks.  The head of that list
   was previously declared `static` in the header, which resulted in each
   translation unit having its own separate `heap_head` variable.  Define it
   once here so the state is truly shared. */
block_t *heap_head = NULL;

/* Request memory from the operating system.  On POSIX we use sbrk so that the
   new memory is adjacent to our previous allocations.  On Windows (or any
   other platform where sbrk isn't available) just fall back to malloc.  This
   isn't a perfect substitute but it keeps the allocator buildable and lets
   the rest of the code continue to exercise the free list logic. */
void *request_from_os(size_t size) {
#if defined(_WIN32) || defined(_WIN64)
    return malloc(size);
#else
    /* The classic sbrk interface returns the *previous* program break, so we
       call it twice: once to query the current break and again to bump it. */
    void *p = sbrk(0);
    if (sbrk(size) == (void *) -1)
        return NULL;
    return p;
#endif
}

/* Utility to round up the requested size to a multiple of 8 bytes (or
   the alignment requirements of block_t).  This helps keep the data portion
   of each allocation properly aligned. */
static size_t align8(size_t s) {
    return (s + 7) & ~((size_t)7);
}

block_t *find_free_block(size_t size) {
    block_t *current = heap_head;

    while (current) {
        if (current->free && current->size >= size)
            return current;
        current = current->next;
    }

    return NULL;
}

/* Allocate a new block by requesting memory from the OS.  The returned
   pointer is inserted at the end of the doubly-linked list. */
block_t *extend_heap(size_t size) {
    block_t *block = request_from_os(sizeof(block_t) + size);
    if (!block) return NULL;

    block->size = size;
    block->free = 0;
    block->next = NULL;
    block->prev = NULL;

    if (!heap_head) {
        heap_head = block;
    } else {
        block_t *last = heap_head;
        while (last->next)
            last = last->next;

        last->next = block;
        block->prev = last;
    }

    return block;
}

void *gc_malloc(size_t size) {

    if (size == 0)
        return NULL;

    size = align8(size);
    block_t *block = find_free_block(size);

    if (!block) {
        block = extend_heap(size);
    } else {
        /* reuse existing free block; split if it's much bigger than
           required. */
        block->free = 0;
        if (block->size >= size + sizeof(block_t) + 8) {
            /* split the leftover into a new free block */
            block_t *newblk = (block_t *)((char *)(block + 1) + size);
            newblk->size = block->size - size - sizeof(block_t);
            newblk->free = 1;
            newblk->next = block->next;
            newblk->prev = block;
            if (newblk->next)
                newblk->next->prev = newblk;
            block->next = newblk;
            block->size = size;
        }
    }

    if (!block)
        return NULL;

    return (void *)(block + 1);
}

/* attempt to merge adjacent free blocks to reduce fragmentation */
static void coalesce(block_t *block) {
    if (block->next && block->next->free) {
        block_t *n = block->next;
        block->size += sizeof(block_t) + n->size;
        block->next = n->next;
        if (n->next)
            n->next->prev = block;
    }
    if (block->prev && block->prev->free) {
        block_t *p = block->prev;
        p->size += sizeof(block_t) + block->size;
        p->next = block->next;
        if (block->next)
            block->next->prev = p;
    }
}

void gc_free(void *ptr) {

    if (!ptr)
        return;

    block_t *block = (block_t*)ptr - 1;
    block->free = 1;
    coalesce(block);
}