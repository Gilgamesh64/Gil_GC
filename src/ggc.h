#pragma once

#include <stdlib.h>
#include <stdbool.h>

void *gc_malloc(size_t size);
void *gc_calloc(size_t element_number, size_t element_size);
void *gc_realloc(void *ptr, size_t new_size);
bool gc_free(const void *ptr);

void gc_print_heap();


void gc_cycle();
