#include <stdio.h>
#include "ggc.h"

int main(){
    int* gigi = gc_malloc(sizeof (int));
    int* tony = gc_malloc(sizeof (int));
    printf("%p\n", gigi);
    printf("%p\n", tony);

    gc_free(tony);

    printf("%p\n", gigi);
}