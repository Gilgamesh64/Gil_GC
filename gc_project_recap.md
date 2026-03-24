# Garbage Collector Project Recap

Greetings, fellow garbage collector enjoyers!

This document contains a recap about this project, silly bugs, and their solutions.

**WARNING:** The style of this rant is **absolutely 0% formal or academic**. If you’re a teacher… well… I hope I can put a smile on your face. I promise this won’t be the “end result” of a formal report.

After all that yapping, let's get right onto it.

## Goals

First of all, my main goal is to **write a Garbage Gollector**.
There are many kinds of GCs out there. Mine is:

- **Mark and Sweep:** first you mark every reachable block, then you delete all unreachable ones
- **Stop-the-World:** while the GC runs the program is stopped
- **Conservative:** if something feels like a pointer, let's say it's a pointer

And you might say: but why should we interpret everything as pointers?<br> 
Well, C has 0 overhead to store variables and it just so happens that pointers and other types **share the same memory representation**<br>
So we cannot distinguish pointers from other data types <br>
Interpreting everything as a pointer ensures we do actually check all pointers but there is a catch<br>
Remember when i said everything is a pointer? Let's say i store some random data (maybe a string or an int, doesn't matter) in memory and it just so happens to have the same representation to one of the allocated blocks, in that case the block will not be deallocated even if there is no pointer pointing to it. <br>
This is why it's **conservative** meaning it guarantees to deallocate blocks that are 100% not in use anymore

**DISCLAIMER**: This Garbage Collector is designed to operate exclusively on memory blocks allocated through the `gc_malloc` function. The author expressly disclaims any responsibility or liability for memory that is allocated by any other means and remains unclaimed or unmanaged. Any allocations performed with standard `malloc` or other allocation mechanisms fall outside the scope of this Garbage Collector and are not subject to its management or reclamation processes.

## The allocator

I know what you are thinking: "Are you kidding? You said we were building a super cool Garbage Collector, why do we need an allocator?"<br>
And no, don't be shy, it's fine, i can feel you, we'll have to wait before we get to the GC<br>
But trust me, we won't get bored here, let's get allocatin'<br><br>
Jokes aside we need a custom allocator because we have to store some **metadata** over heap allocated blocks in order for the GC to function.<br>
First of all, the allocator is just a man in the middle between the user requesting memory and the os giving them some but it cannot create memory out of thin air<br>

### Getting memory from the OS

There are 2 main functions to request memory from the OS:
1. sbrk
2. mmap

- I chose to use sbrk just because mmap felt harder and fuck it who cares

No i'm kidding, partially, mmap works wonders for large allocations while sbrk is more efficient for smaller ones, since for now optimization is not my goal, using just sbrk is fine.<br>
In the future I migrate to using sbrk for small allocations and mmap for larger ones but for now it would be overkill

#### morecore function with only sbrk
```c
static void *request_from_os(size_t size) {
    void *p = sbrk(0);
    if (sbrk(size) == (void *) -1)
        return NULL;
    return p;
}
```

It's already documented in depth so I ain't gonna repeat myself, finger consumption is real <br>

#### Aligning to pages
```c
static inline size_t align8(size_t s) {
    return (s + 7) & ~((size_t)7);
}
```

Since or computer likes page aligned data, let's make it happy so when the ai revolution happens we will be spared<br>
Again due to finger typing consumption just go check comments over this function out. Don't be lazy.


### malloc and free

We have a chunk of memory given by the OS, how should we use it? <br>
Let's say i get 1MB of memory by the OS, the user requests 1B and i give them the whole block... not very efficient isn't it?<br>
When the user asks for some memory we find a chunk of memory big enough for the request, if it's too big we split it and allocate only what is needed, the rest stays free. <br>
But then how do we connect used and free blocks? Each of them with possibly a different size? A **linked list**

#### Creating the list of blocks
```c
typedef struct block {
    size_t size;
   	bool free;
    bool marked;
    struct block *next;
    struct block *prev;
} block_t;

extern block_t *heap_head;
```
This allows to allocate an arbitrary amout of data and to free individual blocks without having to move memory. <br>
The actual `malloc` is pretty straight forward: 

##### definitely not professional pseudo-code
```
search for a free block

if found:
    if too big:
        split
    allocate block
else: 
    try asking gently to the os
    if OS successfully gave memory:
        extend heap
        allocate block
    else: 
        there is nothing else to do, we tried everything
        return null
```


#### We hate fragmentation

Since freeing is purely logical (setting a boolean), after some frees our memory will look like a sponge <br>
To avoid that we can look at the block that is being freed neighbours if any of them is free, in that case we merge the two blocks. This procedure is called to `coalesce` and it allows to avoid fragmentation without moving memory.

#### No pls don't free random ass pointers

Since the user is stoopid and you always have to account its stupidity I added some nice safety features, now if you try to call `gc_free` passing a pointer that was not allocated by `gc_malloc` you get a nice and lovely error message instead of a seg fault, how thoughtful of me to help the user in such a positive and non-aggressive way.

## Implementing the Garbage Collector

Finally, it was time to implement the actual GC which is made of two phases:

- **Mark phase:** where you mark active blocks
- **Sweep phase:** where you free non marked blocks

There are 3 major locations where pointers can point to heap-allocated objects:

1. **The stack**
2. **The heap itself**
3. **Registers**

### Stack Scanning

I first tried to retrieve the **stack borders**.

From now on:

- `stack bottom` = highest possible address inside the stack
- `stack top` = current stack pointer location

#### Finding the stack top

There are two ways:

1. **Direct assembly instruction (not portable):**
```c
asm("mov %%rsp, %0" : "=r"(stack_top));
```

2. **An assumption-based method:**
```c
char a;
void* stack_top = &a;
```
   If I initialize a variable and retrieve its address, it points to the **last created variable**.
   Well duh, there is no way, if i create a variable, a variable gets created. <br>
   But it's actually the key.<br>
   This works because the variable will be created **inside the last possible stack frame**, taking its address means approximating the end of the stack.

#### Finding the stack bottom

I tried the same method:
```c
int main() {
    char test = 0;
    void* stack_bottom = &test;

    int a;
}
```

But for some reason `a`’s address was **bigger than `test`’s** <br>
Theory: When you allocate on the stack, each allocation is placed at the **bottom of the current stack frame**, right? <br>
Reality: Nope. The compiler can shuffle variable order inside the stack frame. Disabling optimizations would be terrible. <br>
A remarkably discourteous individual this compiler huh.

#### The “before main” trick

If the compiler can shuffle things in `main`, what if we take a snapshot **before `main` runs**?

```c
void* stack_bottom;

__attribute__((constructor))
void before_main() {
    char a;
    stack_bottom = &a;
    printf("Stack pointer before main: %p\n", stack_bottom);
}
```

Since this function runs **before `main`**, we can capture the **highest possible reachable address** before the compiler can even try to screw everything up.

### heap scanning


### REGISTERS

