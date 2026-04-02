#ifndef GGC
#define GGC
#include <stdlib.h>
#include <stdbool.h>


void *gc_malloc(size_t size);
bool gc_free(const void *ptr);
void gc_print_heap();
void gc_cycle();
void gc_activate_debug();

void* gc_get_heap_head(void);
void* gc_get_heap_end(void);

#endif
