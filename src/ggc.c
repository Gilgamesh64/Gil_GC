#include "ggc.h"
#include <unistd.h>
#include <stdio.h>
#include <setjmp.h>

block_t *heap_head = NULL;

void* stack_bottom;
__attribute__((constructor)) void before_main() {
    char a;
    stack_bottom = &a;
    printf("Stack bottom: %p\n", stack_bottom);
}

/** Requests a chunk of memory from the os
  * @param size of the chunk
  * @return a void* to the first address
*/
static void *request_from_os(size_t size) {
    void *p = sbrk(0);              //asks for 0 memory to store the first address
    if (sbrk(size) == (void *) -1)  //allocate the chunk of memory and verify its validity
        return NULL;                //Don't question '(void *) -1' it's sbrk being wacky
    return p;
}

/** Utility to round up the requested size to a multiple of 8 bytes (or
  * the alignment requirements of block_t).  This helps keep the data portion
  * of each allocation properly aligned.
  * '(s + 7)' checks if the number goes over the current multiple of 8
  * '~((size_t)7)' is the bit sequence: 11111000
  * a bitwise & between any number and 11111000 erases everything under '8' making the result a multiple of 8
  * for example s = 5, 5+7 = 12 = 00001100 & 11111000 = 00001000 = 8  
  * @param s number to round up to 8
  * @return the number rounded up to 8 
*/
static inline size_t align8(size_t s) {
    return (s + 7) & ~((size_t)7);
}

/** Scans the heap searching for the FIRST free block
  * first fit policy
  * @param size to allocate
  * @return a pointer to allocated block or NULL if there is no free block big enough
  *
*/
static block_t *find_free_block(size_t size) {
    block_t *current = heap_head;

    while (current) {
        if (current->free && current->size >= size)
            return current;
        current = current->next;
    }

    return NULL;
}

void print_block(block_t* block){
    printf("Address: %p \t Data Size: %zd \t Total Size: %zd \tFree: %s \t Marked: %s \t Next: %p\n", 
            block, block->size, block->size + sizeof (block_t), block->free? "True" : "False", block->marked ? "True" : "False", block->next);
}

void print_heap(){
    block_t* current = heap_head;
    while(current){
        print_block(current);
        current = current->next;
    }
    if(current == heap_head) printf("Empty heap\n");
    printf("\n");
}

bool is_allocated(block_t* block){
    block_t *current = heap_head;
    while (current) {
        if(!current -> free && current == block) return true;
        current = current->next;
    }
    return false;
}

block_t* from_ptr(void* ptr){
    return (block_t*)ptr-1;
}

/** Allocate a new block by requesting memory from the OS.
  * @see void *request_from_os(size_t size)   
  * the returned pointer is inserted at the end of the doubly-linked list.
  * @param size to add to the heap
  * @return  pointer to the first element of the newly allocated block 
  */
static block_t *extend_heap(size_t size) {
    block_t *block = request_from_os(sizeof(block_t) + size);   //header + actual size
    if (!block) return NULL;

    block->size = size;
    block->free = false;
    block->next = NULL;
    block->prev = NULL;

    if (!heap_head) {
        heap_head = block;
    } else {
        block_t *last = heap_head;
        while (last->next)
            last = last->next;

        last->next = block;
        block->prev = last;
    }

    return block;
}

/** Allocates a chunk of memory on the heap 
  * Caller must free() for now :)
  * Shuld check if the return value is not NULL
  * @param size to allocate
  * @return pointer to the allocated chunk or NULL if failed to extend heap
*/
void *gc_malloc(size_t size) {
    if (size <= 0)
        return NULL;

    size = align8(size);
    block_t *block = find_free_block(size);

    if (!block) { 
        block = extend_heap(size); //if find_free_block failed you can try to allocate the block extending the heap
        if (!block)                //if extend_heap failed it means the os could not allocate memory
            return NULL;           //malloc should return NULL
    } else {                       //use existing block
        block->free = false;
        //splits block if it is 8B bigger then required
        if (block->size >= size + sizeof(block_t) + 8) {
        	/* block + 1 returns the data part of the block (without header)
        	   pointer arithmetic is based on the size of the type, adding 1 to block moves the pointer by sizeof(block_t), leaving out the header
        	   casting to char* unlocks 1B pointer arithmetic instead of sizeof(block_t) based one
        	   adding size we actually allocate the block
        	   we then have to cast it back to block_t*
        	*/
            block_t *newblk = (block_t *)((char *)(block + 1) + size); 
            newblk->size = block->size - size - sizeof(block_t);
            newblk->free = true;
            newblk->next = block->next;
            newblk->prev = block;
            if (newblk->next)
                newblk->next->prev = newblk;
            block->next = newblk;
            block->size = size;
        }
    }

    return (void *)(block + 1);     //returns the allocated block without the header
}

/**
  * Attempt to merge adjacent free blocks to reduce fragmentation
  * @param free block to merge  
  */
static void coalesce(block_t *block) {
    if (block->next && block->next->free) {
        block_t *n = block->next;
        block->size += sizeof(block_t) + n->size;
        block->next = n->next;
        if (n->next)
            n->next->prev = block;
    }
    if (block->prev && block->prev->free) {
        block_t *p = block->prev;
        p->size += sizeof(block_t) + block->size;
        p->next = block->next;
        if (block->next)
            block->next->prev = p;
    }
}

/** Returns memory back to the allocator
  * It does NOT eliminate the contents of the memory chunk until it is over written
  * using a freed pointer will work unless the allocator writes something over the memory chunk
  * it is then UNDEFINED BEHAVIOUR and should not be done
  * @param ptr to free, if not heap allocated by gc_malloc() function will return before seg faults
  * @return true if freed correctly, otherwise false 
  */
bool gc_free(void *ptr) {
    if (!ptr)
        return false;

    block_t *block = (block_t*)ptr - 1;     //retrieve block header

    if(!is_allocated(block)){               //TODO: make more polite
        printf("EROOOR TRIED TO FREE RANDOM ASS POINTER! \nPointer %p was not allocated via gc_malloc() \nYOU STOOPID\n\n", ptr);
        return false;
    }
    block->free = true;
    coalesce(block);
    return true;
}

static void try_mark(char* sp){
    block_t* candidate = from_ptr(*((int**)sp));
    if(is_allocated(candidate)){
        printf("Marking %p\n", sp);
        candidate -> marked = true;
    }
}

void gc_mark(){
    char a;
    void* stack_top = &a;

    for(char* sp = (char*)stack_bottom; sp >= (char*)stack_top; sp--){
        try_mark(sp);
    }

    jmp_buf env;
    setjmp(env); //forces the os to write all caller register data into env
    for(char* curr_reg = (char*) env; curr_reg <= (char*) env + sizeof(env); curr_reg++){
        try_mark(curr_reg);
    }
}

void gc_sweep(){
    block_t* current = heap_head;
    while(current){
        if(!current -> free && !current -> marked){
            printf("SWEEPING:\n");
            print_block(current);
            gc_free(current + 1);
        }
        current = current -> next;
    }
    printf("\n");
}

void gc_cycle(){
    gc_mark();
    gc_sweep();
}