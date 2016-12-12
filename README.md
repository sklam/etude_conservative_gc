# An Etude for a Conservative Garbage Collector

Implementing an overly simple mark-sweep garbage collector (GC) for C.

The GC is implemented as a C library that allows memory allocations
in a specific code region to be managed and automatically freed.
A GC'ed region is marked by a call to ``gc_begin()`` and ends with
a call to ``gc_end()``.  All managed allocations within the GC'ed region
must use ``gc_malloc()`` instead of ``malloc()``.  A call to ``gc_collect()``
ask the GC to free all unused allocations.

## How It Works

### The Stack

The system stack is a contigous block of memory allocated by the system to store
temporary data to support the program execution.
A stack-pointer keeps track of the top of the stack.
Data are added or removed from the top of the stack.

This GC implementation only looks at the stack for life variables.
C uses the system stack to store *some* local variables and *some* call
arguments and return values.
We highlighted "some" because not all variables are located in the stack.
For speed, C keeps actively used variables in machine registers and will
only put them in the stack when it runs out of register space.
The same is true for call arguments and return values.
One can write assembly code to force all variables into the stack by manually
saving each registers.  But for our GC, we simply call ``setjmp()`` to store
all the register values into the stack.  This trick seems to work for now given
that the ``jmp_buf`` is just a C struct that contains a field for each registers
and probably a bit more, but this relies on OS implementation detail.
The alternative is to handwrite some assembly code to store all the registers.

### The Heap

The heap is where dynamic allocated memory are located.
The purpose of the GC is to free up unused allocation in the heap.
Textbooks like simplify things and just say that the heap starts from the
opposite end of the memory space from the stack and grows towards the stack.
In practice, it really depends on the OS and the language.
We will just use it as a black-box and add my own structure on top of
the whatever ``malloc()`` implementation we am given.  The ``gc_malloc`` will
allocate extra header space to store the following extra information:

* a pointer to link to the previous allocation to form a singlely-linked list
* an integer to store the size of the allocation.

**note:** In practice, implementing an efficient heap is the hardest part for
implementating a GC'ed language.

### The Globals

Global variables (or statically allocated variables) are located in the BSS
segment.  For simplicity, they are ignored.


### The Mark Phase

The *mark* phase scans the stack for any life objects, which are represented
as pointers on the stack.
In the machine level, pointers are just numeric address and is no different
from other integer of the same bit size.
On a 64-bit arch, we look for 64-bit sized and aligned numbers of the stack
and match it against any known allocation inside our heap.
Since any value that resembles a valid heap pointer is marked, this GC is
*conservative*.  (A GC that knows exactly what is a heap pointer is *accurate*.)
We implement an naive approach that will scan each the heap (using our
linked-list) for each possible pointer on the stack.

Recall that this GC is only enabled between ``gc_begin()`` and ``gc_end()``.
We will only stack the stack address range between the current stack location
and the point when ``gc_begin()`` was called.  To read the stack pointer
register, we use the following inline assembly:

```C
    void* sp;
    asm ("movq %%rsp, %0" : "=r" (sp) );
```

Inside ``gc_begin()``, we need to know the stack location before the call
to the function but not the stack location inside the function.
To do that, we need to know that the *base pointer* for AMD64 points to the
stack memory when the previous stack-pointer is preserved.
Reading from the base pointer (RBP) will get what we need.
For other architecture, check the platform ABI.

During the stack scan, every value that matches a heap allocation is marked.
By marking, we set a bit in the linked-list pointer in the header of the
corresponding allocation.
This is a trick to keep the overhead low by using the lowest bit of the pointer
value to store our *mark* bit.
This is possible because the the ``malloc`` implementation will always return
memory on even address.  (This should be true on all architectures.)

Once the stack is scanned, we repeat the process on all interior memory of each
marked heap allocation for reference to other heap allocations.

### The Sweep Phase

The sweep phase is relatively simple.  We will loop through our heap allocations
(using the heap linked-list) and free all unmarked heap allocations.


### Compiling & Running The Code

A Makefile is provided.

The garbage collector is in the "gc.c" and "gc.h" files.

The "test.c" file is the test.  Run it with valgrind to verify that the GC
worked.


## Final notes

The GC presented here is not a complete nor fast implementation.
This solely serves as a simplified study material for those interested
in how a GC works.

The code is only tested on AMD64 architecture.

## References

* http://web.engr.illinois.edu/~maplant2/gc.html
* https://www.hboehm.info/gc/


