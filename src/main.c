#include <stdio.h>
#include "ggc.h"
#include <stdlib.h>

typedef struct test{
    struct test *next;
} test_t;

void test_fun(){
    test_t *a = (test_t *)gc_malloc(sizeof(test_t));
    
    a -> next = gc_malloc(sizeof(test_t));
}

int main(){

    test_fun();

    //register void* ptr_reg asm("rbx") = gc_malloc(sizeof(int));
    gc_cycle();

    return 0;
}