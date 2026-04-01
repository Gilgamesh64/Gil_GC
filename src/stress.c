#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "ggc.h"

#define LARGE_SIZE (1024 * 1024 * 10) // 10 MB
#define SMALL_COUNT 10
#define MIXED_COUNT 50000

// ---------- Test Helpers ----------
void separator(const char *name) {
    printf("\n================ %s ================\n", name);
}

// ---------- Tests ----------

// 1. Basic allocation & survival
void test_basic() {
    separator("Basic Allocation");
    int *a = (int *)gc_malloc(sizeof(int));
    gc_cycle();
    separator("Basic Allocation exit scope");
}

// 2. Large allocation
void test_large_alloc() {
    separator("Large Allocation");
    char *buf = (char *)gc_malloc(LARGE_SIZE);
    memset(buf, 'A', LARGE_SIZE);
    gc_cycle();
    separator("Large Allocation exit scooe");
}

// 3. Many small allocations
void test_many_small() {
    separator("Many Small Allocations");
    void *ptrs[SMALL_COUNT];
    for (int i = 0; i < SMALL_COUNT; i++) {
        ptrs[i] = gc_malloc(16);
    }
    gc_cycle();
    separator("Many small allocations exit scope");
}

// 4. Dangling references (should be collected)
void test_dangling() {
    separator("Dangling References");
    int *a = (int *)gc_malloc(sizeof(int));
    *a = 99;
    a = NULL;
    gc_cycle();
    printf("Dangling object should be freed\n");
}

// 5. Linked list
typedef struct Node {
    struct Node *next;
} Node;

void test_linked_list() {
    separator("Linked List");
    Node *head = NULL;
    for (int i = 0; i < SMALL_COUNT / 2; i++) {
        Node *n = gc_malloc(sizeof(Node));
        n->next = head;
        head = n;
    }
    gc_cycle();
    separator("Linked list out of scope");
}

// 6. Cyclic references (classic GC test)
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
    b = NULL;

    gc_cycle();
    separator("Cycle goes out of scope");
}

// 7. Interior pointers (pointer to middle of block)
void test_interior_pointer() {
    separator("Interior Pointer");
    char *buf = (char *)gc_malloc(100);
    strcpy(buf, "Hello GC");

    char *mid = buf + 3;
    gc_cycle();

    printf("Interior pointer content: %s\n", mid);
}

// 8. Stack pointer retention
void test_stack_roots() {
    separator("Stack Roots");
    int *a = (int *)gc_malloc(sizeof(int));
    *a = 123;

    gc_cycle();
    printf("Stack root value: %d\n", *a);
}

// 9. Overwrite pointer (lost reference)
void test_overwrite() {
    separator("Overwrite Pointer");
    int *a = (int *)gc_malloc(sizeof(int));
    *a = 5;

    a = (int *)gc_malloc(sizeof(int));
    *a = 10;

    gc_cycle();
    printf("Only latest allocation should remain: %d\n", *a);
}

// 10. Fragmentation stress
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
    printf("Fragmentation test complete\n");
}

// 11. Repeated GC cycles
void test_repeated_gc() {
    separator("Repeated GC");
    for (int i = 0; i < 100; i++) {
        void *p = gc_malloc(64);
        (void)p;
        gc_cycle();
    }
    printf("Repeated GC cycles done\n");
}

// 12. Deep recursion (stack scanning stress)
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

// 13. False pointers (random data looking like pointer)
void test_false_pointers() {
    separator("False Pointers");
    uintptr_t *fake = (uintptr_t *)gc_malloc(sizeof(uintptr_t) * 100);

    for (int i = 0; i < 100; i++) {
        fake[i] = (uintptr_t)rand();
    }

    gc_cycle();
    printf("False pointer test complete\n");
}

// 14. Alignment edge cases
void test_alignment() {
    separator("Alignment");
    char *a = (char *)gc_malloc(3);
    char *b = (char *)gc_malloc(5);
    char *c = (char *)gc_malloc(7);

    a[0] = 'A'; b[0] = 'B'; c[0] = 'C';

    gc_cycle();
    printf("Alignment test: %c %c %c\n", a[0], b[0], c[0]);
}

// 15. Massive allocation + GC
void test_massive() {
    separator("Massive Allocation");
    for (int i = 0; i < 1000; i++) {
        void *p = gc_malloc(1024 * 1024);
        memset(p, i, 1024);
    }
    gc_cycle();
    printf("Massive allocation test done\n");
}

void test(){
    gc_cycle();
    gc_print_heap();
    getchar();
}

// ---------- Main ----------
int main() {
    gc_activate_debug();

    test_basic();
    test();

    //test_large_alloc();
    test();

    //test_many_small();
    test();

    test_dangling();
    test();

    test_linked_list();
    test();

    test_cycle();
    test();

    //test_interior_pointer();
    test();

    test_stack_roots();
    test();

    test_overwrite();
    test();

    //test_fragmentation();
    test();

    //test_repeated_gc();
    test();

    test_deep_stack();
    test();

    test_false_pointers();
    test();

    test_alignment();
    test();

    //test_massive();
    test();


    separator("Final Heap State");
    gc_print_heap();

    return 0;
}
