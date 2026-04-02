// gc_visualizer.c
#include "ggc_visualizer.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 200
#define MAX_BLOCKS 1024

typedef struct visual_block {
    size_t offset;
    size_t size;
    bool free;
    bool marked;
} visual_block_t;

static visual_block_t blocks[MAX_BLOCKS];
static size_t block_count = 0;

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

void visualizer_init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return;
    }
    window = SDL_CreateWindow("GC Visualizer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

void visualizer_add_block(size_t offset, size_t size, bool free, bool marked) {
    if (block_count >= MAX_BLOCKS) return;
    blocks[block_count++] = (visual_block_t){ offset, size, free, marked };
}

void visualizer_draw() {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (size_t i = 0; i < block_count; i++) {
        visual_block_t b = blocks[i];
        if (b.free) SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); // gray
        else if (b.marked) SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // blue
        else SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // green

        int x = (int)((double)b.offset / (double)(gc_get_heap_end() - (void*)gc_get_heap_head()) * WINDOW_WIDTH);
        int w = (int)((double)b.size / (double)(gc_get_heap_end() - (void*)gc_get_heap_head()) * WINDOW_WIDTH);
        if (w < 1) w = 1;
        SDL_Rect rect = { x, 50, w, 100 };
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(50); // animation delay
}

void visualizer_clear() {
    block_count = 0;
}

void visualizer_cleanup() {
    if(renderer) SDL_DestroyRenderer(renderer);
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
}