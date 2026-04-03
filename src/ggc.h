#ifndef GGC
#define GGC
#include <stdlib.h>
#include <stdbool.h>

void *gc_malloc(size_t size);
bool gc_free(const void *ptr);
void gc_print_heap();
void gc_mark();
void gc_sweep();
void gc_cycle();
void gc_activate_debug();

void gc_visualizer_draw_list(int screenW, int screenH);

#endif
