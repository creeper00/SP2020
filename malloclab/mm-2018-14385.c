/* mm.c - Segregated List && Best Fit
*
* In this approach, we use doubly linked blocks to indicate free blocks
* Every blocks has its own header and footer.
* Each Free block has a pointers to its previous and next free blokcs
* This approach uses Segregated List for each size range of free blocks ( range : 1 << (1~19) )
* Also, this approach uses Best-Fit algorithmn when finding free blocks in a list
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

/* word size and double word size def */
#define WSIZE 4
#define DSIZE 8

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* base size of extend */
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y)? (x) : (y)) 

/* pack size and allocated field */
#define PACK(size, alloc) ((size) | (alloc)) 

/* r/w at pointer */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (int)(val))

/* get size and allocation bit */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1) 

/* header, footer, next pointer, prev pointer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 
#define NEXTptr(bp) ((char *)(bp)+0)
#define PREVptr(bp) ((char *)(bp)+4)

/* next pointer and prev pointer instrunction for free blocks */
#define GETNEXT(bp) (GET(NEXTptr(bp)))
#define GETPREV(bp) (GET(PREVptr(bp)))
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* function prototypes */
static void* coalesce(void* bp);
static void* extend_heap(size_t words);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void insertlist(void* bp);
static void removelist(void* bp);
static int getid(int size);
static char** getroot(int id);
int mm_check(void);

/*global variables : comp used in best fit, l1~l19 is roots of segeregated lits */
static char* comp;
static char* heap_listp = 0;
static char *l1 = 0, * l2 = 0, * l3 = 0, * l4 = 0, * l5 = 0, * l6 = 0, * l7 = 0, * l8 = 0, * l9 = 0, * l10 = 0; 
static char* l11 = 0, * l12 = 0, * l13 = 0, * l14 = 0, * l15 = 0, * l16 = 0, * l17 = 0, * l18 = 0, * l19 = 0;

/* get id for list at given size */
static int getid(int size)
{
    int count = 0;
    while (size > 1) {
        size >>= 1;
        count++;
        if (count >= 19) break;
    }
    return count;
}

/* get root of the list at given id */
static char** getroot(int id) {
    switch (id) {
    case 1: return &l1;
    case 2: return &l2;
    case 3: return &l3;
    case 4: return &l4;
    case 5: return &l5;
    case 6: return &l6;
    case 7: return &l7;
    case 8: return &l8;
    case 9: return &l9;
    case 10: return &l10;
    case 11: return &l11;
    case 12: return &l12;
    case 13: return &l13;
    case 14: return &l14;
    case 15: return &l15;
    case 16: return &l16;
    case 17: return &l17;
    case 18: return &l18;
    default: return &l19;
    }
}

/* insert the free block into the list of appropriate size 
*
*   insert the new free block at the head of the list
*/
static void insertlist(void* bp)
{
    int id = getid(GET_SIZE(HDRP(bp)));
    char** root = getroot(id);
    if (*root == heap_listp)
    {
        PUT(NEXTptr(bp), heap_listp);
        PUT(PREVptr(bp), heap_listp);
        *root = bp;
    }
    else
    {
        PUT(NEXTptr(bp), *root);
        PUT(PREVptr(bp), heap_listp);
        PUT(PREVptr(*root), bp);
        *root = bp;
    }
    
}

/* remove the free block from the list 
*   
*   remove the block of corresponding size, link previous and next block of removed block
*/
static void removelist(void* bp)
{
    int id = getid(GET_SIZE(HDRP(bp)));
    char** root = getroot(id);
    if (GETNEXT(bp) == (int)heap_listp && GETPREV(bp) == (int)heap_listp)
    {
        *root = heap_listp;
    }
    else if (GETNEXT(bp) == (int)heap_listp)
    {
        PUT(NEXTptr(GETPREV(bp)), heap_listp);
    }
    else if (GETPREV(bp) == (int)heap_listp)
    {
        PUT(PREVptr(GETNEXT(bp)), (int)heap_listp);
        *root = (char*)GETNEXT(bp);
    }
    else
    {
        PUT(NEXTptr(GETPREV(bp)), GETNEXT(bp));
        PUT(PREVptr(GETNEXT(bp)), GETPREV(bp));
    }
}

/*  coalesce the given block
*
*   check next and previous block, aggregate if its a free block 
*   have four possibilities
*/
static void* coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {
        insertlist(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        removelist(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        removelist(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && !next_alloc) {
        removelist(PREV_BLKP(bp));
        removelist(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insertlist(bp);
    return bp;
}

/* extend the size of heap */
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/*  find a appropriate free block when given size to allocate 
*   
*   get list corresponding to given size
*   if no block available, check list of bigger id
*   use best fit algorithm, if find block identical to given size, return that block
*   if not, find the smallest block that is bigger than the given size
*/
static void* find_fit(size_t asize) {
    int check = 1;
    int id = getid((int)asize);
    void* bp;
    while (id < 20)
    {
        for (bp = *getroot(id); bp != heap_listp; bp = (char*)GETNEXT(bp))
        {
            if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) == asize) return bp;
            else if (!GET_ALLOC(HDRP(bp)) && (asize < GET_SIZE(HDRP(bp)))) {
                if (check == 1) {
                    check = 0;
                    comp = bp;
                }
                else if (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(comp))) {
                    comp = bp;
                }
            }
        }
        if (check == 0) return comp;
        id++;
    }
    return NULL;
}

/*  allocate block of given size 
*   
*   if free block size is bigger than the allocate size, split.
*   else just place the allocate block to free block
*/
static void place(void* bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= 4 * WSIZE) {
            removelist(bp);
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));

            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(csize - asize, 0));
            PUT(FTRP(bp), PACK(csize - asize, 0));
            coalesce(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        removelist(bp);
    }
}

/*
 * mm_init - initialize the malloc package.
 * Initialize everything necessary : allocate heap.. etc.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);

    l1 = heap_listp;
    l2 = heap_listp;
    l3 = heap_listp;
    l4 = heap_listp;
    l5 = heap_listp;
    l6 = heap_listp;
    l7 = heap_listp;
    l8 = heap_listp;
    l9 = heap_listp;
    l10 = heap_listp;
    l11 = heap_listp;
    l12 = heap_listp;
    l13 = heap_listp;
    l14 = heap_listp;
    l15 = heap_listp;
    l16 = heap_listp;
    l17 = heap_listp;
    l18 = heap_listp;
    l19 = heap_listp;
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void* mm_malloc(size_t size)
{
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
 * mm_free - Freeing a block
 */

void mm_free(void* bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - implemented with mm_malloc and mm_free according to C standards
 *  If the new block to reallocate is smaller than the original, we just return original (reduces overhead).
 *  its not, allocate new block and copy the info to the new block. 
 *  If we need to extend the heap, we just call it inside the realloc since its faster than calling it by passing through malloc. 
 *  Then update the info.
*/
void* mm_realloc(void* ptr, size_t size)
{
    size_t olds = GET_SIZE(HDRP(ptr));
    size_t news = size + DSIZE;
    size_t next = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    int check = 0;

    if (olds < news) {
        if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0 || GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0) {
            check = olds + next - news;
            if (check < 0) {
                size_t extendsize = MAX( 0-check, CHUNKSIZE);
                if (extend_heap(extendsize / WSIZE) == NULL) return NULL;
                check = check + extendsize;
            }
            removelist(NEXT_BLKP(ptr));
            PUT(HDRP(ptr), PACK(news + check, 1));
            PUT(FTRP(ptr), PACK(news + check, 1));
        }
        else {
            void* newptr = mm_malloc(news);
            place(newptr, news);
            memcpy(newptr, ptr, news);
            mm_free(ptr);
            return newptr;
        }
    }
    return ptr;
}

/* mm_check() - check for heap consistency
*   check :
*   Is every block in the free list marked as free?
*   Are there any contiguous free blocks that somehow escaped coalescing?
*   Do the pointers in the free list point to valid free blocks?
*   Do any allocated blocks overlap?
*   Do the pointers in a heap block point to valid heap addresses?
*   Is size of heap appropriate?
*/
int mm_check()
{
    int id = 1;
    char* bp;
    char* hh = mem_heap_hi();
    char* hl = mem_heap_lo();
    int cont = 0;
    int size = 0;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        size += GET_SIZE(HDRP(bp));
        if (bp > hh || bp < hl)
        {
            printf(" invaild address in heap \n");
            return 0;
        }
        if (GET_ALLOC(HDRP(bp)) == 0)
        {
            if (cont == 1)
            {
                printf(" exists free blcok not coalesced \n");
                return 0;
            }
            else
            {
                cont = 1;
            }
        }
        else 
        {
            if (FTRP(NEXT_BLKP(bp)) != (HDRP(bp) + GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLKP(bp))) - WSIZE)) {
                printf("overlapped blocks.\n");
                return 0;
            }
            cont = 0;
        }
    }

    while (id < 20)
    {
        for (bp = *getroot(id); bp != heap_listp; bp = (char*)GETNEXT(bp))
        {
            if (bp > hh || bp < hl)
            {
                printf(" invaild address in free list \n");
                return 0;
            }
            if (GET_ALLOC(HDRP(bp)) == 1)
            {
                printf(" allocated block in free list \n");
                return 0;
            }
        }
        id++;
    }
    
    if (((int)mem_heapsize() - size) > 16)
    {
        printf("bigger heap size than tracked block \n");
        return 0;
    }
    if (size > (int)mem_heapsize())
    {
        printf("have bigger size than actual heap size \n");
        return 0;
    }
    return 1;
}

