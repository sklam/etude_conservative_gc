#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <assert.h>
#include <setjmp.h>

#include "gc.h"

typedef intptr_t word_t;

// singly-linked list
typedef struct GC_AllocHeader_ GC_AllocHeader;
struct GC_AllocHeader_{
    GC_AllocHeader *next;
    size_t          size;
};

typedef struct GC_Context_ {
    void *stack_base;
    GC_AllocHeader use_list;
    void *heap_begin, *heap_end;
} GC_Context;

static int the_gc_init_flag = 0;
GC_Context the_gc_ctx;


#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define ptrtoint(x) ((intptr_t)(x))
#define inttoptr(x, T) ((T)(x))
#define ptroffset(p, s) (void*)((char*)(p) + (s))

#define TAG(x)  (x = inttoptr(ptrtoint(x)|1, GC_AllocHeader*));
#define UNTAG(x) inttoptr((ptrtoint(x) >> 1) << 1, GC_AllocHeader*)
#define IS_TAGGED(x) ((ptrtoint(x) & 1) == 1)

void gc_list_insert_after(GC_AllocHeader *base, GC_AllocHeader *node){
    node->next = UNTAG(base->next);
    base->next = node;
}

GC_AllocHeader* gc_list_delete_after(GC_AllocHeader *base) {
    GC_AllocHeader *ret = base->next;
    base->next = UNTAG(ret->next);
    return ret;
}


void* gc_read_sp(void) {
    void* sp;
    asm ("movq %%rsp, %0" : "=r" (sp) );
    return sp;
}

#define READ_BASE_PTR(PTR) asm ("movq %%rbp, %0" : "=r" (PTR) );

void gc_dump_stack_info(void) {
    printf("stack base=%p top=%p\n", the_gc_ctx.stack_base, gc_read_sp());
}

void gc_dump_heap_info(void) {
    GC_AllocHeader *hp = the_gc_ctx.use_list.next;
    size_t ct = 0;

    for(; hp; hp = hp->next, ++ct) {
        printf(" [%zu] alloc %p %zu\n", ct, hp, hp->size);
    }

    if (ct == 0) puts("heap is empty");
}

void* gc_stack_next(void *stack) {
    // XXX assume stack grows down
    word_t *p = stack;
    return p - 1;
}

void* gc_stack_prev(void *stack) {
    // XXX assume stack grows down
    word_t *p = stack;
    return p + 1;
}

void gc_init(void){
    if (the_gc_init_flag)
    memset(&the_gc_ctx, 0, sizeof(the_gc_ctx));
    the_gc_ctx.heap_begin = (void*)-1;
    // set flag
    the_gc_init_flag = 1;
}

void gc_begin(void) {
    void *base_ptr, *parent_base;
    puts("gc_begin");
    READ_BASE_PTR(base_ptr);
    parent_base = *(void**)base_ptr; // load parent base ptr
    assert(parent_base > base_ptr && "Parent frame is not in higher addr");
    assert(parent_base < base_ptr + 0xff && "Parent frame too faraway");
    gc_init();
    the_gc_ctx.stack_base = parent_base;
    gc_dump_stack_info();
}

void* gc_header_payload(GC_AllocHeader *header) {
    return ptroffset(header, sizeof(GC_AllocHeader));
}

void gc_scan_heap(void *ptr) {
    // loop thru the use_list
    GC_AllocHeader *hp = the_gc_ctx.use_list.next;

    if (the_gc_ctx.heap_begin > ptr || ptr > the_gc_ctx.heap_end) {
        // pointer out of range
        return;
    }

    for (; hp; hp = UNTAG(hp->next) ) {
        if (!IS_TAGGED(hp->next)) {
            void *payload = gc_header_payload(hp);
            if ( ptr == payload ) {
                // mark
                TAG(hp->next);
                printf("marked header=%p ptr=%p\n", hp, ptr);
                // scan interior
                void **in_end = (void**)ptroffset(ptr, hp->size);
                for(void **in_ptr = ptr; in_ptr < in_end; ++in_ptr) {
                    gc_scan_heap(*in_ptr);
                }
            }
        }
    }
}

void gc_mark(void **begin, void **end) {
    printf("mark begin=%p end=%p\n", begin, end);
    // walk the stack
    for ( void **ptr = begin; ptr != end; ptr = gc_stack_next(ptr) ) {
        gc_scan_heap(*ptr);
    }
}

void gc_sweep() {
    GC_AllocHeader *last = &the_gc_ctx.use_list;
    GC_AllocHeader *hp = the_gc_ctx.use_list.next;
    while(hp) {
        if (IS_TAGGED(hp->next)) {
            // untag
            hp->next = UNTAG(hp->next);
            // advance
            last = hp;
            hp = hp->next;
        } else {
            // unlink
            void *cur = gc_list_delete_after(last);
            // advance; no need to set last
            hp = last->next;
            // deallocate
            printf("free header=%p ptr=%p\n", cur, gc_header_payload(cur));
            free(cur);
        }
    }
}

void gc_collect(void) {
    puts("=== gc collect ===");
    // TODO: is the following setjmp necessary
    // force registers to stack
    jmp_buf jb;
    setjmp(jb);
    // Mark
    void *sp_begin = the_gc_ctx.stack_base;
    void *sp_end = gc_read_sp();

    gc_mark(sp_begin, sp_end);

    // Sweep
    gc_sweep();
}

void gc_end(void) {
    puts("gc_end");
    gc_collect();
}

void *gc_malloc(size_t nbytes) {
    size_t allocsize = sizeof(GC_AllocHeader) + nbytes;
    GC_AllocHeader *header = malloc(allocsize);
    void *ptr = ptroffset(header, sizeof(GC_AllocHeader));
    memset(header, 0, sizeof(GC_AllocHeader));
    header->size = nbytes;
    gc_list_insert_after(&the_gc_ctx.use_list, header);
    printf("malloc header=%p ptr=%p size=%zu\n", header, ptr, nbytes);

    the_gc_ctx.heap_begin = MIN(the_gc_ctx.heap_begin, (void*)header);
    the_gc_ctx.heap_end = MAX(the_gc_ctx.heap_end, ptroffset(header, allocsize));
    return ptr;
}
