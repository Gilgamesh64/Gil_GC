#ifndef GGC_VISUALIER
#define GGC_VISUALIER
#include "ggc.h"

#include <stdbool.h>
#include <stddef.h>

void visualizer_init(void);
void visualizer_cleanup(void);
void visualizer_add_block(size_t offset, size_t size, bool free, bool marked);
void visualizer_draw(void);
void visualizer_clear(void);

#endif
