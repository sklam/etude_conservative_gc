#include <stdlib.h>

void gc_begin(void);
void gc_end(void);

void gc_collect(void);
void *gc_malloc(size_t nbytes);

void gc_dump_stack_info(void);
void gc_dump_heap_info(void);
