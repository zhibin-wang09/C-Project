/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

static void *search_quick_list(size_t size);
static void *search_main_list(int flag, size_t size);
static void *coalesce(sf_block *block);
static void *split_and_reinsert(sf_block *block, size_t size);
static void *grow_wrapper(size_t size); /* uses sf_mem_grow but also takes care of the epilogue and alignments */
static void *init_free_block(sf_block* block,size_t size);
static void *extend_heap(int init);
static void insert_doubly(sf_block *sentinel,sf_block *block);
static void set_prev_alloc(sf_block *block, size_t prev_alloc_bit,int flag);
static void insert_quick_list(sf_block *block);
static void flush_quick_list(sf_block *first);
static size_t check_valid_pp(void *pp);
static sf_block *remove_doubly(sf_block *block);
static int init = 1;

void *sf_malloc(size_t size) {
    if(!size) return NULL;

    /* manipulate size so it meets the requirement */
    size = size + 8; // include header size 8 bytes
    while(size & 7){size++;} /* memalign */
    if(size <= 32){
        size += 32 - size;
    }
    sf_block * list_ptr = NULL;
    if(sf_mem_start() == sf_mem_end()){
        /* initialize the sentinels for the circular lists */
        for(size_t i = 0; i < sizeof(sf_free_list_heads)/sizeof(sf_block); i++){
            /* set the next and prev links to the sentinel itself */
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        list_ptr = grow_wrapper(size);
        return list_ptr == NULL ? NULL : list_ptr;
    }

    /* check if we have enough space in quick list first then main free list */
    if(size < 32 + (NUM_QUICK_LISTS-1) * 8 ){ /* if the request size could exist in the quick list */
        list_ptr = search_quick_list(size);
        uintptr_t payload_addr = ((uintptr_t) list_ptr) + 8;
        if(list_ptr != NULL) return (sf_block *) payload_addr;
    }
    list_ptr = search_main_list(1,size);
    uintptr_t payload_addr = ((uintptr_t) list_ptr) + 8;
    if(list_ptr != NULL) return (sf_block *) payload_addr;
     /* search main free list */

    return grow_wrapper(size);
}

void sf_free(void *pp) {
    // TO BE IMPLEMENTED

    /* check if is valid pointer */
    uintptr_t header_addr =  ((uintptr_t) pp) -8;
    sf_block *block = (sf_block *) header_addr;
    uintptr_t footer_addr = header_addr + (((sf_block *) header_addr) -> header & 0xfffffffffffffff8) - 8;
    size_t block_size = check_valid_pp(pp);


    if(32 <= block_size && block_size <= 184 && !(block_size & 0x7)){
        insert_quick_list(block);
        return;
    } /* block can be inserted into the quicklist */

    /* coalesce then insert into free list if quicklist is not possible*/
    block = coalesce(block);
    block -> header &= 0xfffffffffffffffe;
    footer_addr = ((uintptr_t)block) + (block -> header & 0xfffffffffffffff8) - 8;
    *((sf_header *)footer_addr) = block->header;
    set_prev_alloc(((sf_block *)(footer_addr + 8)), 0xfffffffffffffffd,0); //change the prev alloc bit for the next block
    block_size = block -> header & 0xfffffffffffffff8;
    sf_block *sentinel  = search_main_list(0,block_size);
    insert_doubly(sentinel,block);
}

void *sf_realloc(void *pp, size_t rsize) {
    // TO BE IMPLEMENTED
    abort();
    check_valid_pp(pp);
    if(rsize == 0){
        sf_free(pp);
        return NULL;
    }
}

void *sf_memalign(size_t size, size_t align) {
    // TO BE IMPLEMENTED
    abort();
}

/**
 * This function searches the quick list and look for any available free block.
 * Removes the free block from quick list if found.
 *
 * @return If found, return the address to the block. If not found returns NULL.
 * */
void *search_quick_list(size_t size){
    int cur_list_size = 32;
    struct sf_block *list_ptr = NULL;
    for(int i = 0; i < NUM_QUICK_LISTS; i++){
        if(size > cur_list_size) continue; /* size does not satisfy request */
        if(sf_quick_lists[i].length != 0) {
            list_ptr = sf_quick_lists[i].first;
            sf_quick_lists[i].first = sf_quick_lists[i].first -> body.links.next;
            /* remove the block from the quick list as it is no longer free*/
            break;
        } /* found a quick list with free blocks */
        cur_list_size += 8;
    }
    if(list_ptr != NULL){
        list_ptr -> header = list_ptr -> header & 0xfffffffffffffffb; /* turn off the qklst bit */
        return list_ptr;
    }
        return NULL;
}

/**
 * This function search on a segregated fit list and find the appropriate size class. It handles all the
 * necessary splitting of blocks.
 *
 * @param block: if the block is NULL the function returns the sentinel. If the block is not NULL
 * the function searches for a free block that is of appropriate size.
 *
 * @param size: the size of the block we are looking for.
 *
 * @return the pointer to the free block
 * */
void *search_main_list(int flag ,size_t size){
    int cur_list_size = 32;
    sf_block *list_ptr = &sf_free_list_heads[0];
    sf_block *sentinel = &sf_free_list_heads[0];
    if(size == 32 && !flag){return sentinel;}

    for(int i = 1; i < NUM_FREE_LISTS; i++){
        list_ptr = &sf_free_list_heads[i];
        sentinel = &sf_free_list_heads[i]; // first block is always the sentinel
        sf_header cur_block_size = (list_ptr -> body.links.next) -> header & 0xfffffffffffffff8;

        if(i == 9 && !flag){return sentinel;}

        if((cur_list_size < size && size <=( 2 * cur_list_size)) && !flag){
            if(!flag) return sentinel;
        }

        if((size <= cur_list_size && flag) || i == 9){
            if(list_ptr -> body.links.next != list_ptr){
                while(size < cur_block_size){
                    if(list_ptr -> body.links.next == sentinel) break;
                    list_ptr = list_ptr -> body.links.next;
                } /* search for matching block*/

                if(list_ptr != sentinel){
                    /* remove the block from the free list */
                    list_ptr = remove_doubly(list_ptr);
                    list_ptr -> header |= 0x1; //return the block that the user is going to use, so set the alloc bit
                    break; /* found a free block */
                }
            } /* and the list is non-empty */

         /* found a matching size class  */
        }
        cur_list_size *=2;
    }

    /* split the free block if needed */
    list_ptr = split_and_reinsert(list_ptr,size);
    return list_ptr;
}

/*
 * This function requests for more memory and also takes care of the
 * splitting and coalescing.
 *
 * @param the block size after alignment
 *
 * @return On success, the function returns a memory address pointing
 * the allocated block payload area. Otherwise, returns NULL indicating that heap is
 * out of memory and sets the sf_errno to ENOMEM.
 */
void *grow_wrapper(size_t size){
    size_t extended_mem = 0;
    sf_block *block = NULL;
    sf_block *remeber = block;
    /* Request for a page, then init prologue and epilogue if is first time otherwise
    only move epilogue address. Then initialize the free block with all the necessary header and footer,
    then insert it into the main free list. Coalese the current block with previous block if is free, then
    move them to a new size class in the main free list.
    */
    while(extended_mem < size){
        block = extend_heap(init);
        if(block == NULL){
            block = search_main_list(0,size);
            insert_doubly(block,remeber);
            sf_errno = ENOMEM;
            return NULL;
        }
        extended_mem += init ? PAGE_SZ-40 : PAGE_SZ;
        init = 0;
        remeber = block;
    }

    block = split_and_reinsert(block,size);
    block -> header |= 0x3; //return the block that the user is going to use, so set the alloc bit and previous alloc bit because prologue
    sf_header* epilogue = (sf_header *) (((uintptr_t) block) + (((uintptr_t) block -> header) & 0xfffffffffffffff8));
    *epilogue |= 0x0000000000000002;
    uintptr_t ret = ((uintptr_t) block) + 8;
    return (sf_block *) ret;
}


/**
 * This function extends heap and handles the change of new epilogue address depends
 * on if the extend heap is first time or not.
 *
 * @return the address immediately after the proglogue.
 * */
void *extend_heap(int init){
    sf_block *block = sf_mem_grow(); /* request kernel for memory */
    if(block == NULL){return NULL;}
    uintptr_t ret = (uintptr_t) block; // ret is 8 byte after epilogue on subsequent calls
    uintptr_t epi = (uintptr_t) sf_mem_end();
    epi-=8;

    if(init){
        /* prologue */
        block -> header = 32;
        block -> header = block -> header | 0x0000000000000001; // set alloc bit
        ret += 32;
        /* epilogue */
        sf_header *epilogue = (sf_header *) epi;
        *epilogue = 1;
        /* create free block PAGE_SZ - 40 because excluding the header and footer*/

        block = init_free_block((sf_block *) ret, PAGE_SZ-40);
        /* 4096 - (proglogue) - (epilogue)*/
    }else{

        /* new epilogue */
        *((sf_header *) epi) = 1;
        /* old epilogue becomes the new header */
        uintptr_t new_header_pos =( (uintptr_t) block) - 8;
        /* create free block */

        block = init_free_block((sf_block *) new_header_pos,PAGE_SZ);
        block = coalesce(block);
    }

    return block;
}

/**
 * This function initialize the fields required inside a free block.
 *
 * @param sf_block* block: the block with the address immediately proceeding the epilogue
 * @param size_t size: the size of the block that want to initialize.
 *
 * @return The block after initialization of header,footer, next pointer, previous pointer.
 * */
void *init_free_block(sf_block* block, size_t size){

    uintptr_t old_epi_indicator = (block -> header) & 0x2; // preserve the prevalloc bit from the previous epilogue
    block -> header = size | old_epi_indicator; // size of payload is block size - header size
    uintptr_t footer_addr = (uintptr_t) (block);
    footer_addr = footer_addr + size - 8;
    sf_header *footer = (sf_header *) footer_addr;
    *footer = block -> header;

    return block;
}

/**
 * This function uses pointer to the free block(starting at the header) and
 * coalesce it with the free block immediately preceding it and/or proceeding it.
 *
 * @return the new coalesced block
 * */
void *coalesce(sf_block *block){

    int prev_alloc = block -> header & 0x2;
    size_t current_size = block -> header & 0xfffffffffffffff8;
    uintptr_t footer_addr = ((uintptr_t) block) + (current_size) - 8;
    int next_alloc = ((sf_block *) (footer_addr + 8)) -> header & 0x1;
    int curr_alloc = block -> header & 0x1;

    /* four cases */
    if(!prev_alloc && !next_alloc){ //both previous and next block are free
        size_t prev_size = (sf_header ) ((uintptr_t) block - 8);
        sf_block *next_block = (sf_block *) ((uintptr_t) block + ((uintptr_t) block -> header));
        sf_header *new_footer = (sf_header *) (((uintptr_t) next_block) + next_block -> header);
        *new_footer = *new_footer + prev_size + block -> header;
        sf_block * new_block =  (sf_block *)((uintptr_t) block - 8 - prev_size);
        new_block -> header = *new_footer;
        block = new_block;
        if(curr_alloc) remove_doubly(new_block);
        if(curr_alloc) remove_doubly(next_block);
    }else if(!prev_alloc && next_alloc){ //only previous block is free
        size_t prev_size = *((sf_header*) (((uintptr_t) block) - 8)) & 0xfffffffffffffff8;
        uintptr_t prev_block_addr = ((uintptr_t) block) - prev_size;
        ((sf_block *) prev_block_addr) -> header =  (((sf_block *) prev_block_addr) -> header) + current_size;
        *((sf_header *) footer_addr) = ((sf_block *) prev_block_addr) -> header;
        block = (sf_block *) prev_block_addr;
        if(curr_alloc) remove_doubly((sf_block *) prev_block_addr);
    }else if(prev_alloc && !next_alloc){ //only next block are free
       sf_block *next_block = (sf_block *) ((((uintptr_t) block -> header) & 0xfffffffffffffff8) + (uintptr_t) block);
       block -> header = block -> header  + ((size_t) ((next_block -> header) & 0xfffffffffffffff8));
       sf_header *next_footer = (sf_header*) (((uintptr_t) block) + ((uintptr_t) next_block -> header));
       *next_footer = block -> header;
       if(curr_alloc) remove_doubly(next_block);
    }else{
        return block;
    }

    return block;
}

/**
 * This function uses size and pointer to the free block to determine how to
 * split. The remainder (higher-value address will be re-inserted into the
 * main free list)
 *
 * @return the allocated block after splitting.
 * */
void *split_and_reinsert(sf_block *block, size_t size){
    size_t size_of_block = block -> header & 0xfffffffffffffff8;
    sf_block *split = block;
    sf_block *remainder =NULL;
    /* split block */
    if(size_of_block - size < 32) return block; // if the remainder size < 32 it is a splinter.
    block -> header = size | ((block -> header) & 0x7);
    sf_header *footer = (sf_header *) (((uintptr_t) block) + ((block -> header) & 0xfffffffffffffff8) - 8 );
    *footer = block -> header;

    remainder = (sf_block *) ((((uintptr_t) footer) + 8));
    remainder -> header = size_of_block - size;
    sf_header *remainder_footer = (sf_header *) (((uintptr_t) remainder) + ((remainder -> header) & 0xfffffffffffffff8) - 8);
    *remainder_footer = remainder -> header;
    /* re-insert the remainder block */
    sf_block *sentinel = search_main_list(0,size_of_block - size);
    insert_doubly(sentinel,remainder);
    set_prev_alloc(remainder, 0x0000000000000002,1);// set the prev_alloc bit true for the remainder block
    return split;
}


/**
 * This function takes the sentinel node and insert the block into the list
 *
 * @param sentinel: the first block of the main free list of a specific size class
 * @param block: the block that is intended to be inserted in the free list
 *
 * */
void insert_doubly(sf_block *sentinel,sf_block *block){
    sf_block *temp = sentinel -> body.links.next;
    sentinel -> body.links.next = block;
    temp -> body.links.prev = block;
    block -> body.links.next = temp;
    block -> body.links.prev = sentinel;
}

sf_block *remove_doubly(sf_block *block){
    sf_block *prev = block -> body.links.prev;
    sf_block *next = block -> body.links.next;
    prev -> body.links.next = next;
    next -> body.links.prev = prev;

    return block;
}

/**
 * This function that takes the block with the starting address of the header and
 * changes the prev_alloc_bit (in the footer as well).
 *
 * */
void set_prev_alloc(sf_block *block, size_t prev_alloc_bit,int flag){
    block -> header = flag ? block -> header | prev_alloc_bit : block-> header & prev_alloc_bit;
    uintptr_t footer_addr = ((uintptr_t) block) + (((uintptr_t) block -> header) & 0xfffffffffffffff8) - 8;
    sf_header *footer = (sf_header *) footer_addr;
    *footer = block -> header;
}

/**
 * This function takes a free list head as an input and flush the blocks and insert them
 * into the main list.
 * */
void flush_quick_list(sf_block *first){
    sf_block *ptr = first;
    size_t size = first -> header & 0xfffffffffffffff8;
    while(ptr -> body.links.next != NULL){
        sf_block *block = coalesce(ptr);
        if(block == ptr){ /* the block can't be coalesced so insert into main list */
            sf_block *sentinel = search_main_list(0,size);
            insert_doubly(sentinel,block);
        }else{ /* the block can be coalesced so we don't need to insert it again it is coalesced with
                  existing block in the main free list already */
            continue;
        }
    }
}

/**
 * This function takes a block and insert the block into the free list of the appropriate class size
 * */
void insert_quick_list(sf_block *block){
    size_t cur_list_size = 32; // starting size
    size_t block_size = block -> header & 0xfffffffffffffff8;
    sf_block* first = NULL;
    int i = 0;

    for(; i < NUM_QUICK_LISTS; i++){
        if(block_size == cur_list_size){ first = sf_quick_lists[i].first; break;}
        cur_list_size += 8;
    }

    if(sf_quick_lists[i].length == 5){
        flush_quick_list(first);
    } /* max capacity */

    /* insert into quicklist */
    sf_block *temp = first == NULL ? NULL : first -> body.links.next;
    if(temp == NULL){
        sf_quick_lists[i].first = &(*block);
        block -> body.links.next = NULL;
        sf_quick_lists[i].length += 1;
    }else{
        block -> body.links.next = temp;
        first -> body.links.next = block;
        sf_quick_lists[i].length += 1;
    }

    block -> header |= 0x0000000000000005;
}

size_t check_valid_pp(void *pp){
    /* check if is valid pointer */
    if(pp == NULL) abort();
    uintptr_t header_addr =  ((uintptr_t) pp) -8;
    sf_block *block = (sf_block *) header_addr;
    uintptr_t footer_addr = header_addr + (((sf_block *) header_addr) -> header & 0xfffffffffffffff8) - 8;
    size_t block_size = (*((sf_header *) (header_addr)) & 0xfffffffffffffff8);

    if(((uintptr_t) pp ) & 7 || //block is not 8 byte aligned
      (*((sf_header *) header_addr) & 0xfffffffffffffff8) & 7 || //block size is not multiple of 8
      *((sf_header *)(header_addr)) < 32 || //block size is less than 32
      header_addr < (uintptr_t) (sf_mem_start()) || //start of header is less than heap start
      footer_addr > (uintptr_t) (sf_mem_end()) || //end of block is greater than heap end
      !(*((sf_header *) header_addr) & 0x1) ||//block is not allocated
      *((sf_header *) header_addr) & 0x4 //block is inside quick list
      ) abort();

    /* previous block and current block is not consistent with the prev_alloc bit and alloc_bit */
    if(!(block -> header & 0x2)){
        sf_header * prev_footer  = (sf_header *) (header_addr - 8);
        if(*prev_footer & 0x1) abort(); // prev_alloc is true but previous block is actually allocated
    }/* prev_alloc is false */

    return block_size;
}