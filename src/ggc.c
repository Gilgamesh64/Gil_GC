#include "ggc.h"
#include <unistd.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#define MIN(a,b) ((a) < (b) ? (a) : (b))

typedef struct block {
    size_t size;
   	bool free;
    bool marked;
    struct block *next;
    struct block *prev;
} block_t;

static block_t* heap_head = NULL;
static block_t* heap_tail = NULL;
static void* heap_end = NULL;

bool gc_debug = false;
void gc_activate_gc_debug(){
    gc_debug = true;
}
bool allocator_debug = false;
void gc_activate_allocator_debug(){
    allocator_debug = true;
}

///Retrieves the stack bottom by running this function before main in order to take the max possible stack address
static void* stack_bottom;
__attribute__((constructor)) void before_main() {
    stack_bottom = __builtin_frame_address(0);
    //printf("Stack bottom: %p\n", stack_bottom);
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
static inline size_t align8(const size_t s) {
    return (s + 7) & ~((size_t)7);
}

/** Prints a block, yes, that's it
  * @param block to print
*/
void print_block(const block_t* block){
    printf("####################\nBlock Address: %p Data Address: %p \t Data Size: %zd \t Total Size: %zd \nFree: %s \t Marked: %s \t Next: %p\n####################\n\n", 
            block, 
            block + 1, 
            block->size, 
            block->size + sizeof (block_t), 
            block->free? "True" : "False", 
            block->marked ? "True" : "False", 
            block->next);
}

///Prints the entire heap
void gc_print_heap(){
    printf("Head: %p,\t End: %p\n\n", heap_head, heap_end);
    printf("------HEAP START------\n");
    block_t* current = heap_head;
    while(current){
        print_block(current);
        current = current->next;
    }
    if(current == heap_head) printf("Empty heap\n");
    printf("------HEAP END------\n\n");
}

/** Requests a chunk of memory from the os
  * @param size of the chunk
  * @return a void* to the first address
*/
static void* request_from_os(const size_t size) {
    void* p = sbrk(0);              //asks for 0 memory to store the first address
    if (sbrk(size) == (void* ) -1)  //allocate the chunk of memory and verify its validity
        return NULL;                //Don't question '(void* ) -1' it's sbrk being wacky
    return p;
}

/** Allocate a new block by requesting memory from the OS.
  * @see void* request_from_os(size_t size)   
  * the returned pointer is inserted at the end of the doubly-linked list.
  * @param size to add to the heap
  * @return  pointer to the first element of the newly allocated block 
*/
static block_t* extend_heap(const size_t size) {
    block_t* block = request_from_os(sizeof(block_t) + size);   //header + actual size
    if (!block) return NULL;

    block->size = size;
    block->free = false;
    block -> marked = false;
    block->next = NULL;
    block->prev = NULL;

    heap_end = (char *)block + sizeof(block_t) + size;

    if (!heap_head) {
        heap_head = block;
        heap_tail = block;
    } else {
        heap_tail->next = block;
        block->prev = heap_tail;
        heap_tail = block;
    }

    return block;
}

/** Scans the heap searching for the FIRST free block
  * first fit policy
  * @param size to allocate
  * @return a pointer to allocated block or NULL if there is no free block big enough
*/
static block_t* find_free_block(const size_t size) {
    block_t* current = heap_head;

    while (current) {
        if (current->free && current->size >= size)
            return current;
        current = current->next;
    }

    return NULL;
}

/** Utility to exclude all pointers that surely are not heap allocated
  * It's not precise to further scan is required:
  * @see is_allocated()
  * @param pointer to scan
  * @return true if pointer is out of heap bounds
*/
static inline bool is_out_of_heap(const void* p){
    return p < (void*) heap_head || p > heap_end;
}

/** Splits block if it is 8B bigger then required
  * @param block maybe too big
  * @param size you wish the block to be near to
*/
static void try_split_block(block_t* block, size_t size){
    if (block->size >= size + sizeof(block_t) + 8) {
        /* block + 1 returns the data part of the block (without header)
           pointer arithmetic is based on the size of the type, adding 1 to block moves the pointer by sizeof(block_t), leaving out the header
           casting to char* unlocks 1B pointer arithmetic instead of sizeof(block_t) based one
           adding size we actually allocate the block
           we then have to cast it back to block_t*
        */
        block_t* new_block = (block_t *)((char *)(block + 1) + size); 
        new_block->size = block->size - size - sizeof(block_t);
        new_block->free = true;
        new_block -> marked = false;
        new_block->next = block->next;
        new_block->prev = block;
        if (new_block->next)
            new_block->next->prev = new_block;
        block->next = new_block;
        block->size = size;
    }
}

/** Checks if the block is allocated by gc_malloc()
  * Should usually be called after previous boundary check
  * @see is_out_of_heap()
  * @param block to check
  * @return true if the block is allocated, false otherwise
*/
static bool is_allocated(const block_t* block){
    block_t* current = heap_head;
    while (current) {
        if(!current -> free && current == block) return true;
        current = current->next;
    }
    return false;
}

/** Retrieves a block containing a pointer, it's similar to:
  * @see is_allocated but allows pointer arithmetic
  * @param ptr to scan
  * @return block if found one, NULL if not
*/
static block_t* find_block_containing(const void* ptr) {
    block_t* current = heap_head;

    while (current) {
        void* start = (void* )(current + 1);
        void* end   = (void* )((char* )(start) + current->size);

        if (ptr >= start && ptr < end) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

/** Allocates a chunk of memory on the heap 
  * Shuld check if the return value is not NULL
  * @param size to allocate
  * @return pointer to the allocated chunk or NULL if failed to allocate
*/
void* gc_malloc(size_t size) {
    if (size == 0)
        return NULL;

    size = align8(size);
    block_t* block = find_free_block(size);

    if (!block) { 
        if(allocator_debug) printf("------EXTENDING HEAP------\n");
        block = extend_heap(size); //if find_free_block failed you can try to allocate the block extending the heap
        if (!block)                //if extend_heap failed it means the os could not allocate memory
            return NULL;           //malloc should return NULL
    } else {                       //use existing block
        block->free = false;
        try_split_block(block, size);
    }
    if(allocator_debug){
        printf("------NEW ALLOCATED BLOCK------\n");
        print_block(block);
    } 

    return (void* )(block + 1);     //returns the allocated block without the header
}

/** Allocates a chunk of memory on the heap 
  * Zeros all its contents and checks for overflow 
  * Shuld check if the return value is not NULL
  * @param element_number number of elements
  * @param element_size size of each element
  * @return pointer to the allocated chunk or NULL if failed to allocate
*/
void* gc_calloc(size_t element_number, size_t element_size){
    if(element_number == 0 || element_size == 0)
        return NULL;

    if (element_size != 0 && element_number > SIZE_MAX / element_size) {
        return NULL; // overflow
    }

    size_t total = element_number * element_size;
    void* ptr = gc_malloc(total);
    if (!ptr) return NULL;

    memset(ptr, 0, total);
    return ptr;
}

/** Reallocates a chunk of memory
  * First tries to shrink block
  * Then tries merging with neighbours, if merging fails calls malloc and frees previous block
  * @param ptr to reallocate, if NULL falls back to gc_malloc
  * @param new_size for the block, if NULL fall back to gc_free
  * @return new location of the pointer
*/
void* gc_realloc(void* ptr, size_t new_size){
    if (ptr == NULL)
        return gc_malloc(new_size);

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    if(is_out_of_heap(ptr))
        return NULL;

    block_t* block = (block_t*)ptr - 1;

    if(!is_allocated(block) || block -> free)
        return NULL;
    
    if(block -> size >= new_size){
        try_split_block(block, new_size);
        return ptr;
    }
    
    block_t* left = block -> prev;
    block_t* right = block -> next;

    //merge both sides
    if (left && right &&
        left->free && right->free &&
        (left->size + block->size + right->size + 2*sizeof(block_t)) >= new_size) {
        left -> free = false;
        left->size += block->size + right->size + 2*sizeof(block_t);
        left->next = right->next;
        if (left->next)
            left->next->prev = left;

        try_split_block(block, new_size);
        memmove((void*)(left + 1), ptr, block->size);

        return (void*)(left + 1);
    }
    //merge right
    if (right && right->free &&
        (block->size + right->size + sizeof(block_t)) >= new_size) {

        block->size += right->size + sizeof(block_t);
        block->next = right->next;
        if (block->next)
            block->next->prev = block;
        try_split_block(block, new_size);

        return ptr;
    }

    //merge left
    if (left && left->free &&
        (left->size + block->size + sizeof(block_t)) >= new_size) {
        left -> free = false;
        left->size += block->size + sizeof(block_t);
        left->next = block->next;
        if (left->next)
            left->next->prev = left;
        try_split_block(block, new_size);

        memmove((void*)(left + 1), ptr, block->size);
        return (void*)(left + 1);
    }

    void* new_location = gc_malloc(new_size);
    if (!new_location)
        return NULL;
    memcpy(new_location, ptr, MIN(block -> size, new_size));
    gc_free(ptr);
    return new_location;
}

/** Attempt to merge adjacent free blocks to reduce fragmentation
  * @param free block to merge  
*/
static void coalesce(block_t* block) {
    while(block->next && block->next->free) {
        block_t* n = block->next;
        block->size += sizeof(block_t) + n->size;
        block->next = n->next;
        if (n->next)
            n->next->prev = block;
    }
    while(block->prev && block->prev->free) {
        block_t* p = block->prev;
        p->size += sizeof(block_t) + block->size;
        p->next = block->next;
        if (block->next)
            block->next->prev = p;
    }
}

/** Free called internally if you are sure the block is valid
  * @param block to free
*/
static void gc_free_no_sanitize(block_t* block){
    block->free = true;
    coalesce(block);
}

/** Returns memory back to the allocator
  * It does NOT eliminate the contents of the memory chunk until it is over written
  * using a freed pointer will work unless the allocator writes something over the memory chunk
  * it is then UNDEFINED BEHAVIOUR and should not be done
  * @param ptr to free, if not heap allocated by gc_malloc() function will return before seg faults
  * @return true if freed correctly, otherwise false 
  */
bool gc_free(const void* ptr) {
    if (!ptr)
        return false;

    block_t* block = (block_t*)ptr - 1;     //retrieve block header

    if(!is_allocated(block)){               //TODO: make more polite
        if(allocator_debug) printf("EROOOR TRIED TO FREE RANDOM ASS POINTER! \nPointer %p was not allocated via gc_malloc() \nYOU STOOPID\n\n", ptr);
        return false;
    }
    gc_free_no_sanitize(block);

    return true;
}

static void try_mark(const uintptr_t* ptr);

/** Scans the payload of a block searching for pointers to other blocks
  * @param block to scan
*/
static void mark_contents(const block_t* block){
    uintptr_t* start = (uintptr_t*)(block + 1);
    uintptr_t* end = (uintptr_t*)((char*)(block + 1) + block->size);
    for (; start < end; start++) {
        try_mark(start);
    }
}

/** Perform sanity checks and tries to determine if a pointer to a block is valid
  * In that case, retrieves the block and marks it
  * @param ptr to try mark
*/
static void try_mark(const uintptr_t* ptr){
    if(is_out_of_heap((void*) *ptr)){
        return;
    }

    block_t* candidate = find_block_containing((void*) *ptr);

    if(!candidate){
        return;
    }

    if(!candidate -> marked){
        candidate -> marked = true;
        if(gc_debug){
            printf("MARKING:\n");
            print_block(candidate);
        }
        mark_contents(candidate);
    }
}

///Performs the full mark phase scanning stack, heap, registers and .data segment
static void gc_mark(){
    if(gc_debug) printf("------MARK PHASE STARTED------\n");
    //stack scanning
    void* stack_top = __builtin_frame_address(0);

    uintptr_t* start = (uintptr_t*)stack_top;
    uintptr_t* end   = (uintptr_t*)stack_bottom;

    if (start > end) {
        uintptr_t* tmp = start;
        start = end;
        end = tmp;
    }

    for (uintptr_t* p = start; p < end; p++) {
        try_mark(p);
    }

    //register scanning

    jmp_buf env;
    setjmp(env); //forces the os to write all caller register data into env
    uintptr_t* reg = (uintptr_t*)env;
    uintptr_t* reg_end = (uintptr_t*)((char*)env + sizeof(env));

    for (; reg < reg_end; reg++) {
        try_mark(reg);
    }
    if(gc_debug) printf("------MARK PHASE ENDED------\n\n");

}

///Perform the sweep phase, clearing non marked blocks
static void gc_sweep(){
    if(gc_debug) printf("------SWEEP PHASE STARTED------\n");
    block_t* current = heap_head;
    while(current){
        if(!current -> free && !current -> marked){
            if(gc_debug){
                printf("SWEEPING:\n");
                print_block(current);
            }
            gc_free_no_sanitize(current);
        }
        current -> marked = false;
        current = current -> next;
    }
    if(gc_debug) printf("------SWEEP PHASE ENDED------\n\n");
}

///Performs a full cycle, if gc_debug flag is active performs performance checks
void gc_cycle(){
    if(heap_head == NULL) return;
    
    if(!gc_debug){
        gc_mark();
        gc_sweep();
    }
    else{
        struct timespec start, mark, end;

        clock_gettime(CLOCK_MONOTONIC, &start);

        if(gc_debug) printf("--------GC CYCLE STARTED--------\n");
        
        gc_mark();

        clock_gettime(CLOCK_MONOTONIC, &mark);

        gc_sweep();

        clock_gettime(CLOCK_MONOTONIC, &end);
        double tot = 
            (end.tv_sec - start.tv_sec) +
            (end.tv_nsec - start.tv_nsec) / 1e9;
        double mark_time = 
            (mark.tv_sec - start.tv_sec) +
            (mark.tv_nsec - start.tv_nsec) / 1e9;
        double sweep_time = 
            (end.tv_sec - mark.tv_sec) +
            ((end.tv_nsec - mark.tv_nsec) / 1e9);

        printf("--------GC CYCLE ENDED--------\n");

        printf("\n--------Performance: --------\nTotal: %f\tMark: %f\t Sweep: %f\n\n", tot, mark_time, sweep_time);
    }
}