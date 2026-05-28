#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef void (*gc_finalizer_t)(void*);

void *gc_malloc(size_t size);
void *gc_calloc(size_t element_number, size_t element_size);
void *gc_realloc(void *ptr, size_t new_size);
bool gc_free(const void *ptr);

void gc_print_heap(void);

void gc_cycle(void);


void gc_activate_gc_debug(void);
void gc_activate_allocator_debug(void);
void gc_use_lazy_sweep(void);

bool gc_add_finalizer(void* ptr, gc_finalizer_t fn);