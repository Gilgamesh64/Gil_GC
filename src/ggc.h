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

void *request_from_os(size_t size);
block_t *extend_heap(size_t size);
block_t *find_free_block(size_t size);
void print_heap();
void *gc_malloc(size_t size);
void gc_free(void *ptr);

#endif
