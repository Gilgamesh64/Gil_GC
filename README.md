# Gil_GC
Custom Garbage Collector written in C. Hopefully i will finish it :)

It's composed of a custom malloc implementation to store metadata over heap allocated blocks

gcc src/main.c src/ggc.c -Wall -Wextra -o gc.o
