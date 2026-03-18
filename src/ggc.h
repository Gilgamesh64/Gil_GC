#ifndef GGC
#define GGC
#include <stdlib.h>
#include <stdbool.h>

typedef struct block {
    size_t size;
   	bool free;
    struct block *next;
    struct block *prev;
} block_t;

extern block_t *heap_head;

void *gc_malloc(size_t size);
void gc_free(void *ptr);
void print_heap();

#endif
