/*
 * mm.c
 *
 * Name: Fatma Alwasmi, Pauka Sy
 *
 * For this dynamic memory allocator, we use an explicit free list. First, we initialize the heap with 4096 bytes that inculdes
 * a header of 8 bytes, prlouge of 8 bytes, payload of 4064 bytes, a footer of 8 bytes and lastly epilouge of 8 bytes. this insures
 * that the heap is 16-bytes aligned.
 * 
 * this is what our initial heap will look like:
 * 
 *    padding              header                                   payload                                                    footer            padding
 * +----------------+----------------+-----------------------------------------------------------------------------------+----------------+----------------+
 * |  0/0           |     4064/0     |                                                                                   |   4064/0       |    0/0         |
 * |                |                |                                                                                   |                |                |
 * |                |                |                                                                                   |                |                |
 * |                |                |                                                                                   |                |                |
 * +----------------+----------------+-----------------------------------------------------------------------------------+----------------+----------------+
 * 
 * The header and footer of the heap are used to store the size of the block and the allocated bit.
 * The allocated bit is used to determine if the block is allocated or not. If the allocated bit is 0, then the block is free. If the allocated
 * bit is 1, then the block is allocated. The payload is the actual memory that is allocated. The padding is used to ensure that the payload is 16-bytes
 * alligned and its always initialized to 0. When we want to get the size of the block, we mask out the last bit of the header and footer.
 * When we want to get the allocated bit, we mask out all the bits except the last bit of the header and footer. To allocate memory. we use the first fit to
 * find the first free block that fits the size. If a suitable block is found, we allocate and possibly split the block. If no suitable block is found, we extend the heap
 * to elimenate any fragmentation, we coalesce when freeing memory. we mark the block as free and then check if the previous block or the next block or both previous blocks are free.
 * if so, we coelesce.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
//#define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

static const size_t CHUNKSIZE = (1 << 12);

static bool aligned(const void* p);

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}
/*create block using struct that includes the size and payload. 1 words */
typedef struct blk{ 
    size_t size;
    //size_t padding;
    
} blk;

typedef struct node{
    struct node* next;
    struct node* prev;
}node;

node *first = NULL; //pointer to first node in the list

static size_t lsb_a(size_t size, size_t a);
static size_t get_size(blk *bptr);
static void write_value(blk *bptr, size_t val);
static bool get_allocated(blk* bptr);
static blk *next_blk(blk* bptr);
static blk *prev_blk(blk* bptr); 
static void split(blk *ptr, size_t size);
static void place(blk* bptr, size_t size);
static void coalesce(blk *blkptr);
static blk* expand_heap(size_t bytes);
static blk* first_fit(size_t size);
static void remove_node(blk* bptr);
static void add_node(blk* bptr);

/*
Initialize: returns false on error, true on success. 
*/
bool mm_init(void)
{   
    //create empty heap of 4096 bytes
    char* start_heap = (char*)mem_sbrk(CHUNKSIZE);

    if(start_heap == (char*)-1){
        return false;
    }  
    
    write_value((blk*)start_heap, lsb_a(0,1)); //padding word as a dummy predecessor footer
    write_value((blk*)(((char*)start_heap)+8), lsb_a(CHUNKSIZE-4*sizeof(blk),0));//header size = 4096-4*8 = 4064, allocated bit = 0
    write_value((blk*)(((char*)start_heap)+4080), lsb_a(CHUNKSIZE-4*sizeof(blk),0)); //footer size = 4096-4*8 = 4064, allocated bit = 0
    write_value((blk*)(((char*)start_heap)+4088), lsb_a(0,1)); //padding word as a dummy successor header (epilouge)
    
    first = (node*)(((char*)start_heap)+16); //pointer to header of first node
    
    // Set up the node struct in the payload of the block
    //node* block_node = first; // This will point to the memory right after the header
    first->next = NULL; // Initialize to NULL.
    first->prev = NULL; // Initialize to NULL. 
    
    return true;
}

/*
 * malloc: 
 * Allocates memory on the heap of the specified size. The function searches the heap
 * for a suitable free block using a first-fit strategy. If no block is found that 
 * accommodates the request, the heap is extended. The allocated block is aligned
 * to the required alignment and is ensured to be large enough to hold the user's data
 * and the block's header/footer.
 */
void* malloc(size_t size)
{

    size_t extendsize =0; //amount to extend heap if no fit
    blk *bptr; //pointer to block
    
    //handle invalid request 
    if(size == 0){
        return NULL;
    }
    //adjust size
    size = align(size); //ensures the allocated memory block has the correct alignment
    
    // search for free block
    bptr = first_fit(size); //finds first free block that fits the size (ALGINED)

    //if suitable block found, allocate and possibly split
    if(bptr != NULL){

        place(bptr, size); //places the block. calling remove node in place function instead

        return (void*)(bptr + 1); // Return pointer to the payload
    }


    //if no block found, get more memory and place the block
    if(size + 2*sizeof(blk) > 4096){
        extendsize =  size + 2*sizeof(blk);
    }
    else{
        extendsize = 4096;
    }

    bptr = expand_heap(extendsize); //helper function to expand heap 

    if(bptr == NULL){
        return NULL;
    }

    place(bptr, size); //place the block. calling remove_node in place function instead


    return (void*)(bptr + 1); // Return pointer to the payload
}

/*
 * free:
 * De-allocates or frees a block of memory, making it available for future allocations.
 * After marking a block as free, it checks neighboring blocks to coalesce any adjacent
 * free blocks, reducing fragmentation.
 */
void free(void* ptr)
{   
    //check if ptr is NULL
    if(ptr == NULL){
        return;
    }

    ptr = ((char*)ptr) - sizeof(blk); //get pointer to header

    //check if blk is already allocated
    if (!get_allocated((blk*)ptr)) {
        return;
    }

    write_value((blk*)ptr, lsb_a(get_size(ptr), 0)); //mark block as free with size of payload
    write_value((blk*)(((char*)ptr) + get_size(ptr) + sizeof(blk)), lsb_a(get_size(ptr), 0)); //mark block as free with size of payload

    coalesce((blk*)ptr);

    return;
}

  
/*
 * realloc:
 * Resizes the memory block pointed to by oldptr to 'size' bytes. The contents of the 
 * memory block will remain unchanged up to the minimum of the old and new sizes. 
 * Any unused space due to increased size will be uninitialized. If oldptr is NULL, 
 * it behaves like malloc for the specified size.
 */
void* realloc(void* oldptr, size_t size)
{
    blk* oldptr_hdr = (blk*)(((char*)(oldptr)) - sizeof(blk));
    size_t oldptr_size = get_size(oldptr_hdr);
   
    if(oldptr == NULL){
        return malloc(size);
    }
    if(size == 0){
        free(oldptr);
        return NULL;
    }

    size_t needed_size = align(size) + 2*sizeof(blk);
    //allign size + hdr and ftr

    if(oldptr_size >= needed_size){
        //place
        //place(oldptr_hdr, oldptr_size);

        return oldptr;
    }

    void* mallocPtr = malloc(size); //allocate new block of memory
    mem_memcpy(mallocPtr, oldptr, oldptr_size); //copy old block to new block. 
    free(oldptr); //free old block

    return mallocPtr;
}


/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap:
 * This function serves as a heap consistency checker for debugging purposes. 
 * By iterating over the heap, it ensures that the block's header and footer 
 * match and checks other invariants. When debugging is enabled, this function 
 * provides detailed information about the state of the heap at the line number 
 * provided by 'lineno'. 
 *
 * lineno: The line number in the source code from where mm_checkheap was called.
 */
bool mm_checkheap(int lineno)
{

#ifdef DEBUG
/* Write code to check heap invariants here */
    blk *bptr = (blk*)mem_heap_lo();
    while(bptr < (blk*)mem_heap_hi()){
        dbg_printf("\n\n size in header %ld, size in footer %ld\n", get_size(bptr), get_size((blk*)(((char*)bptr) + get_size(bptr) + sizeof(blk))));
        dbg_printf("alloc bit in header %d, alloc bit in footer %d\n", get_allocated(bptr), get_allocated((blk*)(((char*)bptr) + get_size(bptr) + sizeof(blk))));
        dbg_printf("MM_CHEACKHEAP header: %p, size: %ld, allocated: %d\n\n", bptr, get_size(bptr), get_allocated(bptr));
        bptr = next_blk(bptr);        
    }
    dbg_printf("\n");
#endif /* DEBUG */

    return true;
}

//-------------------------------------------------------helper functions-------------------------------------------------------------------------// 

/*given 'a' (allocated bit), we bitwise OR 'a' with given size so the last bit
represents if memory is allocated or not.*/
static size_t lsb_a(size_t size, size_t a) {
    return size | a;
}

/*given a pointer to blk, we masks out last bit to get actual allocated size
by ANDing the header with 11111110 */
 static size_t get_size(blk *bptr){
    return bptr->size & ~0x1;
 }

/*given a pointer to blk and size of allocated memory,
we write the value into header*/
static void write_value(blk *bptr, size_t val) {
    bptr->size = val;
    //bptr->padding = 0;

}


/*gets the bit that represents allocated memory by ANDing header with 0000001
if funtion returns 0x0, then its free, if it
returns 0x1, then its allocated.*/
static bool get_allocated(blk* bptr){
    if(bptr == NULL){
        return true; //if bptr is NULL, then it is allocated for coallesce function
    }
    return bptr->size & 0x1;
}

/*given a pointer to struct blk, we convert the pointer to a byte pointer (char*)
and moves the pointer bptr to the next block*/
static blk *next_blk(blk* bptr) {
    return (blk *)(((char *)bptr) + get_size(bptr) + 2*sizeof(blk));
}


/*given a pointer to struct blk, we convert the pointer to a byte pointer (char*)
//and moves the pointer bptr to the previous block.*/
static blk *prev_blk(blk* bptr) {
    blk* prevFooter =(blk*)(((char*)(bptr)) - sizeof(blk));
    return (blk *)(((char *)(bptr)) - get_size(prevFooter) - 2*sizeof(blk));
    
}

/*
 * split:
 * The split function is used when the size of a free block exceeds the requested 
 * size for allocation. In such cases, to avoid wastage of space, the block is divided 
 * into two distinct blocks. The first block is allocated with the requested size, while 
 * the second block retains the remaining space and remains free. This division is 
 * accomplished by updating the headers and footers of the blocks accordingly. By doing so, 
 * the allocator ensures efficient memory utilization and prepares the newly created free 
 * block for potential future allocations.
 *
 * ptr: A pointer to the free block that needs to be split.
 * size: The size of the block to be allocated post-split.
 */
static void split(blk *ptr, size_t size){
    size_t oldSize = get_size(ptr); //get size of old block 
    size_t rest_size = oldSize - size - (2*sizeof(blk)); //get size of rest of block 

    write_value(ptr, lsb_a(size,1)); //header of new block w/ size and allocated bit =1
    write_value((blk*)(((char*)ptr) + size + sizeof(blk)), lsb_a(size,1));//footer of new block w/ size and allocated bit =1
    remove_node(ptr); 
    blk* nextBlk = (blk*)(((char*)ptr) + size + 2*sizeof(blk)); //get pointer to next block

    write_value(nextBlk, lsb_a(rest_size,0)); //header of rest of block w/ rest_size and allocated bit =0
    write_value((blk*)(((char*)nextBlk) + rest_size + sizeof(blk)), lsb_a(rest_size,0)); //footer of rest of block w/ rest_size and allocated bit =0

    if(!get_allocated((blk*)(((char*)nextBlk) + rest_size + 2*sizeof(blk)))){ //if next block is not allocated
    
        coalesce(nextBlk);
    }
    else{
        add_node(nextBlk);    
    }
        
}

/*
 * place:
 * This function is tasked with placing a block of a specified size in a free space within 
 * the heap. Once a suitable free block is identified (typically using a strategy like first-fit),
 * this function is invoked to allocate the desired size. If the remaining space in the free block 
 * after allocation is sufficiently large (greater than or equal to the minimum block size), the 
 * block is split into two: one allocated and one free. This ensures that no memory is wasted. 
 * If the remaining space is smaller, the entire block is allocated. The function updates block 
 * headers and footers to reflect these changes, ensuring the heap remains consistent.
 *
 * bptr: A pointer to the free block where the allocation should take place.
 * size: The size of the block to be allocated.
 */
static void place(blk* bptr, size_t size){
    size_t bsize = 0;
    bsize = get_size(bptr); //get size of free blk
    size_t rest = bsize - size; //get size of rest of blk

    if(rest >= 4*sizeof(blk)){ //if rest is greater than or equal to 32 bytes
        split(bptr, size); //split blk
        return;
    }
    else{ 
        write_value(bptr, lsb_a(bsize,1)); //mark blk as allocated
        write_value((blk*)(((char*)bptr) + bsize + sizeof(blk)), lsb_a(bsize,1)); //mark blk as allocated
        remove_node(bptr); //remove node from free list
    }
}

/*
* coalesce:
* The primary role of this function is to address fragmentation by merging adjacent 
* free blocks in the heap. By checking the allocation status of neighboring blocks, 
* this function determines if the current block can be merged with the previous and/or 
* next blocks. Such coalescing ensures efficient utilization of heap space, reduces 
* fragmentation, and prevents the creation of unnecessarily small free blocks that 
* might not be useful for future allocation requests. this function can perform one 
* of four possible coalescing operations.first, we take pointer bptr pointing to 
* the header, and check neighboring blocks to check if free.
* this is done by using fuctions next_blk and prev_blk. 
* case1: if prev_blk and next_blk both allocated, do nothing
* case2: if prev_blk is alloc but next_blk is free, coalesce w/ next_blk
* case3: if prev_blk is free by next_blk is alloc, coalesce w/ prev_blk
* case4: if both prev_blk and next_blk free coalesce w/ all 
*/
static void coalesce(blk *blkptr)
{
    blk* nextBlock;
    blk* prevBlock;


    //if blkptr is at the beginning of the heap 
    if(get_size((blk*)(((char*)blkptr)-sizeof(blk))) == 0){
        prevBlock = NULL;
        nextBlock = next_blk(blkptr);
    }
    //if blkptr is at the end of the heap
    else if(get_size((blk*)(((char*)blkptr)+get_size(blkptr)+sizeof(blk))) == 0){
        nextBlock = NULL;
        prevBlock = prev_blk(blkptr);
    }
    //if blkptr is in the middle of the heap
    else{
        nextBlock = next_blk(blkptr);
        prevBlock = prev_blk(blkptr);
    }

    size_t size = get_size(blkptr); //size of blkptr
    bool prev = get_allocated(prevBlock); //footer of prev_blk
    bool next = get_allocated(nextBlock); //header of next_blk
    
    
    //case1: alloc  alloc
    if(prev && next){
        add_node(blkptr);
    }

    //case2: alloc   free
    else if(prev && !next){
        size_t nextSize = get_size(nextBlock);
        size+=(nextSize + 2*sizeof(blk));

        remove_node(nextBlock);

        write_value(blkptr, lsb_a(size,0));
        write_value((blk*)(((char*)blkptr) + size + sizeof(blk)), lsb_a(size,0));

        add_node(blkptr);
    }

    //case3: free   alloc
    else if(!prev && next){
        size_t prevSize = get_size(prevBlock);
        size+=(prevSize + 2*sizeof(blk));

        remove_node(prevBlock);

        write_value(prevBlock, lsb_a(size,0));
        write_value((blk*)(((char*)prevBlock) + size + sizeof(blk)), lsb_a(size,0));

        add_node(prevBlock);
    }

    //case4: free     free
    else if(!prev && !next){
        size_t prevSize = get_size(prevBlock);
        size_t nextSize = get_size(nextBlock);
        size += (prevSize + nextSize + 4*sizeof(blk));

        remove_node(prevBlock);
        remove_node(nextBlock);

        write_value(prevBlock, lsb_a(size,0));
        write_value((blk*)(((char*)prevBlock) + size + sizeof(blk)), lsb_a(size,0));

        add_node(prevBlock);
    }
}

/*
 * expand_heap:
 * This function is responsible for increasing the size of the heap by a specified 
 * number of bytes. When the allocator cannot find a suitable free block to satisfy 
 * an allocation request, it uses this function to expand the heap. By adding more 
 * space to the heap, the allocator can then use this new space to fulfill the request. 
 * Besides increasing the heap size, this function also sets the header and footer 
 * of the new free block, ensuring it's ready for future allocations or coalescing.
 *
 * bytes: The number of bytes by which the heap should be expanded.
 * return: A pointer to the start of the new block or NULL if the heap expansion failed.
 */
static blk* expand_heap(size_t bytes){
    bytes = align(bytes);

    //new_space is a pointer to the first byte of the newly allocated heap area.
    blk *new_space = (blk*)mem_sbrk(bytes); // Extend heap by bytes initially using mem_sbrk
    

        if ((void*)new_space != (void*)-1) {
            write_value((blk*)(((char*)new_space)+bytes - sizeof(blk)), lsb_a(0,1)); //new epilouge header size = 0, allocated bit = 1

            write_value((blk*)(((char*)new_space)+bytes - 2*sizeof(blk)), lsb_a(bytes-2*sizeof(blk),0)); //footer size = bytes-2*8 = bytes-16, allocated bit = 0

            write_value((blk*)(((char*)new_space) - sizeof(blk)), lsb_a(bytes-2*sizeof(blk),0)); // override old epilouge with header size = bytes-2*8 = bytes-16, allocated bit = 0

            new_space = new_space - 1; //move pointer to header
        }
        else{
            return NULL;
        }
        
        blk* prevBlk = prev_blk(new_space);

        //coalesce if the previous block is free
        if(!get_allocated(prevBlk)){
            coalesce(new_space);
            new_space = prevBlk;
        }
        else{
            add_node(new_space);
        }
        return new_space;
} 

/*
 * first_fit:
 * This function employs a first-fit search strategy to identify a free block 
 * that can accommodate a specified size. It sequentially traverses the heap from 
 * its start to its end, looking for the first free block that is large enough to 
 * fit the requested size. By returning the pointer to this block (or NULL if no 
 * suitable block is found), it enables the memory allocator to use this block for 
 * allocation, ensuring efficient memory utilization and reducing wastage.
 *
 * size: The size of the block to search for.
 * return: A pointer to the first free block that fits the size or NULL if no such block is found.
 */

static blk* first_fit(size_t size){
    //blk *bptr = (blk*)((char*)(mem_heap_lo()+8)); //pointer to first block
     node* start = first; //pointer to first node in the list

     while(start != NULL){
        blk* bptr = (blk*)(((char*)start) - sizeof(blk));
        if(!get_allocated(bptr) && get_size(bptr) >= size){
             bptr = (blk*)(((char*)start) - sizeof(blk));
             return bptr;
         }
         start = start->next;
     }

    return NULL;
}

static void remove_node(blk* bptr){
    node* curr = (node*)(((char*)bptr) + sizeof(blk)); // This will point to the memory right after the header. i.e. node
    node* prev_node = curr->prev;
    node* next_node = curr->next;

    //case1: if curr is in the center
    if(prev_node != NULL && next_node != NULL){
        prev_node->next = curr->next;
        next_node->prev = curr->prev; 
    }

    //case2: if curr is at the begining 
    else if(prev_node == NULL && next_node != NULL){
        next_node->prev = NULL;
        first = curr->next;
    }
    
    //case3: if curr is at the end
    else if(prev_node != NULL && next_node == NULL){
        prev_node->next = NULL;
    }
    //case4: if curr is the only element in the free list
    else if (prev_node == NULL && prev_node == NULL){
        first = NULL;
    }
    
    return;
 }

static void add_node(blk* bptr){
    node* n = (node*)(((char*)bptr) + sizeof(blk)); // This will point to the memory right after the header
    n->next = first; 
    n->prev = NULL;
    
    if(first != NULL){
        first->prev = n;
    }
    first = n;
    return;
}
