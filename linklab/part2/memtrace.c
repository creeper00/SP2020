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

    LOG_STATISTICS(n_allocb, alloc_avg, n_freeb);

    int nonfreed_flag = 0;

    item* checklist = list->next;

    while (checklist != NULL) {
        if (checklist->cnt > 0) {
            if (nonfreed_flag == 0) {
                LOG_NONFREED_START();
                nonfreed_flag = 1;
            }
            LOG_BLOCK(checklist->ptr, checklist->size, checklist->cnt, checklist->fname, checklist->ofs);
        }
        checklist = checklist->next;
    }

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

    n_malloc++;
    n_allocb += size;

    alloc(list, res, size);

    return res;
}

void* realloc(void* ptr, size_t size) {
    void* res;
    char* error;
    int free_flag = 1;

    if (!reallocp) {
        reallocp = dlsym(RTLD_NEXT, "realloc");
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }
    }
    item* checker = find(list, ptr);

    if (checker == NULL) {
        res = reallocp(NULL, size);
        LOG_REALLOC(ptr, size, res);
        free_flag = 0;
    }
    else if (checker->cnt <= 0) {
        res = reallocp(NULL, size);
        LOG_REALLOC(ptr, size, res);
        free_flag = 0;
    }
    else {
        dealloc(list, ptr);
        res = reallocp(ptr, size);
        LOG_REALLOC(ptr, size, res);
    }

    n_realloc++;
    n_allocb += size;

    if (free_flag) n_freeb += checker->size;
    alloc(list, res, size);

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

    n_calloc++;
    n_allocb += nmemb * size;

    alloc(list, res, nmemb * size);

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

    LOG_FREE(ptr);

    item* dealloc_checker = dealloc(list, ptr);

    if (dealloc_checker != NULL) {
        freep(ptr);
        n_freeb += dealloc_checker->size;
    }
    

    return;
}
