# Garbage Collector Project Recap

Greetings, fellow garbage collector enjoyers!

This document contains a recap about this project, silly bugs, and their solutions.

**WARNING:** The style of this rant is **absolutely 0% formal or academic**. If you’re a teacher… well… I hope I can put a smile on your face. I promise this won’t be the “end result” of a formal report.

After all that yapping, let's get right into it.

## Goals

First of all, my main goal is to **write a Garbage Collector**.
There are many kinds of GCs out there. Mine is:

- **Mark and Sweep:** first you mark every reachable block, then you delete all unreachable ones
- **Stop-the-World:** while the GC runs the program is frozen
- **Conservative:** if something feels like a pointer, let's say it's a pointer

And you might say: but why should we interpret everything as pointers?<br> 
Well, C has 0 overhead to store variables and it just so happens that pointers and other types **share the same memory representation**<br>
So we cannot distinguish pointers from other data types <br>
Interpreting everything as a pointer ensures we do actually check all pointers, but there is a catch:<br>
Remember when I said everything is a pointer? Let's say I store some random data (maybe a string or an int, doesn't matter) in memory and it just so happens to have the same representation to one of the allocated blocks, in that case the block will not be deallocated even if there is no pointer pointing to it. <br>
This is why it's **conservative**, meaning it guarantees to deallocate blocks that are 100% not in use anymore.

**DISCLAIMER**: This Garbage Collector is designed to operate exclusively on memory blocks allocated through the `gc_malloc` function. The author expressly disclaims any responsibility or liability for memory that is allocated by any other means and remains unclaimed or unmanaged. Any allocations performed with standard `malloc` or other allocation mechanisms fall outside the scope of this Garbage Collector and are not subject to its management or reclamation processes.

## The Allocator

I know what you are thinking: "Are you kidding? You said we were building a super cool Garbage Collector, why do we need an allocator?"<br>
And no, don't be shy, it's fine, I can feel you, we'll have to wait before we get to the GC<br>
But trust me, we won't get bored here, let's get allocatin'<br><br>
Jokes aside we need a custom allocator because we have to store some **metadata** over heap-allocated blocks in order for the GC to function.<br>
First of all, the allocator is just a man in the middle between the user requesting memory and the OS giving them some, but it cannot create memory out of thin air.<br>

### Getting memory from the OS

There are 2 main functions to request memory from the OS:
1. sbrk
2. mmap

- I chose to use `sbrk` just because `mmap` felt harder and fuck it who cares

No I'm kidding, partially, `mmap` works wonders for larger allocations while `sbrk` is more efficient for smaller ones. Since for now optimization is not my goal, using just `sbrk` is fine.<br>
In the future I'll migrate to using `sbrk` for smaller allocations and `mmap` for larger ones but for now it would be overkill.

#### morecore function with only sbrk
```c
static void *request_from_os(size_t size) {
    void *p = sbrk(0);
    if (sbrk(size) == (void *) -1) //don't ask, just don't
        return NULL;
    return p;
}
```

It's already documented in depth so I ain't gonna repeat myself, finger consumption is real. <br>

#### Aligning to pages
```c
static inline size_t align8(size_t s) {
    return (s + 7) & ~((size_t)7); //magic abracadabra bit hack
}
```

Since our computah likes page aligned data, let's make it happy so when the ai revolution happens we will be spared.<br>
Again due to finger typing consumption just go check comments over this function out. Don't be lazy.


### malloc and free

We have a chunk of memory given by the OS, how should we use it? <br>
Let's say I get 1MB of memory by the OS, the user requests 1B and I give them the whole block... not very efficient isn't it?<br>
When the user asks for some memory we find a chunk of memory big enough for the request, if it's too big we split it and allocate only what is needed, the rest stays **free**. <br>
But then how do we connect used and free blocks? Each of them with possibly a different size? With a **linked list**

#### Creating the list of blocks
```c
typedef struct block {
    size_t size;
   	bool free;
    struct block* next;
    struct block* prev;
} block_t; //just your standard linked-list struct
```
This allows to allocate an arbitrary amount of data and to free individual blocks without having to move memory. <br>
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

##### Best pseudo-code ever made
```
when freeing:

    look left
    if free:
        join

    look right
    if free:
        join
```
        
The reason being we want to avoid having contiguous small free blocks, a single big chunk that can be later split is way better.

#### No pls don't free random ass pointers

Since the user is stoopid and you always have to account for its stoopidity I added some nice safety features, now if you try to call `gc_free` passing a pointer that was not allocated by `gc_malloc` you get a nice and lovely error message instead of a seg fault, how thoughtful of me to help the user in such a positive and non-aggressive way.<br>
Don't question about this famous message.

## Implementing the Garbage Collector

Finally, it was time to implement the actual GC which is made of two phases:

- **Mark phase:** where you mark active blocks
- **Sweep phase:** where you free non marked blocks

There are 4 major locations where pointers can point to heap-allocated blocks:

1. **The stack**
2. **Registers**
3. **.data segment**
4. **The heap itself**

## Mark phase

### Stack Scanning

I first tried to retrieve the **stack borders**.

From now on:

- `stack bottom` = highest possible address inside the stack
- `stack top` = current stack pointer location

#### Finding the stack top

There are two ways:

1. **Direct assembly instruction (not portable):**
```c
asm("mov %%rsp, %0" : "=r"(stack_top)); //no I don't know how it works, just googled it
```

2. **An assumption-based method:**
```c
char a;
void* stack_top = &a;
```
   If I initialize a variable and retrieve its address, it points to the **last created variable**.
   Well duh, there is no way, if I create a variable, a variable gets created. <br>
   But it's actually the key.<br>
   This works because as the variable will be created **inside the last possible stack frame**, taking its address means approximating the end of the stack.

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
A remarkably discourteous individual this compiler huh?

#### The “before main” trick

If the compiler can shuffle things in `main`, what if we take a snapshot **before `main` runs**?

```c
void* stack_bottom;

__attribute__((constructor)) //just a trick to run a function between .start and main
void before_main() {
    char a;
    stack_bottom = &a;
}
```

Since this function runs **before `main`**, we can capture the **highest possible reachable address** before the compiler can even try to screw anything up.

#### Alternative but specific to gcc/Clang
```c
stack_bottom/stack_top = __builtin_frame_address(0)
```
Only **after** all of that mess I discovered this function... Oh well, I suppose I'll use that now <br>
Hours to come up with some real cooking, and then I discover a better way to do it...
It's slightly more precise then creating a variable and taking its address but it will not work with **MSVC**. <br>
Not that it matters... MSVC sucks anyways.

### Registers

Sometimes, pointers to heap-allocated blocks could live in registers.<br>
The compiler is free to optimize code and it often will not even write variables in memory as keeping them in registers is way faster. But what happens if the compiler optimizes away a heap pointer?<br>
We scan the stack, the pointer is **not** there because it lives in registers, and we sweep away its block. When the program then tries to access it: boom, undefined behavior. <br>
Shit, how many edge cases do I have to handle.<br>
We then need **some** way to check registers in the mark phase. How can we do that? Checking registers one by one is a giant footgun, so many things can go wrong and it would be platform dependent.<br>
The solution is a C function to force the OS to write all register data inside a buffer to then scan it:

##### setjmp
```c
static void scan_range(void* start, void* end) {
    for (uintptr_t* p = start; p < (uintptr_t*)end; p++) {
        try_mark(p);
    }
}
jmp_buf env;
setjmp(env); //forces the os to write all caller register data into env
uintptr_t* reg = (uintptr_t*)env;
uintptr_t* reg_end = (uintptr_t*)((char*)env + sizeof(env));

scan_range(reg, reg_end);
```

To test if this scanning properly works we can write something like this in our main function:
```c
register void* ptr_reg asm("rbx") = gc_malloc(sizeof(int));
```

This line stores the pointer returned by `gc_malloc` into the `rbx` register. Without register scanning, the first GC cycle would clear the memory block, but with this extra piece of code, we can assure the memory is still alive.

### Data segment
For once this was pretty linear to implement, somehow.<br>
```c
extern char __data_start;
extern char _edata;
                          //magic variables filled by the linker. Don't ask me how, do I look like a linker to you?
extern char __bss_start;
extern char _end;
```

I just put these variables as globals, I found out these variables get filled by the `linker` when the program gets linked.<br>
Do I really need to repeat myself on how to scan a range of pointers?<br>
REALLY? You are such a moron, fine:

#### How to scan globals for morons
```c
static void scan_range(void* start, void* end) {
    for (uintptr_t* p = start; p < (uintptr_t*)end; p++) {
        try_mark(p);
    }
}
scan_range(&__data_start, &_end);
```

### Heap scanning
Oh god, another edge case... Let's see what is happening this time.<br>
Imagine a linked-list: it has one pointer from the stack to the head of the list, and then each of the following nodes is connected to its successor by a `next` pointer. <br> 
And where is this `next` pointer located?
Why of course in a location we didn't handle yet: its payload. <br><br>

When we successfully mark a block, we have to check if its payload contains pointers to other heap allocated blocks, in which case we must free them too. <br>
Then we recursively scan all the pointers we found until we find a block that contains no pointers to blocks, in that case we are sure to have successfully reached the end on the list. Other data structures **should** work too. I will try debugging this **garbage** wink wink, with other weirder data structures like graphs but it **should** work.

#### full gc_mark implementation, more definitely not professional pseudo-code
```c
gc_mark(){
    foreach ptr in stack:
        try_mark(ptr)
    
    hi OS, I have this really beautiful buffer called env
    can you please put all your registers inside env? No? Fuck off imma force you to do it anyway
    
    foreach ptr in env:
        try_mark(ptr)

    hey linker, since you very kindly put .data and .bss borders in my variables I can now scan them
    foreach ptr between these ptrs:
        try_mark(ptr)
}

try_mark(ptr){
    if ptr does not look like a ptr:
        skip it, not even a pointer...

    if ptr is not between heap borders:
        skip it, definitely not a good pointer

    else check more thoroughly and if it really is a heap pointer:
        block = block pointed by ptr
        mark block
        mark_contents(block) //mark eventual pointers it contains
}

mark_contents(block){
    foreach ptr in block contents:
        try_mark(ptr)
}
```

## Sweep phase
Finally! Something simple and definitely not full of crap. <br>
The sweep phase is non ironically super simple

``` I'm getting used to this garbage pseudo-code
foreach block in heap:
    if not marked:
        free
```

Yes, that's it bye.<br><br><br>
What now? It's not cool enough? You wanted some revolutionary algorithm? It's not the place for you then. <br><br><br><br><br>
Still here? Huh, I suppose I'm gonna make it more complex just for you.<br>
**coming soon**

## Final improvements

### calloc and realloc
In order to be a real and (obviously) professional garbage collector I also need to provide implementations for both of them.<br>
Doing so, you can include this library into an existing project and all heap allocating functions provided by the standard library would be replaced by my implementation, thus enabling my gc to operate. <br>

There is nothing really magic happening here, it's just standard calloc and realloc

### Interior pointers crap
What happens if the user does pointer arithmetic on their pointers? Spoiler: everything blows up.<br>
Even the standard library provides no standards on what happens if you move pointers<br>
Since I'm obviously better than those fiveheads who wrote C's standard library, I will allow users to shift pointers freely<br>
My GC wants to be as **conservative** as possible, thus having even a single edge case where the GC would break is unacceptable<br>
This possibility comes with a cost tho so it's possible to disable it through a setting

```c
int* arr = gc_malloc(sizeof(int) * 4);
arr++; //is allowed

arr = null; //arr would still be collected
```

### Finalizers
Lemme tell you a spooky story: <br>
Once upon a time there was an object called Timmy. <br>
Timmy was such a wholesome object, everyone loved him. <br>
One day Timmy died (R.I.P Timmy). <br>
Just before the GC was about to sweep him away completely, his family contacted the object necromancer. <br>
He pointed his grey finger towards poor Timmy and shouted: look, he is not dead, I'm pointing at him!!<br>
The GC was very confused... "You know this will serve nothing right? Timmy is dead".<br>
As soon as the GC spoke, Timmy woke up. <br>
"Wait... how?" Said the now extremely confused GC. <br>
"We paid for his life insurance" Said Timmy's family. <br>
"The first death is free" <br>
The GC decided not to investigate and proceeded his sweeping like nothing happened. <br>
"They ain't paying me enough for this shit, resurrecting objects now? What then? Flying pointers?

##### Disclaimer
I know C is not object oriented so technically Timmy would not be an object but an: `inhales` **heap allocated block of memory of fixed size owned and managed by the garbage collector who can reclaim the block if there are no pointers pointing to it** <br>
The story would have been really boring if I had to repeat all of that crap all the time so I just called him Object. <br>
I could sense from a mile away you little imperative purists who were already screaming: Object and C in the same sentence!?<br>
In this section I'm gonna call them objects anyway.  

#### The idea
As you might ask: Why should you ever want to resurrect objects? <br>
Welp, You don't. Object resurrection is a consequence of the possibility of using a certain feature we are about to discuss.<br>
In C it often happens to manually call a destructor right before some pointer is about to be freed. <br>
Maybe you had to close a file or a socket, so in idiomatic C you would write something like:
```c
some_type* foo = malloc(sizeof(some_type));
...
release_resources(foo);
free(foo); 
```
But what happens when you add into the equation a Garbage Collector? <br>
Now freeing the block is his responsibility so you lose control over when the pointer will go out of scope. <br>
But as we said, we have to free the block's internal resources, so how do we do that? <br>
The idea is to attach the destructor to the memory block so when the GC is about to sweep it, it can call the function first. <br>
These destructors, or callback functions are called Finalizers and they are used to perform some operations right before clearing the block. <br>
```c
void greet(void* ptr){
    printf("Goodbye, you cruel world, %p\n", ptr);
}
some_type* foo = gc_malloc(sizeof(some_type));
gc_add_finalizer(foo, greet);
```
In this example the greet function will be called only **after** the block is freed.<br>
In this way you don't really know when the finalizer will be called but you are sure it will be called before the block is freed.<br>
This is such a great system, nothing will ever go wrong!

#### What can go wrong
The finalizers are functions that take as argument a single void pointer (which will be the pointer to the block) and return void.<br>
We obviously must expose the pointer because we will probably have to perform some operations on the block.<br>
But what happens if we do something silly? <br>
```c
some_type* bar = NULL;

void greet(void* ptr){
    bar = ptr;
}

void fn(){
    some_type* foo = gc_malloc(sizeof(some_type));
    gc_add_finalizer(foo, greet);
}
```
Here foo is created and the function greet is attached to it. <br>
Once foo goes out of scope and the GC runs, the block will have 0 pointers pointing to it. So the GC will not mark it. <br>
Then at the sweep phase the GC will realize the block has a finalizer, so it will call the finalizer. <br>
The finalizer attaches a pointer to the block. <br>
Then it will free the block since there are no pointers pointing to it... <br>
WAIT WHAT? We just reattached a pointer? Surely you can't do that.<br>
We now have a dangling pointer! Yeppeeeee. <br>
How do we fix this? Welp, some GCs just suggest the programmer not to do that, kinda lame isn't it? <br>
Soo I went on a journey searching for how to allow object resurrection to the user (it's really funny) without breaking anything. <br>
The solution was to allow each block to call its finalizer only once and to have a teeny tiny state machine around blocks that are about to be swept.
After calling a finalizer you preemptively keep the block alive for one gc cycle and set its flag (has_finalized) to true.<br>
Next GC cycle, if the block was resurrected it will just be marked as usual, if not the GC will detect the block holds a finalizer, but it has already called it; meaning it wasn't resurrected, so we are sure to sweep it without consequences.

#### How are they implemented
One way was to add a function pointer to each block's header, simple as that. <br>
My main issue was that objects with a finalizer are a minority. With this idea every header would have to pay the extra memory to hold the function pointer, even if it does not point to any function. <br>
I then moved to keeping finalizers in another struct: a linked list
```c
typedef struct finalizer_entry {
    block_t* ptr;
    gc_finalizer_t fn;
    struct finalizer_entry* next;
} finalizer_entry_t;

static finalizer_entry_t* finalizers_head;
```

Here we can keep track of all finalizers without adding extra memory to all headers.


### Customization options 

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
And then I thought: "Is it really the print function that makes the GC work correctly?"

#### I tried this:
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
Previously I was approximating the top of the stack so it was possible to reach past the stack pointer, finding pointers that were supposedly just been destroyed. After updating the line to find stack borders, the stack to is more precise so this issue doesn't happen now<br> 
Still, it's a nice and interesting behavior to note.