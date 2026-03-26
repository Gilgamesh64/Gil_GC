#include <stdio.h>
#include "ggc.h"
#include <stdlib.h>

typedef struct test{
    struct test *next;
} test_t;

void create_list(){
    test_t *head = gc_malloc(sizeof(test_t));
    
    head -> next = gc_malloc(sizeof(test_t));
} //head pointer goes out of scope

void test_list(){
    create_list();
    gc_cycle(); //frees the entire list
}

void test_reg(){
    register void* ptr_reg asm("rbx") = gc_malloc(sizeof(int));

    gc_cycle(); //data stays alive
    gc_cycle();
}

void test_free(){
    int* a = gc_malloc(sizeof(int));
    int* b = gc_malloc(sizeof(int));
    gc_free(a);
    gc_free(b);

    int c;
    int* p = &c;
    gc_free(p);
}

int* test_allocation(){
    volatile int* a = gc_malloc(sizeof(int));
    return gc_malloc(sizeof(int));
}

void test_out_of_scope(){
    volatile int* b = test_allocation();
}

void test_new_stack_frame(){
    double a,b,c;
}

void test_gc(){
    test_out_of_scope();
    gc_cycle();
}

int main(){
    test_reg();

    return 0;
}