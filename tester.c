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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y)? (x) : (y)) 

#define PACK(size, alloc) ((size) | (alloc)) 

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (int)(val))
#define PUT(p, val) (*(unsigned int *)(p) = (int)(val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1) 

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 
#define NEXTptr(bp) ((char *)(bp)+0)
#define PREVptr(bp) ((char *)(bp)+4)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


static char* heap_listp;
static char* prev;
static char* rootnext;

static void* coalesce(void* bp)
{

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) return bp;
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0)); 
        PUT(FTRP(bp), PACK(size, 0));

        PUT(NEXTptr(bp), GET(NEXTptr(NEXT_BLKP(bp))));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        PUT(NEXTptr(PREV_BLKP(bp)), GET(NEXTptr(bp)));
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        PUT(NEXTptr(PREV_BLKP(bp)), GET(NEXTptr(NEXT_BLKP(bp))));

        bp = PREV_BLKP(bp);
    }

    return bp;
}

static void* extend_heap(size_t words) 
{
    char* bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    PUT(NEXTptr(bp), heap_listp);
    PUT(PREVptr(bp), prev);
    prev = bp;

    return coalesce(bp);
}

static void* find_fit(size_t asize) 
{ 
    void* bp; 
    if (rootnext == heap_listp) return NULL;

    for ( /*bp = rootnext; NEXTptr(bp)!=heap_listp; bp = NEXTptr(bp)*/ bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    { 
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) 
            return bp; 
    } 
    return NULL; 
}

static void place(void* bp, size_t asize) 
{
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2 * DSIZE)) 
    {
        int thisnext = GET(NEXTptr(bp));
        int thisprev = GET(PREVptr(bp));
        char* curr = bp;

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1)); 
        bp = NEXT_BLKP(bp); 
        PUT(HDRP(bp), PACK(csize - asize, 0)); 
        PUT(FTRP(bp), PACK(csize - asize, 0));

        PUT(NEXTptr(bp), thisnext);
        PUT(PREVptr(bp), thisprev);

        if (curr == rootnext) rootnext = bp;

        if (thisprev != heap_listp) 
            PUT(NEXTptr(thisprev), bp);

        if (thisnext != heap_listp) 
            PUT(PREVptr(thisnext), bp);
        else 
            prev = bp;

    } 
    else 
    { 
        PUT(HDRP(bp), PACK(csize, 1)); 
        PUT(FTRP(bp), PACK(csize, 1)); 
        rootnext = NEXTptr(bp);
    } 


}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) return -1;


    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);

    prev = heap_listp;
    rootnext = heap_listp;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    /* int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }*/

    size_t asize; 
    size_t extendsize; 
    char* bp; 
    
    if (size == 0) return NULL;
    if (size <= DSIZE) asize = 2 * DSIZE; 
    else asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE); 
    
    if ((bp = find_fit(asize)) != NULL) 
    { 
        place(bp, asize); 
        return bp; 
    } 
    
    extendsize = MAX(asize, CHUNKSIZE); 
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL; 
    place(bp, asize); 
    
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0)); 
    PUT(FTRP(ptr), PACK(size, 0)); 
    if ( GET_ALLOC(PREV_BLKP(ptr)) && GET_ALLOC(NEXT_BLKP(ptr)) ) 
    {
        int currnext = GET(rootnext);
        PUT(NEXTptr(ptr), currnext);
        PUT(PREVptr(ptr), heap_listp);
        PUT(PREVptr(currnext), ptr);
        rootnext = ptr;
    }
    else if (!GET_ALLOC(PREV_BLKP(ptr)) && GET_ALLOC(NEXT_BLKP(ptr)))
    {
        int currnext = GET(rootnext);
        PUT(NEXTptr(ptr), currnext);
        PUT(NEXTptr(PREV_BLKP(ptr)), ptr);

        PUT(PREVptr(PREV_BLKP(ptr)), heap_listp);
        PUT(PREVptr(ptr), GET(PREV_BLKP(ptr)));
        PUT(PREVptr(currnext), ptr);

        rootnext = GET(PREV_BLKP(ptr));
        coalesce(ptr);
    }
    else if (GET_ALLOC(PREV_BLKP(ptr)) && !GET_ALLOC(NEXT_BLKP(ptr)))
    {
        int currnext = GET(rootnext);
        PUT(NEXTptr(ptr), GET(NEXT_BLKP(ptr)));
        PUT(NEXTptr(NEXT_BLKP(ptr)), currnext);

        PUT(PREVptr(NEXT_BLKP(ptr)), ptr);
        PUT(PREVptr(ptr), heap_listp);
        PUT(PREVptr(currnext), GET(NEXT_BLKP(ptr)));

        rootnext = ptr;
        coalesce(ptr);
    }
    else if (!GET_ALLOC(PREV_BLKP(ptr)) && !GET_ALLOC(NEXT_BLKP(ptr)))
    {
        int currnext = GET(rootnext);
        PUT(NEXTptr(ptr), GET(NEXT_BLKP(ptr)));
        PUT(NEXTptr(NEXT_BLKP(ptr)), currnext);
        PUT(NEXTptr(PREV_BLKP(ptr)), ptr);

        PUT(PREVptr(PREV_BLKP(ptr)), heap_listp);
        PUT(PREVptr(ptr), GET(PREV_BLKP(ptr)));
        PUT(PREVptr(NEXT_BLKP(ptr)), ptr);
        PUT(PREVptr(currnext), GET(NEXT_BLKP(ptr)));

        rootnext = GET(PREV_BLKP(ptr));
        coalesce(ptr);
    }

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
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
