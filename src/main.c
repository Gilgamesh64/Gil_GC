#include <stdio.h>
#include "ggc.h"
#include <stdlib.h>

int main(){
    print_heap();

    int* gigi = gc_malloc(sizeof (int));
    int* tony = gc_malloc(sizeof (int));
    int* beppi = gc_malloc(sizeof (int));

    print_heap();

    gc_free(gigi);
    gc_free(tony);

    print_heap();

    gc_free(beppi);

    print_heap();

    long* gino = gc_malloc(sizeof(long) * 12);

    print_heap();

    gc_free(gino);

    print_heap();

    int* a = malloc(sizeof(int));
    gc_free(a);

    int b = 4;
    int* c = &b;
    gc_free(c); 
}