# Checkpoint 1 Work

 * For this dynamic memory allocator, we use an implicit free list. First, we initialize the heap with 4096 bytes that inculdes
 * a header of 8 bytes, padding of 8 bytes, payload of 4080 bytes, a footer of 8 bytes and lastly padding of 8 bytes. this insures
 * that the heap is 16-bytes aligned.
 * 
 * this is what our initial heap will look like:

 * The header and footer of the heap are used to store the size of the block and the allocated bit.
 * The allocated bit is used to determine if the block is allocated or not. If the allocated bit is 0, then the block is free. If the allocated
 * bit is 1, then the block is allocated. The payload is the actual memory that is allocated. The padding is used to ensure that the payload is 16-bytes
 * alligned and its always initialized to 0. When we want to get the size of the block, we mask out the last bit of the header and footer.
 * When we want to get the allocated bit, we mask out all the bits except the last bit of the header and footer. To allocate memory. we use the first fit to
 * find the first free block that fits the size. If a suitable block is found, we allocate and possibly split the block. If no suitable block is found, we extend the heap
 * to elimenate any fragmentation, we coalesce when freeing memory. we mark the block as free and then check if the previous block or the next block or both previous blocks are free.
 * if so, we coelesce.

 *  Work done:
 * Pauka Sy: Coalesce, realloc, expand_heap, first_fit, place
 * Fatma Alwasmi: struc blk, mm_init, malloc, free, max, lsb_a, get_size, get_allocated, next_blk, prev_blk, write_value, split,
 * both: mm_checkheap, debuging.

# Final Submission Work
we implemented an explicit free list in our memory management system. Instead of scanning the entire heap, we maintain a list of only the free blocks, thereby reducing the search time significantly.

Structure of a Free Block:

Each free block in our system now has a new structure: it contains pointers within its payload. These pointers, often referred to as "next" and "previous" pointers, essentially allow the free blocks to be linked together in a doubly-linked list.


Previous Pointer: Points to the previous free block in the list.
Next Pointer: Points to the next free block in the list.
This means that starting from any point in the list, we can easily traverse to either the next free block or the previous one, without having to scan through used blocks in the heap.

Benefits:

Efficiency: The main advantage of using an explicit free list is the improvement in search efficiency. By only having to traverse through free blocks, we avoid scanning potentially large chunks of allocated memory, making the allocation process faster.

Better Utilization: With the explicit list, splitting a large free block to fit a requested size becomes more straightforward. Once split, the remaining free portion can be easily added back to the list.

 *  Work done:
 * Pauka Sy: Coalesce, realloc, expand_heap, first_fit, place
 * Fatma Alwasmi: struc blk, mm_init, malloc, free, max, lsb_a, get_size, get_allocated, next_blk, prev_blk, write_value, split,
 * both: mm_checkheap, debuging, remove_node, add_node
