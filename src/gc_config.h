#pragma once

typedef enum{
    GC_EAGER_SWEEP = 0,
    GC_LAZY_SWEEP
} sweep_mode_t;

extern bool gc_debug;
extern bool allocator_debug;
extern sweep_mode_t gc_sweep_mode;

void gc_activate_gc_debug();
void gc_activate_allocator_debug();
void gc_set_sweep_mode(sweep_mode_t sweep_mode);