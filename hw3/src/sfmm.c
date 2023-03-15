/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"

static void *search_quick_list(size_t size);
static void *search_main_list(size_t size);
static void *coalesce(sf_block *block);
static void *split(sf_block *block, size_t size);
static void *grow_wrapper(size_t size); /* uses sf_mem_grow but also takes care of the epilogue and alignments */
static void *init_free_block(sf_block* block,size_t size);
static void *eextend_heap(sf_block *block, int *init,size_t size);


void *sf_malloc(size_t size) {
    if(!size) return NULL;

    /* manipulate size so it meets the requirement */
    size = size + 8; // include header size 8 bytes
    while(size & 7){size++;} /* memalign */
    if(size <= 32){
        size += 32 - size;
    }

    if(sf_mem_start == sf_mem_end){
        /* initialize the sentinels for the circular lists */
        for(size_t i = 0; i < sizeof(sf_free_list_heads)/sizeof(sf_block); i++){
            /* set the next and prev links to the sentinel itself */
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        list_ptr = grow_wrapper(size);
        if(list_ptr == NULL) {return NULL;}
    }

    /* check if we have enough space in quick list first then main free list */
    if(size < 32 + (NUM_QUICK_LISTS-1) * 8 ){ /* if the request size could exist in the quick list */
        list_ptr = search_quick_list(size);
        if(list_ptr != NULL) return list_ptr;
    }else{ 
        list_ptr = search_main_list(size);
        if(list_ptr != NULL) return list_ptr;
    } /* search main free list */
    
    return grow_wrapper(size);
}

void sf_free(void *pp) {
    // TO BE IMPLEMENTED
    abort();
}

void *sf_realloc(void *pp, size_t rsize) {
    // TO BE IMPLEMENTED
    abort();
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
    struct sf_block *list_ptr == NULL;
    for(int i = 0; i < NUM_QUICK_LISTS; i++){
        if(size > cur_list_size) continue; /* size does not satisfy request */
        if(sf_quick_lists[i].length != 0) {
            list_ptr = *(sf_quick_lists[i].first); 
            sf_quick_lists[i].first -> sf_quick_lists[i].first
            /* remove the block from the quick list as it is no longer free*/
            break;
        } /* found a quick list with free blocks */
        cur_list_size += 8;
    }
    list_ptr.header -> list_ptr.header & 0xfffffffffffffffb; /* turn off the qklst bit */
    if(list_ptr != NULL) return list_ptr;
    return NULL;
}

/**
 * This function applies first-fit search on a segregated fit list. It handles all the 
 * necessary splitting of blocks.
 * 
 * @return the pointer to the free block.
 * */
void *search_main_list(size_t size){
    int cur_list_size = 32; 
    sf_block *list_ptr = NULL;
    sf_block *sentinel = NULL;
    for(int i = 0; i < NUM_FREE_LISTS; i++){
        list_ptr = &sf_free_list_heads[i];
        sentinel = &sf_free_list_heads[i]; // first block is always the sentinel

        if((cur_list_size < size && size <= 2 * cur_list_size) && list_ptr.body.links.next != & list_ptr.body.links.next){
            sf_header cur_block_size = list_ptr.header & 0xfffffffffffff000; //mask the upper 13 bits for the size of the block
            while(size < cur_block_size){
                if(list_ptr == sentinel) break;
                list_ptr = list_ptr.body.links.next;
            } /* search for matching block*/

            if(list_ptr != sentinel) break; /* found a free block */
        } /* found a matching size class and the list is non-empty */
    }

    /* split the free block if needed */
    list_ptr = split(list_ptr);
    return list_ptr;
}

/* 
 * This function requests for more memory and also takes care of the 
 * splitting and coalescing. 
 * 
 * @param the block size after alignment 
 *
 * @return On success, the function returns a memory address pointing
 * the allocated block. Otherwise, returns NULL indicating that heap is
 * out of memory and sets the sf_errno to ENOMEM.
 */
void *grow_wrapper(size_t size){
    int init = sf_mem_start = sf_mem_end;
    uintptr_t ret = 0;
    size_t extended_mem = 0;
    sf_block* block = NULL;

    /* Request for a page, then init prologue and epilogue if is first time otherwise 
    only move epilogue address. Then initialize the free block with all the necessary header and footer,
    then insert it into the main free list. Coalese the current block with previous block if is free, then
    move them to a new size class in the main free list.
    */
    while(extended_mem < size){
        block = extend_heap(init);
        if(block == NULL){sf_errno = ENOMEM; return NULL;}
        extended_mem += init ? PAGE_SZ-40 : PAGE_SZ;
        init = 0;
    }

    ret = (uintptr_t) block;
    return split((sf_block*) ret,size);
}


/**
 * This function extends heap and handles the change of new epilogue address depends
 * on if the extend heap is first time or not.
 * 
 * @return the address immediately after the proglogue.
 * */
void *extend_heap(int init){
    sf_block *block = sf_mem_grow(); /* request kernel for memory */
    if(block == NULL){sf_errno = ENOMEM; return NULL;}
    uintptr_t ret = (uintptr_t) block; // ret is 8 byte after epilogue on subsequent calls 
    uintptr_t epi = (uintptr_t) sf_mem_end; 
    epi-=8;

    if(init){
        /* prologue */
        block -> header = 32 << 3;
        block -> header = header | 0x0000000000000001; // set alloc bit
        ret += 32;
        /* epilogue */
        sf_header *epilogue = (sf_header *) epi;
        epilogue = 0;

        /* create free block PAGE_SZ - 40 because excluding the header and footer*/
        init_free_block((sf_block *) ret, PAGE_SZ-40);
    }else{

        /* new epilogue */
        sf_header *epilogue = (sf_header *) epi;
        epilogue= 0;

        /* create free block */
        init_free_block(block,PAGE_SZ);
    }

    return coalesce((sf_block *) ret);
}

/**
 * This function initialize the fields required inside a free block. And insert this free block 
 * into the appropriate list in the main freelist
 * 
 * @param the block with the address immediately proceeding the epilogue 
 * 
 * @return The block after initialization of header,footer,pointer to previous free block, and pointer
 * to next free block.
 * */
void *init_free_block(sf_block* block, size_t size){

    sf_block -> header = size << 3;
    sf_header *footer = sf_mem_end(); // get the addr of end of heap
    uintptr_t footer_addr = (uintptr_t *) footer;
    footer_addr += 16; // move up 16 to find the addr of the footer
    sf_header *footer = (sf_header *) footer_addr;
    footer -> 0; // set the footer

    return block;
}

/**
 * This function uses pointer to the free block and coalesce it with the
 * free block immediately preceding it. Move them to a new size class
 * 
 * @return the new coalesced block
 * */
void coalesce(sf_block *block){

}

/**
 * This function uses size and pointer to the free block to determine how to
 * split. The remainder (higher-value address will be re-inserted into the 
 * main free list)
 * 
 * @return the allocated block after splitting.
 * */
void *split(sf_block *block, size_t size){

}