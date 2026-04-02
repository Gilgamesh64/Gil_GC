#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "ggc.h"

#define LARGE_SIZE (1024 * 1024 * 10) // 10 MB
#define SMALL_COUNT 100
#define MIXED_COUNT 500

// ---------- Test Helpers ----------
void separator(const char *name) {
    printf("\n================ %s ================\n", name);
}

void test_basic() {
    separator("Basic Allocation");
    int *a = (int *)gc_malloc(sizeof(int));
    gc_cycle();
    separator("Basic Allocation exit scope");
}

void test_large_alloc() {
    separator("Large Allocation");
    char *buf = (char *)gc_malloc(LARGE_SIZE);
    memset(buf, 'A', LARGE_SIZE);
    gc_cycle();
    separator("Large Allocation exit scooe");
}

void test_many_small() {
    separator("Many Small Allocations");
    void *ptrs[SMALL_COUNT];
    for (int i = 0; i < SMALL_COUNT; i++) {
        ptrs[i] = gc_malloc(16);
    }
    gc_cycle();
    separator("Many small allocations exit scope");
}

void test_dangling() {
    separator("Dangling References");
    int *a = (int *)gc_malloc(sizeof(int));
    *a = 99;
    a = NULL;
    gc_cycle();
    separator("Dangling object should be freed");
}

typedef struct Node {
    struct Node *next;
} Node;

void test_linked_list() {
    separator("Linked List");
    Node *head = NULL;
    for (int i = 0; i < SMALL_COUNT; i++) {
        Node *n = gc_malloc(sizeof(Node));
        n->next = head;
        head = n;
    }
    gc_cycle();
    separator("Linked list out of scope");
}

typedef struct Cycle {
    struct Cycle *other;
} Cycle;

void test_cycle() {
    separator("Cyclic References");
    Cycle *a = (Cycle *)gc_malloc(sizeof(Cycle));
    Cycle *b = (Cycle *)gc_malloc(sizeof(Cycle));
    a->other = b;
    b->other = a;

    a = NULL;
    //b = NULL;

    gc_cycle();
    separator("Cycle goes out of scope");
}

void test_interior_pointer() {
    separator("Interior Pointer");
    char *buf = (char *)gc_malloc(100);
    strcpy(buf, "Hello GC");

    char *mid = buf + 3;
    gc_cycle();

    separator("Interior pointer goes out of scope");
}

void test_stack_roots() {
    separator("Stack Roots");
    int *a = (int *)gc_malloc(sizeof(int));
    *a = 123;

    gc_cycle();
    separator("Stack roots goes out of scope");
}

void test_overwrite() {
    separator("Overwrite Pointer");
    int *a = (int *)gc_malloc(sizeof(int));
    *a = 5;

    a = (int *)gc_malloc(sizeof(int));
    *a = 10;

    gc_cycle();
    printf("Only latest allocation should remain: %d\n", *a);
    separator("Overwrittern pointer goes out of scope");
}

void test_fragmentation() {
    separator("Fragmentation Stress");
    void *ptrs[MIXED_COUNT];

    for (int i = 0; i < MIXED_COUNT; i++) {
        size_t size = (i % 128) + 1;
        ptrs[i] = gc_malloc(size);
    }

    for (int i = 0; i < MIXED_COUNT; i += 2) {
        ptrs[i] = NULL;
    }

    gc_cycle();
    separator("Fragmented memory goes out of scope");
}

void test_repeated_gc() {
    separator("Repeated GC");
    for (int i = 0; i < 100; i++) {
        void *p = gc_malloc(64);
        (void)p;
        gc_cycle();
    }
    printf("Repeated GC cycles done\n");
    separator("Going out of scope");
}

void recursive_alloc(int depth) {
    if (depth == 0) return;
    int *a = (int *)gc_malloc(sizeof(int));
    *a = depth;
    recursive_alloc(depth - 1);
    if (*a == -1) printf("Impossible\n");
}

void test_deep_stack() {
    separator("Deep Stack");
    recursive_alloc(10000);
    gc_cycle();
}

void test_false_pointers() {
    separator("False Pointers");
    uintptr_t *fake = (uintptr_t *)gc_malloc(sizeof(uintptr_t) * 100);

    for (int i = 0; i < 100; i++) {
        fake[i] = (uintptr_t)rand();
    }

    gc_cycle();
    printf("False pointers going out of scope");
}

void test_alignment() {
    separator("Alignment");
    char *a = (char *)gc_malloc(3);
    char *b = (char *)gc_malloc(5);
    char *c = (char *)gc_malloc(7);

    a[0] = 'A'; b[0] = 'B'; c[0] = 'C';

    gc_cycle();
    separator("Alignment test goes out of scope");
}

void test_massive() {
    separator("Massive Allocation");
    for (int i = 0; i < 2000; i++) {
        void *p = gc_malloc(2048 * 2048 * 2);
        memset(p, i, 1024);
    }
    gc_cycle();
    separator("Massive allocation goes out of scope");
}

void debug_test(){
    gc_cycle();
    gc_print_heap();
    getchar();
}

void trigger_test(void(*function)()){
    function();
    debug_test();
}

typedef void (*TestFunc)(void);

typedef struct {
    const char *name;
    TestFunc func;
} TestEntry;

TestEntry tests[] = {
    { "manual_gc_cycle", gc_cycle},
    { "test_basic", test_basic },
    { "test_large_alloc", test_large_alloc },
    { "test_many_small", test_many_small },
    { "test_dangling", test_dangling },
    { "test_linked_list", test_linked_list },
    { "test_cycle", test_cycle },
    { "test_interior_pointer", test_interior_pointer },
    { "test_stack_roots", test_stack_roots },
    { "test_overwrite", test_overwrite },
    { "test_fragmentation", test_fragmentation },
    { "test_repeated_gc", test_repeated_gc },
    { "test_deep_stack", test_deep_stack },
    { "test_false_pointers", test_false_pointers },
    { "test_alignment", test_alignment },
    { "test_massive", test_massive }
};

int num_tests = sizeof(tests) / sizeof(tests[0]);

void select_option(int input){
    TestFunc function = test_basic; // default

    if (input >= 1 && input <= num_tests) {
        function = tests[input - 1].func;
    }

    trigger_test(function);
}

void print_prompt() {
    printf("\n=== Select a test to run ===\n");
    printf(" 0) Exit\n");

    for (int i = 0; i < num_tests; i++) {
        printf("%2d) %s\n", i + 1, tests[i].name);
    }

    printf("Enter your choice: ");
}

// ---------- Main ----------
int main() {
    gc_activate_debug();
    int input = 0;

    do{
        print_prompt();
        scanf("%d", &input);
        if(input > 0)
            select_option(input);
    } while(input != 0);

    separator("Final Heap State");
    gc_print_heap();

    return 0;
}
