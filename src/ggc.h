#ifndef GGC
#define GGC
#include <stdlib.h>

typedef struct block {
    size_t size;
    int free;
    struct block *next;
    struct block *prev;
} block_t;

/* The head of our explicit free list.  Defined in ggc.c so that all
   translation units share the same allocator state.  Previously this was
   declared `static` in the header which created a separate `heap_head` for
   each .c file that included <ggc.h>.  That meant user code and the allocator
   were talking to different heaps! */
extern block_t *heap_head;

void *request_from_os(size_t size);
block_t *extend_heap(size_t size);
block_t *find_free_block(size_t size);
void *gc_malloc(size_t size);
void gc_free(void *ptr);

#endif