# Garbage Collector Project Recap

Greetings, fellow garbage collector enjoyers!

This document contains a recap about this project, silly bugs, and their solutions.

**WARNING:** The style of this rant is **absolutely 0% formal or academic**. If you’re a teacher… well… I hope I can put a smile on your face. I promise this won’t be the “end result” of a formal report.

---

After all that yapping, let's get right onto it.

## Goals

First of all, my main goal is to **write a garbage collector**.
There are many kinds of GCs out there. Mine is:

- **Mark and Sweep**
- **Stop-the-World**
- **Conservative**

---

## TODO

- Write allocator recap

---

## Implementing the GC

Finally, it was time to implement the actual GC.

- **Sweep phase:** which was very easy
- **Mark phase:** a huge mess

There are 3 major locations where pointers can point to heap-allocated objects:

1. **The stack**
2. **The heap itself**
3. **Registers**

---

### Stack Scanning

I first tried to retrieve the **stack borders**.

From now on:

- `stack bottom` = highest possible address inside the stack
- `stack top` = current stack pointer location

#### Finding the stack top

There are two ways:

1. **Direct assembly instruction (not portable):**
```c
asm("mov %%rsp, %0" : "=r"(sp_before_main));
```

2. **An assumption-based method:**
   If I initialize a variable and retrieve its address, it points to the **last created variable**.
   Well duh, there is no way if i create a variable, a variable gets created but it's actually the key
   This works because it will be created **inside the last possible stack frame**, approximating the end of the stack.

---

#### Finding the stack bottom

I tried the same method:
```c
int main() {
    char test = 0;
    void* stack_bottom = &test;

    int a;
}
```

- But for some reason `a`’s address was **bigger than `test`’s**
- Theory: When you allocate on the stack, each allocation is placed at the **bottom of the current stack frame**, right?
- Reality: Nope. The compiler can shuffle variable order inside the stack frame. Disabling optimizations would be a pain.

---

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

- Since this function runs **before `main`**, we can capture the **highest possible reachable address** before the compiler can even try to screw everything up.

