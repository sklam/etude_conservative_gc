#include <stdio.h>

#include "gc.h"


// use noinline attr to avoid variables in work() out live the caller function
void work()   __attribute__((noinline));

int main(){
    gc_dump_stack_info();
    gc_begin();
    work();
    gc_end();
    gc_dump_heap_info();
    return 0;
}

////// details


void* foo() {
    gc_dump_stack_info();
    void** a = gc_malloc(sizeof(void*) * 4);
    void* b = gc_malloc(sizeof(int) + 1);
    void* c = gc_malloc(sizeof(int) + 2);
    void* d = gc_malloc(sizeof(int) + 3);
    void* e = gc_malloc(sizeof(int) + 4);
    void* f = gc_malloc(sizeof(int) + 5);

    printf("%s a @%p = %p\n", __func__, &a, a);

    gc_collect();

    // stores pointers into interior memory of `a`
    a[0] = b;
    a[1] = c;
    a[2] = d;
    a[3] = e;

    printf("%s f @%p = %p\n", __func__, &f, f);

    return a;
}

void work(){
    void ** a = foo();
    gc_dump_stack_info();
    // printf("%s a @%p = %p\n", __func__, &a, a);
    gc_collect();
    printf("a = %p [0] = %p\n", a, a[0]);
}

