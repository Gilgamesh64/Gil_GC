#include <stdio.h>
#include "ggc.h"
#include <stdlib.h>

int main(){
    int* gigi = gc_malloc(sizeof (int));
    int* tony = gc_malloc(sizeof (int));
    int* beppi = gc_malloc(sizeof (int));

    print_heap();

    gc_free(gigi);
    gc_free(tony);
    gc_free(beppi);

    print_heap();
}