#include <stdio.h>
#include "ggc.h"

int main(){
    int* gigi = gc_malloc(sizeof (int));
    printf("%p", gigi);

}