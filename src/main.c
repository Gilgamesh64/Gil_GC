#include <stdio.h>
#include "ggc.h"

int main(){
    int* gigi = gc_malloc(sizeof (int));
    int* tony = gc_malloc(sizeof (int));
    printf("%p\n", gigi);
    printf("%p\n", tony);

    *gigi = 4;
    *tony = 10;

    gc_free(tony);

    printf("%d\n", *gigi);
    printf("%d\n", *tony);

}