# Dynamic Memory Allocation in C — Complete Deep Dive

---

## Table of Contents
1. [Why memory management is hard](#why-hard)
2. [The memory layout of a C program](#layout)
3. [The heap — where dynamic memory lives](#heap)
4. [malloc — allocate memory](#malloc)
5. [calloc — allocate and zero memory](#calloc)
6. [realloc — resize memory](#realloc)
7. [free — release memory](#free)
8. [How malloc actually works internally](#internals)
9. [Common bugs and how they happen](#bugs)
10. [Applying it to jit — the realloc refactor](#jit)

---

## 1. Why memory management is hard <a name="why-hard"></a>

You're right that the API itself is simple — malloc, realloc, free. Four
functions. That's it. So why do people say it's hard?

Because the bugs are **silent and invisible until they explode.**

With a syntax error the compiler catches it immediately. With a memory bug:
- The program compiles fine
- The program runs fine
- Then 10 minutes later it crashes somewhere completely unrelated
- Or it never crashes but silently produces wrong output
- Or it only crashes on someone else's machine
- Or it only crashes in production with real data

Memory bugs are the hardest category of bugs in all of programming because:
- They have no obvious symptoms at the point where the mistake was made
- The crash happens far away from the actual bug
- They're often non-deterministic — sometimes crash, sometimes don't
- Tools like valgrind exist specifically because these bugs are so hard to find manually

That's why people say it's hard. Not the API — the debugging.

---

## 2. The memory layout of a C program <a name="layout"></a>

When your C program runs, the operating system gives it a chunk of memory
divided into distinct regions. From bottom to top:

```
HIGH ADDRESS
┌─────────────────────┐
│        Stack        │  ← local variables, function calls
│          ↓          │  grows downward
├─────────────────────┤
│                     │
│     (free space)    │
│                     │
├─────────────────────┤
│          ↑          │  grows upward
│        Heap         │  ← malloc lives here
├─────────────────────┤
│        BSS          │  ← uninitialized global variables
├─────────────────────┤
│        Data         │  ← initialized global variables
├─────────────────────┤
│        Text         │  ← your compiled code
└─────────────────────┘
LOW ADDRESS
```

### The Stack
Where all your local variables live. When you write:
```c
char filename[1024];
int count = 0;
struct file_and_hash f;
```
These all go on the stack. The stack is managed automatically — memory is
allocated when a function is called and freed when it returns. Fast, simple,
automatic. But limited in size (usually 1-8MB) and the lifetime is tied to
the function — you can't return a pointer to a stack variable!

### The Heap
Where malloc lives. Much larger than the stack (limited only by available RAM).
Lifetime is controlled entirely by YOU — memory stays allocated until you
explicitly free it. This is both the power and the danger.

---

## 3. The heap — where dynamic memory lives <a name="heap"></a>

The heap is a large pool of memory that the OS gives to your program. The C
runtime manages this pool using a **heap allocator** — a piece of code that
keeps track of what parts of the heap are in use and what parts are free.

Think of the heap like a giant whiteboard. The heap allocator is the person
managing it:
- When you call `malloc(100)` they find 100 bytes of free space, mark it as
  used, and hand you the address
- When you call `free(ptr)` they mark that space as available again
- When you call `realloc(ptr, 200)` they try to grow your existing space or
  find a new bigger space and move your data there

The heap allocator maintains a data structure internally to track all of this.
The most common implementation uses something called a **free list** — a
linked list of free memory blocks.

---

## 4. malloc — allocate memory <a name="malloc"></a>

```c
void* malloc(size_t size);
```

Allocates `size` bytes of memory on the heap and returns a pointer to the
first byte. The memory is **uninitialized** — it contains whatever garbage
was there before.

### What malloc actually does step by step:

**Step 1 — Check the free list:**
The heap allocator looks through its list of free memory blocks for one
that is big enough to satisfy the request.

**Step 2 — If a free block is found:**
Split it if necessary (if the free block is 200 bytes and you asked for 100,
split it into a 100 byte used block and a 100 byte free block). Mark the
block as used. Return a pointer to it.

**Step 3 — If no free block is big enough:**
Ask the operating system for more memory using the `sbrk()` or `mmap()`
system call. The OS extends the heap. Now there's a new big free block.
Proceed as in step 2.

**Step 4 — Return the pointer:**
The pointer points to the first usable byte of your allocated block. Just
before this byte (hidden from you) the allocator stores metadata — the size
of the block, whether it's free or used. This metadata is how free() knows
how many bytes to release.

### What malloc returns:

```c
void* ptr = malloc(100);
```

`void*` means "pointer to anything" — it can be cast to any pointer type.
This is why you can do:

```c
char* str = malloc(100);
int* nums = malloc(sizeof(int) * 10);
struct file_and_hash* entries = malloc(sizeof(struct file_and_hash) * 30);
```

### When malloc fails:

If there's not enough memory (very rare on modern systems but possible),
malloc returns NULL. Always check:

```c
struct file_and_hash* entries = malloc(sizeof(struct file_and_hash) * 100);
if (entries == NULL) {
    printf("out of memory!\n");
    return;
}
```

### sizeof — how to calculate the right size:

Always use `sizeof` to calculate how many bytes to allocate:

```c
// for a single struct:
malloc(sizeof(struct file_and_hash))

// for an array of 30 structs:
malloc(sizeof(struct file_and_hash) * 30)

// for a string of length n:
malloc(strlen(str) + 1)   // +1 for null terminator
```

Never hardcode byte counts — sizeof handles platform differences
automatically.

---

## 5. calloc — allocate and zero memory <a name="calloc"></a>

```c
void* calloc(size_t count, size_t size);
```

Like malloc but takes two arguments — count and size — and **zeros out** all
the allocated memory. So `calloc(30, sizeof(struct file_and_hash))` allocates
space for 30 structs AND sets every byte to 0.

```c
// these are equivalent in terms of allocation size:
malloc(sizeof(struct file_and_hash) * 30)
calloc(30, sizeof(struct file_and_hash))

// but calloc also zeroes the memory — all fields start at 0/NULL/'\0'
```

### When to use calloc vs malloc:

Use calloc when you want guaranteed zero initialization — useful for strings
(already null terminated), counters (already 0), pointers (already NULL).

Use malloc when you're going to immediately overwrite the memory anyway —
no point zeroing if you're about to fill it with data via strcpy or fread.

---

## 6. realloc — resize memory <a name="realloc"></a>

```c
void* realloc(void* ptr, size_t new_size);
```

This is the interesting one. Takes an existing malloc'd pointer and a new
size, and returns a pointer to a resized block containing the original data.

### What realloc does internally:

**Case 1 — The block can grow in place:**
If there's free memory immediately after your current block, realloc just
extends it. Same pointer is returned. Super fast, no copying.

```
before: [your data: 100 bytes][FREE: 200 bytes]
after:  [your data: 200 bytes (extended)      ]
```

**Case 2 — The block cannot grow in place:**
There's something else right after your block. Realloc:
1. Finds a new free block somewhere else that's big enough
2. Copies all your original data to the new location
3. Frees the old block
4. Returns the new pointer

```
before: [your data: 100 bytes][someone else's data]...[FREE: 300 bytes]
after:  [your data: 100 bytes][someone else's data]...[your data: 200 bytes]
                                                        ↑ new location!
```

**This is why you MUST use the returned pointer:**
```c
// WRONG — ptr might have moved!
realloc(ptr, new_size);

// CORRECT — always capture the return value:
ptr = realloc(ptr, new_size);
```

If you ignore the return value and the block moved, `ptr` now points to freed
memory — instant disaster!

### The grow-as-needed pattern with realloc:

This is the pattern that makes truly unlimited arrays:

```c
int count = 0;
int capacity = 10;  // start with space for 10
struct file_and_hash* entries = malloc(sizeof(struct file_and_hash) * capacity);

while (/* more entries to read */) {
    if (count == capacity) {
        // full! double the capacity
        capacity *= 2;
        entries = realloc(entries, sizeof(struct file_and_hash) * capacity);
        if (entries == NULL) {
            printf("out of memory!\n");
            return;
        }
    }
    // add new entry at entries[count]
    count++;
}
```

**Why double the capacity?**
Doubling (instead of adding 1 each time) means you call realloc much less
often. If you add 1 each time and have 1000 entries, you called realloc 990
times, potentially copying data 990 times. If you double each time, you only
called realloc about 10 times (10 → 20 → 40 → 80 → 160 → 320 → 640 → 1280).
This is called **amortized O(1)** growth — the average cost per insertion is
constant even though occasional insertions trigger a copy.

This is exactly how C++ `std::vector`, Python lists, and JavaScript arrays
work internally!

### realloc with NULL:

A useful special case — if you pass NULL as the pointer, realloc behaves
exactly like malloc:
```c
realloc(NULL, 100)  // same as malloc(100)
```

This lets you write code that works whether or not you've allocated yet.

---

## 7. free — release memory <a name="free"></a>

```c
void free(void* ptr);
```

Tells the heap allocator that you're done with this memory. The allocator
marks the block as free and adds it back to the free list for future malloc
calls to use.

### What free does NOT do:

- It does NOT zero the memory — the old data is still there, just marked as
  available
- It does NOT set your pointer to NULL — ptr still holds the old address
- It does NOT immediately return memory to the OS — it goes back to the heap
  allocator's free list first

### The dangling pointer problem:

After free, the pointer still holds the old address but that memory is no
longer yours:

```c
char* str = malloc(100);
strcpy(str, "hello");
free(str);
// str still contains the old address!
printf("%s", str);  // UNDEFINED BEHAVIOUR — might work, might crash, might print garbage
str[0] = 'H';       // UNDEFINED BEHAVIOUR — writing to freed memory
```

The fix — set to NULL after freeing:
```c
free(str);
str = NULL;  // now any accidental use will crash immediately with a clear error
             // instead of silent corruption
```

### Double free — the other common bug:

Calling free twice on the same pointer:
```c
free(ptr);
free(ptr);  // CRASH — or silent heap corruption
```

The second free corrupts the heap allocator's internal data structures
because it tries to add an already-free block back to the free list, creating
an inconsistent state. This can cause completely unrelated malloc calls to
fail or behave incorrectly later.

### Memory leaks:

If you malloc and never free, the memory stays allocated for the entire
lifetime of the program:
```c
void some_function() {
    char* buffer = malloc(65536);
    // use buffer...
    // forgot to free!
    return;  // 64KB leaked every time this function is called!
}
```

For short programs this doesn't matter — the OS reclaims all memory when
the program exits. For long-running programs (servers, daemons) this is
catastrophic — memory usage grows until the system runs out.

---

## 8. How malloc actually works internally <a name="internals"></a>

This is where it gets really interesting. Most C standard libraries use a
variant of the following approach:

### The block header:

Every allocated block has a hidden header just before the pointer you receive:

```
what malloc returns to you:
ptr → [your data: N bytes]

what's actually in memory:
      [header: size, flags][your data: N bytes]
      ↑                     ↑
      hidden from you       ptr points here
```

The header typically stores:
- Size of the block
- Whether the block is free or in use
- Pointer to the next free block (when free)

This is why free() knows how many bytes to release even though you only pass
the pointer — it reads the header just before the pointer!

### The free list:

Free blocks are linked together in a linked list. Each free block uses some
of its own memory to store a pointer to the next free block:

```
[header|size=100|next→][free space 92 bytes] → [header|size=200|next→][free space 192 bytes] → NULL
```

When you call malloc(50):
1. Walk the free list looking for a block >= 50 bytes
2. Found the 100 byte block — split it into 50 bytes (used) + 50 bytes (free)
3. Return pointer to the 50 byte used block

When you call free(ptr):
1. Read the header just before ptr to get the block size
2. Mark block as free
3. Add to the front of the free list
4. Check if adjacent blocks are also free — if so merge them (coalescing)

### Coalescing — why it matters:

Without coalescing you get fragmentation:
```
[used 50][free 50][used 100][free 50][used 200][free 50]
```
Total free: 150 bytes but no single block is big enough for a 100 byte malloc!

With coalescing, adjacent free blocks are merged:
```
[used 50][free 150][used 200][free 50]  ← after coalescing
```
Now a 100 byte malloc can succeed.

### Size classes — the optimization:

Modern allocators like jemalloc and tcmalloc (used by Firefox and Chrome)
use size classes — separate free lists for different size ranges:
- tiny allocations (1-16 bytes) → one pool
- small allocations (16-256 bytes) → another pool
- medium allocations (256-4096 bytes) → another pool
- large allocations → handled separately

This dramatically reduces fragmentation and speeds up allocation because
you don't need to search for a best-fit block — just grab the next item
from the right size class.

---

## 9. Common bugs and how they happen <a name="bugs"></a>

### Buffer overflow:
```c
char* str = malloc(5);
strcpy(str, "hello world");  // 12 bytes into 5 byte buffer!
```
Writes past the end of the allocated block into adjacent memory. Can corrupt
the heap header of the next block, leading to crashes in completely unrelated
malloc/free calls later.

### Use after free:
```c
free(ptr);
printf("%s", ptr);  // reading freed memory
```
The freed memory might have been reallocated to something else. You're now
reading someone else's data. Or the allocator might have zeroed it.
Completely undefined behavior.

### Memory leak:
```c
ptr = malloc(100);
ptr = malloc(200);  // original 100 bytes lost forever!
```
Overwriting a pointer without freeing what it points to first. The original
allocation is now unreachable — leaked.

### Forgetting the null terminator:
```c
char* str = malloc(strlen(source));     // WRONG — no room for \0
char* str = malloc(strlen(source) + 1); // CORRECT
```
One of the most common bugs. strlen doesn't count the null terminator but
the string needs it.

### Not checking malloc return:
```c
int* arr = malloc(sizeof(int) * 1000000000);  // 4GB!
arr[0] = 5;  // crash if malloc returned NULL
```
On systems with limited memory malloc can fail. Always check for NULL.

---

## 10. Applying it to jit — the realloc refactor <a name="jit"></a>

Currently jit has arbitrary limits everywhere:
```c
struct file_and_hash index[30];      // max 30 files in index
struct file_and_hash tree[30];       // max 30 files in tree
char *filenames_in_index[1000];      // max 1000 files
char *filenames_in_dir[1000];        // max 1000 files in directory
struct file_and_hash entries[100];   // max 100 entries in add.c
```

### The refactor pattern for each:

**Before:**
```c
struct file_and_hash index[30];
int k = 0;
while (fgets(line, 1024, fptr) != NULL) {
    // fill index[k]
    k++;
}
```

**After:**
```c
int k = 0;
int capacity = 10;
struct file_and_hash* index = malloc(sizeof(struct file_and_hash) * capacity);

while (fgets(line, 1024, fptr) != NULL) {
    if (k == capacity) {
        capacity *= 2;
        index = realloc(index, sizeof(struct file_and_hash) * capacity);
    }
    // fill index[k]
    k++;
}
// ... use index ...
free(index);
```

### For pointer arrays like filenames_in_index:

**Before:**
```c
char *filenames_in_index[1000];
```

**After:**
```c
int capacity = 10;
char **filenames_in_index = malloc(sizeof(char*) * capacity);

// when adding:
if (index_count == capacity) {
    capacity *= 2;
    filenames_in_index = realloc(filenames_in_index, sizeof(char*) * capacity);
}
filenames_in_index[index_count] = malloc(strlen(filename) + 1);
strcpy(filenames_in_index[index_count], filename);
index_count++;
```

Note the double pointer `char**` — it's a pointer to an array of pointers.
The outer array is malloc'd (holds the pointers), and each individual string
is also malloc'd separately.

### The cleanup:

With dynamic allocation you now have two levels to free for pointer arrays:
```c
// free each string first
for (int i = 0; i < index_count; i++) {
    free(filenames_in_index[i]);
}
// then free the array itself
free(filenames_in_index);
```

For struct arrays just one free:
```c
free(index);
```

---

## Summary — the four functions

| Function | Purpose | Returns |
|----------|---------|---------|
| `malloc(size)` | allocate size bytes, uninitialized | pointer or NULL |
| `calloc(count, size)` | allocate count*size bytes, zeroed | pointer or NULL |
| `realloc(ptr, new_size)` | resize existing allocation | new pointer or NULL |
| `free(ptr)` | release allocation | void |

### The golden rules:
1. Every malloc must have exactly one free
2. Always use the return value of realloc
3. Never use memory after freeing it
4. Always check malloc/realloc return for NULL
5. Set pointer to NULL after freeing
6. Never free stack memory — only heap memory
7. Always malloc `strlen + 1` for strings

---

*Last updated: February 2026*