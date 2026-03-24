#include <stdio.h>
#include "ggc.h"
#include <stdlib.h>

//asm("mov %%rsp, %0" : "=r"(sp_before_main));

void test(){
    volatile int* tony = gc_malloc(sizeof(int));
}

int main(){
    volatile int* gigi = gc_malloc(sizeof(int));
    printf("Gigi stack ptr: %p, Gigi heap ptr: %p\n", &gigi, gigi);

    test();
    gc_cycle();

    return 0;
}