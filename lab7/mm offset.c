/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
#define CHUNKSIZE (1<<12) /* extend heap by this amount (bytes)  */

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

/* cast 32 bit address stored to 48 bit heap address, the high 16 bit is same ?????really???*/
//#define MYCAST(bp)  (((long)mem_heap_lo() & 0xffff00000000) | (long)(bp))

/* read and write a pointer at address p */
#define NOUSEREAD(p) 	((unsigned int *)(long)(GET(p))) 
//#define newread(p)			MYCAST(NOUSEREAD(p)) 
#define WRITE(p, aim)   (*(unsigned int *)(p) = ((long)aim))  

/* fit way */
#define first_sign 0
#define next_sign 1
#define best_sign 0

/* test realloc */
static int realloc_sign = 1;

/* some important pointer and const number*/
static char *heap_listp = NULL; // global variable points to prologue block
static void *recentp = NULL;    // global variable points recent block point
static char *seg_head = NULL;	// global variable points segregated list head
static char *rea_p = NULL;		// global variable points to realloc space	

//static size_t heap_lo = mem_heap_lo(); // the address of heap low
//static size_t heap_hi = mem_heap_hi(); // the address of heap high
//GET_SIZE(HDRP(((long)mem_heap_lo() & 0xffff00000000) | (long)newread(root))))

/* static auxiliary function */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);  
static void *first_fit(size_t asize); 
static void *next_fit(size_t asize); 
static void *best_fit(size_t asize);
static void *seg_first_fit(size_t asize);
static void place(void *bp, size_t asize);  
static void insert(void *bp);
static void delete(void *bp);

static long *newread(unsigned int *bp){
	void *heap_lo = mem_heap_lo(); // the address of heap low
	void *heap_hi = mem_heap_hi(); // the address of heap high
	return (((long)heap_lo & 0xffff00000000) | (long)(bp));
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //system("pause");
	//size_t size = 8000000;
	//size_t a = size <= 4096 ? ceil(log(size)/log(2))-4 : 9;
	//printf("init ceil %d\n", a);
	printf("long size%d, unsign int size %d \n", sizeof(long), sizeof(unsigned int));
	/* create the initial empty heap */
	if ((heap_listp = mem_sbrk(14*WSIZE)) == (void *) -1){//seg 14, origin 4
		printf("init error\n");
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
	
	/* for segregated list */
	/*PUT(seg_head, 0);				// <=16
	PUT(seg_head + (1*WSIZE), 0);	// <=32	
	PUT(seg_head + (2*WSIZE), 0);	// <=64
	PUT(seg_head + (3*WSIZE), 0);	// <=128
	PUT(seg_head + (4*WSIZE), 0);	// <=256
	PUT(seg_head + (5*WSIZE), 0);	// <=512
	PUT(seg_head + (6*WSIZE), 0);	// <=1024
	PUT(seg_head + (7*WSIZE), 0);	// <=2048
	PUT(seg_head + (8*WSIZE), 0);	// <=4096
	PUT(seg_head + (9*WSIZE), 0);	// >4096
	PUT(seg_head + (10*WSIZE), 0);				// alignment padding
	PUT(seg_head + (11*WSIZE), PACK(DSIZE, 1));	// prologue header
	PUT(seg_head + (12*WSIZE), PACK(DSIZE, 1)); // prologue footer
	PUT(seg_head + (13*WSIZE), PACK(0, 1));		// epilogue header
	*/
	printf("init 1\n");	
	

	if (next_sign){
		recentp = heap_listp;
	}
	/* extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;
	
	printf("init 2 \n");
	return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  	//system("pause");
	printf("malloc 1 %d\n",size);
  /* // origin
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
  */
    
	size_t asize; 		// adjusted block size
	size_t extendsize;	// amount to extend heap if no fit
	void *bp;
	
	/* ignore spurious requests */
	if (size == 0)
		return NULL;
	
	/* adjust block size to include overhead and alignment reqs */
	if (size <= DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE -1)) / DSIZE);
	
	/* search the free list for a fit */
	if ((bp = seg_first_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}
	printf("malloc 88 %d\n",size);
	/* no fit, get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL) 
		return NULL;
	printf("malloc 2 %d\n",size);
	place(bp, asize);
	printf("malloc last %d\n",size);
	return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
	if (bp == NULL) 
		return;
	
	size_t size = GET_SIZE(HDRP(bp));
	
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	//if (size != 640 && realloc_sign == 1){
	//printf("realloc 0 size:%d\n",size);
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
	/*}

	else {
		printf("realloc 1 size:%d\n",size);
		if (realloc_sign){
			printf("realloc 2 size:%d\n",size);
			size_t copySize;
			copySize = *(size_t *)((char *)ptr - SIZE_T_SIZE);
			printf("aaaaaa\n");
			rea_p = extend_heap((614784+8)/WSIZE);
			printf("bbbbbb\n");
			PUT(HDRP(rea_p), PACK(size, 1));
			PUT(FTRP(rea_p), PACK(size, 1));
			printf("cccccc\n");
			
			if (size < copySize)  
				copySize = size; 
			memcpy(rea_p, ptr, copySize);
			printf("dddddd\n");
			mm_free(ptr);
			printf("eeeeee\n");
			realloc_sign = 0;
			return rea_p;
		}
		else{
			size_t copySize;
			copySize = *(size_t *)((char *)ptr - SIZE_T_SIZE);
			
			PUT(HDRP(rea_p), PACK(size, 1));
			PUT(FTRP(rea_p), PACK(size, 1));
			if (size < copySize)
				copySize = size;
			memcpy(rea_p, ptr, copySize);

			return rea_p;
		}
	}
	*/
}

static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;
	
	printf("extend\t");

	/* allocate an even number of words to  maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;
	
	/* initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));         // free block header
	PUT(FTRP(bp), PACK(size, 0));         // free block footer
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header
 	
	printf("extend size %d\n", size);
	/* colalesce if the previous block was free */
	return coalesce(bp);
}

static void *coalesce(void *bp)
{
	printf("coalesce1 %d\n", GET_SIZE(HDRP(bp)));

	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // whether prev block allocated
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));	// whether next block allocated
	size_t size = GET_SIZE(HDRP(bp));					// size of this block
	
	printf("coalesce %d\n", prev_alloc);
	printf("coalesce %d\n", next_alloc);

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
	printf("coalesce %d\n", size);
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
 * get the right segregated list root index	
 */	
static int get_index(size_t asize){
	/*	
	size_t index = asize <= 4096 ? ceil(log(asize)/log(2)) - 4 : 9;
	printf("index is %d\n", index);
	*/
	
	if (asize > 4096)
		return 9;
	if (asize <= 16)
		return 0;
	size_t index = -4;
	while (asize){
		index++;
		asize /= 2;
	}
	
	return index;
}

static void *seg_first_fit(size_t asize){
	int index;
	printf("seg fit 1 %d\n",asize);

	for (index = get_index(asize); index <=9; index++){
		printf("seg fit index xx %d\n",index);    
		
		char *bp =newread(seg_head + index * WSIZE); // the first list node or null
		while (bp != NULL) {
			printf("get size %d\n",GET_SIZE(HDRP(bp)));    
			if (GET_SIZE(HDRP(bp)) >= asize)
				return bp;
			bp = newread(NEXT_NODE(bp));
		}
	}

	return NULL;
}

static void *next_fit(size_t asize)
{
	/* next fit search */
	void *bp;
	//printf("next 1 ");	
	for (bp = recentp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
			recentp = bp;
			return bp;
		}
	}
	//printf("next 2 ");
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
	void *bp;
	void *best = NULL;
	size_t min = 99999999, size;

	for (bp = heap_listp; (size = GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && asize <= size && size < min) {
			best = bp;
			min = size;
		}	
	}

	return best;
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
static void insert(void *bp) {
	size_t asize = GET_SIZE(HDRP(bp));
	int index = get_index(asize);
	char *root = seg_head + index*WSIZE;
	char *pred = root; 
	char *next = newread(root);
printf("insert %d\n", index); //9
	/*
	while (next != NULL) {
		if (GET_SIZE(HDRP(next) >= GET_SIZE(HDRP(bp))))
			break;
		pred = next;

		next = GET(NEXT_NODE(next));
	}
printf("insert %d\n", 222); 
	if (pred == root) {
printf("insert %d\n", 333); 
		PUT(PREV_NODE(bp), root); // BUG?
		PUT(NEXT_NODE(bp), next);
		PUT(root, bp);
printf("insert bp size is %d\n", GET_SIZE(HDRP(GET(root))));
	}
	else {
		PUT(PREV_NODE(bp), pred);
		PUT(NEXT_NODE(bp), next);
		PUT(NEXT_NODE(pred), bp);
	}

	if (next != NULL)
		PUT(PREV_NODE(next), bp);
	*/
	
	if (next == NULL) {
		// this list is empty, insert bp to root
		WRITE(root, bp);
		WRITE(PREV_NODE(bp), NULL); //NULL ? root? no diff
		//((unsigned *)bp + 1) == ((char *)bp + WSIZE)
		WRITE(NEXT_NODE(bp), NULL);
		printf("heap first %p, last %p \n", mem_heap_lo(), mem_heap_hi());
		printf("insert %d, bp at %p root point at %p\n", GET_SIZE(HDRP(bp)),bp, newread(root));
		printf("gg...%d\n",GET_SIZE(HDRP(newread(root))) );
	}
	else {
		// this list is not empty, insert bp next to root  
		printf("list not empty\n");
		
		WRITE(NEXT_NODE(bp), newread(root));
		WRITE(PREV_NODE(newread(root)), bp);
		WRITE(PREV_NODE(bp), NULL);
		WRITE(root, bp);

		printf("heap first %p, last %p \n", mem_heap_lo(), mem_heap_hi());
		printf("insert %d, bp at %p root point at %p\n", GET_SIZE(HDRP(bp)),bp, newread(root));
		printf("gg...%d\n",GET_SIZE(HDRP(((long)newread(root)))));
	}
	return;
}


static void delete(void *bp) {
	size_t asize = GET_SIZE(HDRP(bp));
	int index = get_index(asize);

	char *root = seg_head + index*WSIZE;
	char *pred = newread(PREV_NODE(bp));
	char *next = newread(NEXT_NODE(bp));

	/*if (pred == root) {
		PUT(root, next);// root = pred
		
	}
	else {
		PUT(NEXT_NODE(pred), next);
	}
	
	if (next != NULL)
		PUT(PREV_NODE(next), pred);
	
	PUT(NEXT_NODE(bp), NULL);
	PUT(PREV_NODE(bp), NULL);*/

	if (pred == NULL && next == NULL) {
		// the list contain only this block
		WRITE(root, NULL);
	}
	else if (pred == NULL && next != NULL ) {
		// bp is the first list node
		WRITE(root, newread(NEXT_NODE(bp)));
		WRITE(NEXT_NODE(bp), NULL);
	}
	else if (pred != NULL && next == NULL) {
		// bp is the last
		WRITE(NEXT_NODE(pred), NULL);
		WRITE(PREV_NODE(bp), NULL);
	}
	else {
		// within 
		WRITE(NEXT_NODE(pred), next);
		WRITE(PREV_NODE(next), pred);
	}

	return;
}










