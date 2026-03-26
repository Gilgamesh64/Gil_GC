#ifndef GGC
#define GGC
#include <stdlib.h>
#include <stdbool.h>

void *gc_malloc(size_t size);
bool gc_free(void *ptr);
void print_heap();
void gc_cycle();

#endif
