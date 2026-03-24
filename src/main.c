#include <stdio.h>
#include "ggc.h"
#include <stdlib.h>

int main(){
    
    register void* ptr_reg asm("rbx") = gc_malloc(sizeof(int));
    gc_cycle();

    return 0;
}