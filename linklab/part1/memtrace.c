//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>
#include "callinfo.h"

//
// function pointers to stdlib's memory management functions
//
static void* (*mallocp)(size_t size) = NULL;
static void (*freep)(void* ptr) = NULL;
static void* (*callocp)(size_t nmemb, size_t size);
static void* (*reallocp)(void* ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc = 0;
static unsigned long n_calloc = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb = 0;
static unsigned long n_freeb = 0;
static item* list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
    char* error;

    LOG_START();

    // initialize a new list to keep track of all memory (de-)allocations
    // (not needed for part 1)
    list = new_list();

    // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
    // ...
    int alloc_avg = 0;
    if (n_allocb == 0) alloc_avg = 0;
    else alloc_avg = n_allocb / (n_malloc + n_calloc + n_realloc);

    LOG_STATISTICS(n_allocb, alloc_avg, 0L);

    LOG_STOP();

    // free list (not needed for part 1)
    free_list(list);
}

// ...

void* malloc(size_t size) {
    void* res;
    char* error;

    if (!mallocp) {
        mallocp = dlsym(RTLD_NEXT, "malloc");
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }
    }

    res = mallocp(size);
    LOG_MALLOC(size, res);

    n_malloc ++;
    n_allocb += size;

    return res;
}

void* realloc(void* ptr, size_t size) {
    void* res;
    char* error;

    if (!reallocp) {
        reallocp = dlsym(RTLD_NEXT, "realloc");
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }
    }

    res = reallocp(ptr, size);
    LOG_REALLOC(ptr, size, res);

    n_realloc ++;
    n_allocb += size;

    return res;
}

void* calloc(size_t nmemb, size_t size) {
    void* res;
    char* error;

    if (!callocp) {
        callocp = dlsym(RTLD_NEXT, "calloc");
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }   
    }

    res = callocp(nmemb, size);
    LOG_CALLOC(nmemb, size, res);

    n_calloc ++;
    n_allocb += nmemb*size;

    return res;
}

void free(void* ptr) {
    char* error;

    if (!freep) {
        freep = dlsym(RTLD_NEXT, "free");
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }
    }
    freep(ptr);

    LOG_FREE(ptr);

    return;
}
