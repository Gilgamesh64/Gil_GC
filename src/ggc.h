#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef enum{
    GC_DEBUG_NONE = 0,
    GC_DEBUG_BASIC,
    GC_DEBUG_PEDANTIC,
    GC_DEBUG_PARANOID
} gc_debug_mode_t;

typedef enum{
    GC_GROW_ON_REQUEST = 0,
    GC_GROW_LINEAR,
    GC_GROW_EXPONENTIAL
} gc_growth_factor_t;

typedef void (*gc_finalizer_t)(void*);

void *gc_malloc(size_t size);
void *gc_calloc(size_t element_number, size_t element_size);
void *gc_realloc(void *ptr, size_t new_size);
bool gc_free(const void *ptr);

void gc_print_heap(void);

void gc_cycle(void);

void gc_set_debug_mode(gc_debug_mode_t gc_debug_mode);
void gc_set_growth_factor(gc_growth_factor_t gc_growth_factor);
void use_lazy_sweep(void);
void gc_disable_interior_ptr(void);
void gc_disable_depth_ptr(void);
void gc_set_max_heap_size(size_t size);

bool gc_add_finalizer(void* ptr, gc_finalizer_t fn);