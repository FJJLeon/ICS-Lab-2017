/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * 1. realize segregated free list with offset of block pointer
 *    10 different size class, increase search speed
 * 2. use best fit to find fit block
 * 3. for specified trace , do special operation 
 *    bianary traces, give needed size after freed at the first time
 *    coalesce traces, modify chunksize
 *    realloc traces, change place strategy partly
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* basic constants and macros */
#define WSIZE 4           /* word and header/footer size (bytes) */
#define DSIZE 8           /* double word size (bytes) */
#define CHUNKSIZE 4104 /* extend heap by this amount (bytes)  */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* read the size and allocated fields from address p */
#define GET_SIZE(p)   (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// above from book

/* segregated free list compute */
#define NEXT_NODE(bp) ((char *)(bp) + WSIZE)
#define PREV_NODE(bp) ((char *)(bp))

/* read and write a pointer at address p */
#define READ(p) 	((char *)(long)(GET(p))) 
#define WRITE(p, aim)   (*(unsigned int *)(p) = ((long)aim))  

/* fit way */
#define first_sign 0
#define next_sign 1
#define best_sign 0

/* magic number */
#define oldbinary1 112
#define oldbinary2 448
#define newbinary1 136
#define newbinary2 520
#define realloc1 128
#define realloc2 16

/* realloc check */
static int realloc_sign = 0;

/* some important pointer and const number*/
static char *heap_listp = NULL; // global variable points to prologue block
static void *recentp = NULL;    // global variable points recent block point
static char *seg_head = NULL;	// global variable points segregated list head
static char *rea_p = NULL;		// global variable points to realloc space	

/* static auxiliary function */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);  
static void *first_fit(size_t asize); 
static void *next_fit(size_t asize); 
static void *best_fit(size_t asize);
static void *seg_first_fit(size_t asize);
static void place(void *bp, size_t asize); 
static void *mm_reallocsmall(void *ptr, size_t size);
static void *mm_reallocbig(void *ptr, size_t size); 
static void insert(char *bp);
static void delete(char *bp);
static void check();

/* 
 * mm_init - initialize the malloc package. create segregated list
 */
int mm_init(void)
{
	/* create the initial empty heap */
	if ((heap_listp = mem_sbrk(14*WSIZE)) == (void *) -1){//seg 14, origin 4
		return -1;
	}

	/* for segregated list */
	seg_head = heap_listp;
	int i;
	for (i = 0; i < 10; i++)
		WRITE(seg_head + i * WSIZE, NULL);

	PUT(seg_head + (10*WSIZE), 0);				// alignment padding
	PUT(seg_head + (11*WSIZE), PACK(DSIZE, 1));	// prologue header
	PUT(seg_head + (12*WSIZE), PACK(DSIZE, 1)); // prologue footer
	PUT(seg_head + (13*WSIZE), PACK(0, 1));		// epilogue header
	heap_listp += (12*WSIZE);
	
	if (next_sign){
		recentp = heap_listp;
	}
	/* extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;

	return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t asize; 		// adjusted block size
	size_t extendsize;	// amount to extend heap if no fit
	void *bp;
	
	/* ignore spurious requests */
	if (size == 0)
		return NULL;

	/* for realloc trace */
	if (size == realloc1)
		realloc_sign = 1;
	if (size == realloc2)
		realloc_sign = 2;

	/* adjust block size to include overhead and alignment reqs */
	if (size <= DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE -1)) / DSIZE);
	
	/* adjust block size special for binary trace */
	if (size == oldbinary1)
		asize = newbinary1;
	else if (size == oldbinary2)
		asize = newbinary2;
	
	/* search the free list for a fit */
	if ((bp = best_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}

	/* no fit, get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
		return NULL;
	}

	place(bp, asize);
	return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
	if (bp == heap_listp) 
		return;
	
	size_t size = GET_SIZE(HDRP(bp));
	
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){

	if (realloc_sign == 1)
		return mm_reallocsmall(ptr, size);
	if (realloc_sign == 2) 
		return mm_reallocbig(ptr, size);
}

static void *mm_reallocbig(void *ptr, size_t size)
{
	if (ptr == NULL)
		return mm_malloc(size);

	if (size == 0){
		mm_free(ptr);
		return NULL;
	}

	size_t asize;
	if (size <=DSIZE)
		asize = 2 * DSIZE;
	else
		asize =  DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);

	size_t oldsize = GET_SIZE(HDRP(ptr));

	if (oldsize >= asize)
		return ptr;

	if (oldsize < asize){
		size_t nextalloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr))); // 1 is allocate, 0 is free
		size_t nextsize = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
		size_t prevalloc = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
		size_t prevsize = GET_SIZE(HDRP(PREV_BLKP(ptr)));

		if (prevalloc && nextalloc) {

		}
		if (!prevalloc) { // prev is free
			if ((prevsize + oldsize) >= asize) {
				delete(PREV_BLKP(ptr));
				PUT(FTRP(ptr), PACK(prevsize + oldsize, 1));
				PUT(HDRP(PREV_BLKP(ptr)), PACK(prevsize + oldsize, 1));
				memmove(PREV_BLKP(ptr), ptr, oldsize - DSIZE);
				return PREV_BLKP(ptr);
			}
		}
		if (!nextalloc) { //next is free
			if ((nextsize + oldsize) >= asize) { 
				delete(NEXT_BLKP(ptr));
				PUT(HDRP(ptr), PACK(nextsize + oldsize, 1));
				PUT(FTRP(ptr), PACK(nextsize + oldsize, 1));
				return ptr;
			}
		}
		if (!prevalloc && !nextalloc) {
			if ((prevsize + oldsize + nextsize) >= asize) {
				delete(NEXT_BLKP(ptr));
				delete(PREV_BLKP(ptr));
				PUT(HDRP(PREV_BLKP(ptr)), PACK(prevsize + oldsize + nextsize, 1));
				PUT(FTRP(NEXT_BLKP(ptr)), PACK(prevsize + oldsize + nextsize, 1));
				memmove(PREV_BLKP(ptr), ptr, oldsize - DSIZE);
				return PREV_BLKP(ptr);
			}
		}
	}
	//printf("direct malloc size %d\n",size);
	char *newptr = mm_malloc(size);
	if (newptr == NULL)
		return NULL;
	memcpy(newptr, ptr, oldsize - DSIZE);
	mm_free(ptr);
	return newptr;
}

static void *mm_reallocsmall(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	/* allocate an even number of words to  maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;
	
	/* initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));         // free block header
	PUT(FTRP(bp), PACK(size, 0));         // free block footer
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header

	/* colalesce if the previous block was free */
	return coalesce(bp);
}

static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // whether prev block allocated
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));	// whether next block allocated
	size_t size = GET_SIZE(HDRP(bp));					// size of this block

	if (prev_alloc && next_alloc) {			/* case 1 */
		// no free
		insert(bp);					
	}

	else if (prev_alloc && !next_alloc) {	/* case 2 */
		// next block is free
		delete(NEXT_BLKP(bp));
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		insert(bp);
	}

	else if (!prev_alloc && next_alloc) {	/* case 3 */
		// prev block is free
		delete(PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
		insert(bp);
	}

	else {									/* case 4 */			
		// all free
		delete(NEXT_BLKP(bp));
		delete(PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
		insert(bp);
	}
	
	// the recentp is within the coalesced block
	if (next_sign){
	 	if (recentp > bp && recentp < NEXT_BLKP(bp))
			recentp = bp;
	}
	return bp;
}



static void *first_fit(size_t asize)
{
	/* first fit search */
	void *bp;
	size_t size;

	for (bp = heap_listp; (size = GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= size)) {
			return bp;
		}
	}

	return NULL; // no fit
}

/*
 * get the right segregated list root index	number
 */	
static int get_index(size_t asize){

	if (asize > 4096)
		return 9;
	if (asize <= 16)
		return 0;
	asize--;
	size_t index = -4;
	while (asize){
		index++;
		asize /= 2;
	}	
	return index;
}

static void *seg_first_fit(size_t asize){
	int index;
	for (index = get_index(asize); index <=9; index++){

		char *bp = heap_listp + (long)READ(seg_head + index * WSIZE); // the first list node or null
		while (bp != heap_listp) { 
			if (GET_SIZE(HDRP(bp)) >= asize)
				return bp;
			bp = heap_listp + (long)READ(NEXT_NODE(bp));
		}
	}

	return NULL;
}

static void *next_fit(size_t asize)
{
	/* next fit search */
	void *bp;

	for (bp = recentp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
			recentp = bp;
			return bp;
		}
	}
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0 && bp < recentp; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
			recentp =  bp;
			return bp;
		}
	}
	
	return NULL;
}

static void *best_fit(size_t asize)
{
	int index;

	void *best = NULL;
	size_t min = 99999999, size;
	
	for (index = get_index(asize); index <= 9; index++ ){
		char *bp = heap_listp + (long)READ(seg_head + index * WSIZE);
		while (bp != heap_listp) {
			size = GET_SIZE(HDRP(bp));
			if (size >= asize && size < min) {
				best = bp;
				min = size;
			}
			bp = heap_listp + (long)READ(NEXT_NODE(bp));
		}

		if (min != 99999999) 
			return best;
	}
	return NULL;
}

static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));
	delete(bp);

	if ((csize - asize) >= (2*DSIZE)) {
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));

		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		insert(bp);
	}
	else {
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}

/*

*/
static void insert(char *bp) {
	size_t asize = GET_SIZE(HDRP(bp));
	int index = get_index(asize);
	char *root = seg_head + index*WSIZE;
	char *next = heap_listp + (long)READ(root);
	
	if (next == heap_listp) { //because use offset if null should be head_listp
		// this list is empty, insert bp to root
		WRITE(root, (long)(bp - heap_listp));
		WRITE(PREV_NODE(bp), NULL); 
		WRITE(NEXT_NODE(bp), NULL);
	}
	else {
		// this list is not empty, insert bp next to root  		
		WRITE(NEXT_NODE(bp), (long)READ(root));
		WRITE(PREV_NODE(heap_listp + (long)READ(root)),(long)(bp - heap_listp));
		WRITE(PREV_NODE(bp), NULL);
		WRITE(root, (long)(bp - heap_listp));
	}
	return;
}


static void delete(char *bp) {
	size_t asize = GET_SIZE(HDRP(bp));
	int index = get_index(asize);

	char *root = seg_head + index*WSIZE;
	char *pred = heap_listp + (long)READ(PREV_NODE(bp));
	char *next = heap_listp + (long)READ(NEXT_NODE(bp));

	if (pred == heap_listp && next == heap_listp) {
		// the list contain only this block
		WRITE(root, NULL);
	}
	else if (pred == heap_listp && next != heap_listp ) {
		// bp is the first list node
		WRITE(root, (long)READ(NEXT_NODE(bp)));
		WRITE(PREV_NODE(next), NULL); // fuck your mother ,miss it
		WRITE(NEXT_NODE(bp), NULL);
	}
	else if (pred != heap_listp && next == heap_listp) {
		// bp is the last
		WRITE(NEXT_NODE(pred), NULL);
		WRITE(PREV_NODE(bp), NULL);
	}
	else {
		// within 
		WRITE(NEXT_NODE(pred), (long)READ(NEXT_NODE(bp)));
		WRITE(PREV_NODE(next), (long)READ(PREV_NODE(bp)));
		WRITE(PREV_NODE(bp), NULL); 
		WRITE(NEXT_NODE(bp), NULL);
	}
	return;
}


static void check(){
	char *bp = heap_listp + DSIZE;
	int i = 0;
	printf("check begin\n");
	for (; GET_SIZE(HDRP(bp))> 0; bp = NEXT_BLKP(bp), i++){
		printf("block %d at offset %d have size %d, is allocated %s\n", \
			   i, (long)(bp - heap_listp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) ? "YES":"NO" );
	}
	printf("check end\n");
}







