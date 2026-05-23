#include "gc_config.h"
#include <stdbool.h>

gc_debug = false;
allocator_debug = false;
gc_sweep_mode = GC_EAGER_SWEEP;

void gc_activate_gc_debug() { gc_debug = true; }
void gc_activate_allocator_debug() { allocator_debug = true; }
void gc_set_sweep_mode(sweep_mode_t sweep_mode)  {gc_sweep_mode = sweep_mode; }