#include <stdio.h>
#include "ggc.h"
#include <raylib.h>
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
    int screenW = 1000;
    int screenH = 400;

    InitWindow(screenW, screenH, "Linked List Visual");
    SetTargetFPS(60);
    void* allocations[100];
    int curr = 0;

    printf("Press:\nD -> nullify last\nQ -> allocate int\nW -> allocate 2 doubles\nM -> mark\nS -> sweep\nC -> full cycle");

    while (!WindowShouldClose()) {

        // --- demo interaction ---
        if (IsKeyPressed(KEY_D)) {
            if(curr > 0) curr--;
            allocations[curr] = NULL;
            printf("ptr at pos %d becomes NULL\n", curr);
        }
        if (IsKeyPressed(KEY_Q)) {
            allocations[curr] = gc_malloc(sizeof(int));
            printf("Allocating new pointer at pos: %d\n", curr);
            curr++;
        }
        if (IsKeyPressed(KEY_W)) {
            allocations[curr] = gc_malloc(sizeof(double) * 2);
            printf("Allocating new pointer at pos: %d\n", curr);
            curr++;
        }
        if (IsKeyPressed(KEY_M)) {
            gc_mark();
        }
        if (IsKeyPressed(KEY_S)) {
            gc_sweep();
        }
        if (IsKeyPressed(KEY_C)) {
            gc_cycle();
        }

        // --- draw ---
        BeginDrawing();
        ClearBackground(RAYWHITE);

        gc_visualizer_draw_list(GetScreenWidth(), GetScreenHeight());

        EndDrawing();
    }

    CloseWindow();
    return 0;
}