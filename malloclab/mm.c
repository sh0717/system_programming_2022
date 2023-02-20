/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
/*this program should be aligned by double word */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<10)
#define MAX(x,y) ((x)>(y)? (x):(y))
#define MIN(x,y) ((x)>(y)? (y):(x))
#define PACK(size,alloc) ((size)|(alloc))/*size and allocator pack for header or footer*/
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


/*unsigned int is 4 byte*/
#define GET(ptr) (*(unsigned int *)(ptr))
#define PUT(ptr, val) (*(unsigned int*)(ptr)= (val))

#define GET_SIZE(ptr) (GET(ptr) & ~0x7)
#define GET_ALLOC(ptr) (GET(ptr) & 0x1)

/* this is in the heap real heap next prev*/
#define HDRP(bptr) ((char *)(bptr) -WSIZE)
#define FTRP(bptr) ((char *)(bptr) + GET_SIZE(HDRP(bptr))-DSIZE)
#define NEXT_BLKP(bptr) ((char*)(bptr) + GET_SIZE(((char*)(bptr)-WSIZE)))
#define PREV_BLKP(bptr) ((char*)(bptr) - GET_SIZE(((char*)(bptr)-DSIZE)))
/*address assign*/
#define SET_ADD(p,ptr) (*(unsigned int *)(p)= (unsigned int)ptr)
/* actually same with put */

/*this is for free_list */
/*in freelist block pointer is  for pred address*/
#define PRED_PTR(bptr) ((char*)(bptr))
#define NEXT_PTR(bptr) ((char*)(bptr)+WSIZE)

#define PRED(ptr) (*(char **)(ptr))
#define NEXT(ptr) (*(char **)(NEXT_PTR(ptr)))

#define FREEMAX 20

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
/*heap start point*/
static void* heap_list_p;


static void* freelist[FREEMAX+1];


static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void insert_free(void* ptr,size_t size);
static void delete_free(void *ptr);

static void* firstfit(size_t size);
static void partitioning(void* bp, size_t size);

static void partitioning(void* bp, size_t size){
    size_t b_size = GET_SIZE(HDRP(bp));
    delete_free(bp);

    if ((b_size-size)>=(2*DSIZE)){
        PUT(HDRP(bp),PACK(size,1));
        PUT(FTRP(bp),PACK(size,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(b_size-size,0));
        PUT(FTRP(bp),PACK(b_size-size,0));
        insert_free(bp,(b_size-size));
        coalesce(bp);
    }
    else{
        PUT(HDRP(bp),PACK(b_size,1));
        PUT(FTRP(bp),PACK(b_size,1));
    }
}






/*it returns first point of heap that are extended 
    so if it is null there is problem in extending
*/
static void* extend_heap(size_t words){
    char *bp;
    size_t size;

    size= (words%2)? (words+1)*WSIZE : words*WSIZE;
    /* size should be a*8 bytes*/
    if((long)(bp=mem_sbrk(size))==-1)
        return NULL;


    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(FTRP(bp)+WSIZE,PACK(0,1));
   
    insert_free(bp,size);
   
    return coalesce(bp);
}

/* when insert small block are put at front 
    size         freelist
    1               0
    2~             1
    4~             2
    8~              3
    16~             4
    32~             5     
    64~             6
    128~            7
    256~            8
    512~            9
because putting  small at first is beneficail 
*/
static void insert_free(void* ptr, size_t size){
     
    size_t a=size;
    size_t b=size;
    if(size==0||ptr==NULL){
        return;
    }
    size_t idx=0;
    void* search=NULL ;
    void* insert_ptr=NULL;

    while((idx<FREEMAX)&&(a>1)){
        a>>=1;
        ++idx;
    }
    
    search=freelist[idx];
        
    while ((search != NULL) && (size > GET_SIZE(HDRP(search)))){
        
        insert_ptr=search;
        search=NEXT(search);
    }
    if(search==NULL){
        /*search==null insert!=null*/
        if(insert_ptr!=NULL){
            SET_ADD(NEXT_PTR(insert_ptr),ptr);
            if(NEXT(insert_ptr)==insert_ptr){
              

            }
            SET_ADD(NEXT_PTR(ptr),NULL);
            
            SET_ADD(PRED_PTR(ptr),insert_ptr);
        }
        /*search==null insert==null*/
        else{
            SET_ADD(NEXT_PTR(ptr),NULL);
            SET_ADD(PRED_PTR(ptr),NULL);
            freelist[idx]=ptr;
        }

    }
    else {
        /*search!=null insert!=null*/
        if(insert_ptr!=NULL){
            SET_ADD(NEXT_PTR(ptr),search);
             if(NEXT(ptr)==ptr){
            }
            SET_ADD(NEXT_PTR(insert_ptr),ptr);
            if(NEXT(insert_ptr)==insert_ptr){
               
            }
            SET_ADD(PRED_PTR(search),ptr);
            SET_ADD(PRED_PTR(ptr),insert_ptr);
        }
        /*null->put->search?*/
        else{

            SET_ADD(NEXT_PTR(ptr),search);
            if(NEXT(ptr)==ptr){
                
            }
            SET_ADD(PRED_PTR(search),ptr);
            SET_ADD(PRED_PTR(ptr),NULL);
            freelist[idx]=ptr;
        }

    }
    return;
}
/*
delete is used for allocating 
just deleting node is not enough becuase there is case for 
    changing first freelist 
    need update free_list_i
*/
static void delete_free(void* ptr){
    int idx=0;
    if(ptr==NULL){
        return;
    }
    size_t size= GET_SIZE(HDRP(ptr));
    while((idx<FREEMAX)&&(size>1)){
        size>>=1;
        ++idx;
    }
    /*idx is freelist idx :  where this pointer located*/
    
    if(NEXT(ptr)!=NULL){
        /*Pre!=null next!=null*/
        if(PRED(ptr)!=NULL){
            SET_ADD(PRED_PTR(NEXT(ptr)),PRED(ptr));
            SET_ADD(NEXT_PTR(PRED(ptr)),NEXT(ptr));
            if(NEXT(PRED(ptr))==PRED(ptr)){
            }
        }
        /*Pre==null next!=null*/
        else{
            SET_ADD(PRED_PTR(NEXT(ptr)),NULL);
            freelist[idx]=NEXT(ptr);
           
        }
    }   /*Pre!=null next==null*/
    else{
        if(PRED(ptr)!=NULL){
            SET_ADD(NEXT_PTR(PRED(ptr)),NULL);
        }
        /*Pre==null next==null*/
        else{
            SET_ADD(NEXT_PTR(ptr),NULL);
            SET_ADD(PRED_PTR(ptr),NULL);
            freelist[idx]=NULL;
        }
    }
    return;
}

/*coalsece return bp of coalesced block*/
static void* coalesce(void* bptr){
   
    size_t prev_alloc= GET_ALLOC(FTRP(PREV_BLKP(bptr)));
    size_t next_alloc= GET_ALLOC(HDRP(NEXT_BLKP(bptr)));
    size_t size= GET_SIZE(HDRP(bptr));
    if(size==0){
        return NULL;
    }
    if(prev_alloc&&next_alloc){
        return bptr;
    }
    else if(prev_alloc&&!next_alloc){
        delete_free(bptr);
        delete_free(NEXT_BLKP(bptr));
        size+=GET_SIZE(HDRP(NEXT_BLKP(bptr)));
        PUT(HDRP(bptr),PACK(size,0));
        PUT(FTRP(bptr),PACK(size,0));
    }
    else if(!prev_alloc&&next_alloc){
        delete_free(bptr);
        delete_free(PREV_BLKP(bptr));
        size+=GET_SIZE(FTRP(PREV_BLKP(bptr)));
        PUT(HDRP(PREV_BLKP(bptr)),PACK(size,0));
        PUT(FTRP(PREV_BLKP(bptr)),PACK(size,0));
        bptr=PREV_BLKP(bptr);
    }
    else{
        delete_free(bptr);
        delete_free(NEXT_BLKP(bptr));
        delete_free(PREV_BLKP(bptr));
        size+=(GET_SIZE(HDRP(PREV_BLKP(bptr)))+GET_SIZE(HDRP(NEXT_BLKP(bptr))));
        PUT(HDRP(PREV_BLKP(bptr)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bptr)),PACK(size,0));
        bptr=PREV_BLKP(bptr);
    }
   

    insert_free(bptr,size);
    return bptr;
    
}

/*if init return 0 it is normal 
    if init return -1 it has problem
*/
int mm_init(void)
{   
    for(int i=0;i<=FREEMAX;i++){
       freelist[i]=NULL;
    }
    if((heap_list_p=mem_sbrk(4*WSIZE))==(void*)-1)
        return -1;
    PUT(heap_list_p,0);
    PUT(heap_list_p+(1*WSIZE),PACK(DSIZE,1));
    PUT(heap_list_p+(2*WSIZE),PACK(DSIZE,1));
    PUT(heap_list_p+(3*WSIZE),PACK(0,1));
    heap_list_p+=(2*WSIZE);

    size_t first_size=(1<<5);
    if(extend_heap(first_size)==NULL)
        return -1;
    return 0;
}

/*return memory address that fit firstly only just only find*/
static void* firstfit(size_t size){
    char* bptr=NULL;
    int idx=0;
    int a=size;
    char* tmp=NULL;
     while((idx<FREEMAX)&&(a>1)){
        a=a>>1;
        ++idx;
    }
   
    while(idx<=FREEMAX)
    {
        bptr=freelist[idx];
        while ((bptr != NULL) && ((size > GET_SIZE(HDRP(bptr)))))  
            {   
                bptr = NEXT(bptr);  
                
            }
        if (bptr != NULL)     
            return bptr;
        idx++;    
    }
    return NULL;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{  
    
    if(size==0){
        return NULL;
    }
    
    size_t real_size=0;
    size_t extend_size=0;
    char* bptr=NULL;
    if(size<=DSIZE){
        real_size=2*DSIZE;
    }
    else{
        real_size=ALIGN(DSIZE+size);
    }
     
    bptr=firstfit(real_size);
    /*find?*/
   
    if(bptr!=NULL){
      
        partitioning(bptr,real_size);
        /* hear you need to delete in freelist and change head and footer cutting and insertingalso*/
        
        return bptr;
    }
    
    extend_size=MAX(CHUNKSIZE,real_size);
    
    bptr=extend_heap(extend_size/WSIZE);
     
    if(bptr==NULL){
        
        return NULL;
    }
    else{
       
        partitioning(bptr,real_size);
        
        return bptr;
        
    }
}

/*
 */
void mm_free(void *ptr)
{  
    size_t size=GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    insert_free(ptr,size);
    coalesce(ptr);   
    

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    if(oldptr==NULL){
        newptr=mm_malloc(size);
        return newptr;
    }
    if(size==0){
        mm_free(oldptr);
        return NULL;
    }

    size_t cur_size= GET_SIZE(HDRP(ptr));
    size_t asize=MAX(ALIGN(size)+DSIZE, 2*DSIZE);
    void* bp=NULL;
    char * next_block= HDRP(NEXT_BLKP(ptr));
    char * prev_block= HDRP(PREV_BLKP(ptr));
    size_t prev_cur_size=GET_SIZE(prev_block)+cur_size;

    if(GET_ALLOC(next_block)==0){
        coalesce(NEXT_BLKP(ptr));
    }
    if(GET_ALLOC(prev_block)==0){
        coalesce(PREV_BLKP(ptr));
    }

    if(cur_size==asize){
        return ptr;
    }
    else if(asize< cur_size){
        if((cur_size-asize)>2*DSIZE){
          
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            bp = NEXT_BLKP(ptr);
            PUT(HDRP(bp), PACK(cur_size - asize, 1));
            PUT(FTRP(bp), PACK(cur_size - asize, 1));
            mm_free(bp);
            return ptr;
        }
        else{
            newptr = mm_malloc(size);
            copySize=GET_SIZE(HDRP(oldptr))-(2*WSIZE);
            if(size<copySize){
            copySize=size;
            }   
    
            memcpy(newptr, oldptr, copySize);
            mm_free(oldptr);
            return newptr;
        } 
            

    }
    else{
        
        if(GET_ALLOC(next_block)==0&&((GET_SIZE(next_block)+cur_size)-asize)>=2*DSIZE){
            delete_free(NEXT_BLKP(ptr));
            PUT(HDRP(ptr),PACK(asize,1));
            PUT(FTRP(ptr),PACK(asize,1));
            void* tmp=NEXT_BLKP(ptr);
            PUT(HDRP(tmp),PACK(GET_SIZE(next_block)+cur_size-asize,1));
            PUT(FTRP(tmp),PACK(GET_SIZE(next_block)+cur_size-asize,1));
            mm_free(tmp);
            return ptr;
        }
        else if(GET_ALLOC(prev_block)==0&&2*GET_SIZE(prev_block)>cur_size&&(prev_cur_size-asize)>=2*DSIZE){
            delete_free(PREV_BLKP(ptr));
            void* tmp=PREV_BLKP(ptr);

            copySize=cur_size-(2*WSIZE);
            memcpy(tmp, ptr, copySize);

            PUT(HDRP(tmp),PACK(asize,1));
            PUT(FTRP(tmp),PACK(asize,1));

            void* tmp2=NEXT_BLKP(tmp);
            PUT(HDRP(tmp2),PACK(prev_cur_size-asize,1));
            PUT(FTRP(tmp2),PACK(prev_cur_size-asize,1));
            mm_free(tmp2);
            return tmp;
        }
        else{
            newptr = mm_malloc(size);
            copySize=GET_SIZE(HDRP(oldptr))-(2*WSIZE);
            if(size<copySize){
            copySize=size;
            }   
    
            memcpy(newptr, oldptr, copySize);
            mm_free(oldptr);
            return newptr;

        }

    }

}











