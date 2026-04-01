# Garbage Collector Project Recap

Greetings, fellow garbage collector enjoyers!

This document contains a recap about this project, silly bugs, and their solutions.

**WARNING:** The style of this rant is **absolutely 0% formal or academic**. If you’re a teacher… well… I hope I can put a smile on your face. I promise this won’t be the “end result” of a formal report.

After all that yapping, let's get right onto it.

## Goals

First of all, my main goal is to **write a Garbage Collector**.
There are many kinds of GCs out there. Mine is:

- **Mark and Sweep:** first you mark every reachable block, then you delete all unreachable ones
- **Stop-the-World:** while the GC runs the program is stopped
- **Conservative:** if something feels like a pointer, let's say it's a pointer

And you might say: but why should we interpret everything as pointers?<br> 
Well, C has 0 overhead to store variables and it just so happens that pointers and other types **share the same memory representation**<br>
So we cannot distinguish pointers from other data types <br>
Interpreting everything as a pointer ensures we do actually check all pointers, but there is a catch:<br>
Remember when i said everything is a pointer? Let's say i store some random data (maybe a string or an int, doesn't matter) in memory and it just so happens to have the same representation to one of the allocated blocks, in that case the block will not be deallocated even if there is no pointer pointing to it. <br>
This is why it's **conservative** meaning it guarantees to deallocate blocks that are 100% not in use anymore

**DISCLAIMER**: This Garbage Collector is designed to operate exclusively on memory blocks allocated through the `gc_malloc` function. The author expressly disclaims any responsibility or liability for memory that is allocated by any other means and remains unclaimed or unmanaged. Any allocations performed with standard `malloc` or other allocation mechanisms fall outside the scope of this Garbage Collector and are not subject to its management or reclamation processes.

## The allocator

I know what you are thinking: "Are you kidding? You said we were building a super cool Garbage Collector, why do we need an allocator?"<br>
And no, don't be shy, it's fine, i can feel you, we'll have to wait before we get to the GC<br>
But trust me, we won't get bored here, let's get allocatin'<br><br>
Jokes aside we need a custom allocator because we have to store some **metadata** over heap allocated blocks in order for the GC to function.<br>
First of all, the allocator is just a man in the middle between the user requesting memory and the OS giving them some, but it cannot create memory out of thin air<br>

### Getting memory from the OS

There are 2 main functions to request memory from the OS:
1. sbrk
2. mmap

- I chose to use sbrk just because mmap felt harder and fuck it who cares

No i'm kidding, partially, mmap works wonders for large allocations while sbrk is more efficient for smaller ones, since for now optimization is not my goal, using just sbrk is fine.<br>
In the future I'll migrate to using sbrk for small allocations and mmap for larger ones but for now it would be overkill

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

Since our computer likes page aligned data, let's make it happy so when the ai revolution happens we will be spared<br>
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
    try asking gently to the OS
    if OS successfully gave memory:
        extend heap
        allocate block
    else: 
        there is nothing else to do, we tried everything
        return null
```


#### We hate fragmentation

Since freeing is purely logical (setting a boolean), after some frees our memory will look like a sponge <br>
To avoid that we can check if the blocks adjacent to the one we want to free are free as well: in that case we merge the two blocks. This procedure is called to `coalesce` and it allows to avoid fragmentation without moving memory.

#### No pls don't free random ass pointers

Since the user is stoopid and you always have to account its stupidity I added some nice safety features, now if you try to call `gc_free` passing a pointer that was not allocated by `gc_malloc` you get a nice and lovely error message instead of a seg fault, how thoughtful of me to help the user in such a positive and non-aggressive way.

## Implementing the Garbage Collector

Finally, it was time to implement the actual GC which is made of two phases:

- **Mark phase:** where you mark active blocks
- **Sweep phase:** where you free non marked blocks

There are 3 major locations where pointers can point to heap-allocated objects:

1. **The stack**
2. **Registers**
3. **The heap itself**

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
}
```

Since this function runs **before `main`**, we can capture the **highest possible reachable address** before the compiler can even try to screw everything up.

#### Alternative but specific to gcc/Clang
```c
stack_bottom/stack_top = __builtin_frame_address(0)
```
Only **after** all of that mess i discovered this function... Oh well, i suppose i'll use that now <br>
Hours to come up with some real cooking, and then i discover a better way to do it...
It's slightly more precise then creating a variable and taking its address but it will not work with **MSVC**

### Registers

Sometimes, pointers to heap blocks could live in registers<br>
The compiler is free to optimize code and often will not even write variables in memory, keeping them in registers is way faster. But what happens if the compiler optimizes away a heap pointer?<br>
We scan the stack, the pointer is **not** there because it lives in registers, and we sweep away its block. When the program then tries to access it: boom, undefined behaviour. <br>
Shit, how many edge cases do i have to handle.<br>
We then need **some** way to check registers in the mark phase. How can we do that? Checking registers one by one is a giant footgun, so many things can go wrong and it would be platform dependent.<br>
The solution is a C function to force the OS to write all register data inside a bufffer to then scan it:

##### setjmp
```c
jmp_buf env;
setjmp(env); //forces the os to write all caller register data into env
uintptr_t* reg = (uintptr_t*)env;
uintptr_t* end = (uintptr_t*)((char*)env + sizeof(env));

for (; reg < end; reg++) {
    try_mark((void*)*reg);
}
```

To test if this scanning properly works we can write something like this in our main function:
```c
register void* ptr_reg asm("rbx") = gc_malloc(sizeof(int));
```

This line stores into the `rbx` register the pointer returned by gc_malloc`. Without register scanning, the first GC cycle would clear the memory block, but with this extra piece of code, we can assure the memory is still alive.

### Heap scanning
Oh god, another edge case... Let's see what is happening this time<br>
Imagine a liked-list, it's composed by one pointer from the stack to the head of the linked-list, then each node is connected to the next one, and where is this **next** pointer located?
Why of chourse in a location we didn't handle yet: its payload. <br>

When we successfully mark a block, we now have to check if its payload contains pointers to other heap allocated blocks, in that case we must free them too. <br>
Then we recursively scan all pointers we found until we find a block that contains no pointer to blocks, then we are sure to have successfully reached the end on the list. Other data structures **shuld** work too. I will try debug this **garbage** wink wink, with other weirder data structures like graphs but it **should** work.

## Final improvements

## Silly but interesting bugs

```c
int* test_allocation(){
    volatile int* a = gc_malloc(sizeof(int));
    return gc_malloc(sizeof(int));
}
void test_out_of_scope(){
    volatile int* b = test_allocation();
}
void test_gc(){
    test_out_of_scope();
    gc_cycle();
}
```
For some reason `gc_cycle()` was marking both a and b even if they both went out of scope.
And even weirder: calling `print_heap()` in between `test_out_of_scope()` and `gc_cycle()` was working perfectly.
So printing the heap made the GC work? Does not make sense.
And then i thought: "Is it really the print function that makes the GC work correctly?"

#### i tried this:
```c
void test_new_stack_frame(){
    int a,b;
}

void test_gc(){
    test_out_of_scope();
    test_new_stack_frame();
    gc_cycle();
}
```
And it worked! Somehow... <br>
Well, when a stack frame ends, all data it contained is NOT deleted, we just move the **stack pointer** before the start of the stack frame that is being closed. <br>
So when we exit a function, all variables contained in it stay written on the stack until another stack frame that overwrites them is created. For this reason calling any function after `test_out_of_scope()` makes the GC work because it overwrites both stack frames of the previous functions! <br>
Previously i was approximating the top of the stack so it was possible to reach past the stack pointer, finding pointers that were supposedly just been destroyed. After updating the line to find stack borders, the stack to is more prcise so this issue doesn't happen now<br> 
Still, it's a nice and intreresting behaviour to note.

