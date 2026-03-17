#include <stdio.h>
#include "ggc.h"

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
}