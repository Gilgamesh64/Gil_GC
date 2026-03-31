#include <stdio.h>
#include "ggc.h"
#include <stdlib.h>

typedef struct test{
    struct test *next;
} test_t;

void create_list(){
    test_t *head = gc_malloc(sizeof(test_t));
    
    head -> next = gc_malloc(sizeof(test_t));

    gc_cycle();
    printf("********LIST GOES OUT OF SCOPE********\n");
} //head pointer goes out of scope

void test_list(){
    create_list();
    gc_cycle(); //frees the entire list
}

void test_reg(){
    register void* ptr_reg asm("rbx") = gc_malloc(sizeof(int));
    printf("********REGISTER IN SCOPE********\n");


    gc_cycle(); //data stays alive
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

    int* a = gc_malloc(sizeof(int));
    printf("********2 VAR OUT OF SCOPE, 1 NOT********\n");
    gc_print_heap();
    gc_cycle();
}

void run_tests(){
    printf("\n----------------------------Testing standard allocation----------------------------\n");
    test_gc();
    printf("********1 VAR GOES OUT OF SCOPE********\n");
    gc_cycle();
    printf("\n\n----------------------------Testing list handling----------------------------\n");
    test_list();
    printf("\n\n----------------------------Testing register marking----------------------------\n");
    test_reg();

    gc_print_heap();

}

int main(){
    gc_activate_debug();
    run_tests();
    return 0;
}