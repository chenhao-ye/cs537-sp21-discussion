# COMP SCI 537 Discussion Week 11

Today we will talk about p6. It's still a memory-related project, so the code walkthrough on week 9 is still helpful for this project. If you haven't watched week 9's video, I will recommend watching it first.

## Clock Algorithm

In p6, all pages are in one of three states:

- encrypted: `present:0, encrypted: 1, reference: 0 `
- hot: `present: 1, encrypted: 0, reference: 1`: the process is actively accessing this page
- cold: `present: 0, encrypted: 0, reference: 0`: present bit must be clear to force MMU to trigger page fault so that we could set reference bit.

The possible state transitions include:

- When a page is created, it should be in the encrypted state.
- When an encrypted page is accessed, it turns to the hot state.
- When the clock head moving over a hot page, it turns to the cold state.
- When the clock head moving over a cold page, it gets evicted and turns to the encrypted state.
- When a cold page has a page fault triggered, it means it is just accessed, so it turns hot.

Some tips:

- Since the cold page will have the exactly same three bits as an invalid page, you can't tell the difference just by these three bits. You may introduce another valid bit.

- It might be tricky to encrypt the page when it is allocated. It might be difficult to do it in functions like `allocuvm` because the kernel may access these pages after calling `allocuvm`, but we don't want it to be decrypted immediately. These two functions (which are actually callers of `allocuvm`) might be better places to implement this:

    - `exec.c:exec`: it is the implementation for the syscall `exec`. `exec` will initialize memory and load data/code. Make sure you encrypt the pages after that.

    - `proc.c:growproc`: it is called only for growing user address space, typically when growing the heap by syscall `sbrk`.

- Since all the userspace pages are potential pages to encrypt, you may find some pages are always stay in the hot page states: e.g. stack. Don't feel weird if you see them in your clock queue.

## Ring Buffer

In p6, you will need to implement a clock algorithm to decide which page to encrypt/decrypt. Though it's doable to implement the clock algorithm by a linked list, it might be easier to implement using a ring buffer.

Taking example 2 from project spec. Suppose N = 4. Your clock queue (an array) and clock head (defined as the next process to check) should look like below:

```
Format: (virtual page number, reference bit)
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,0 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
    ^
clock_head_idx = 0
```

**Event 1**: After virtual page 0x4 gets accessed, a page fault is triggered and set the reference bit. There is no eviction happen so the clock head is not moving

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,1 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
    ^
clock_head_idx = 0
```

**Event 2**: Then virtual page 0x7 gets accessed, which is not in the working set (the clock queue) yet. Thus, we need to enqueue the virtual page 0x7 to the clock queue and evict a victim (and encrypt it) if the queue is full.

The clock head first checks the virtual page 0x3 and clears its reference bit.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,1 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
            ^
clock_head_idx = 1
```

It keeps scanning and clear 0x4's reference bit.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,0 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
                    ^
clock_head_idx = 2
```

It still hasn't found the victim, so it keeps going. It checks 0x5's reference bit, which is 0, so we choose to evict 0x5 and add 0x7.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,0 | 0x7,1 | 0x6,1 |
+-------+-------+-------+-------+
                            ^
clock_head_idx = 3
```

Note when adding a new virtual page to the queue, the reference bit is set to 1 and the head moving "over" it (so the clock head is not pointing to 0x7 but 0x6).

There can also be other corner cases. For example, if the clock queue is not full at all (which happens when the process just starts):

**Before:**

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 |       |       |       |
+-------+-------+-------+-------+
    ^
clock_head_idx = 0
```

 **After:** page 0x4 gets accessed. Enqueue page 0x4. There is no eviction happens, so the clock head doesn't move.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,1 |       |       |
+-------+-------+-------+-------+
    ^
clock_head_idx = 0
```

There can also be the case that all the pages having the reference bit set, so you may need to scan through all the pages. Suppose we want to enqueue virtual page 0x7:

**Before:**

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,1 | 0x5,1 | 0x6,1 |
+-------+-------+-------+-------+
            ^
clock_head_idx = 1
```

**After:**

First, it scans through all the pages and clears their reference bits:

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,0 | 0x5,0 | 0x6,0 |
+-------+-------+-------+-------+
            ^
clock_head_idx = 1
```

Then it checks 0x4 again, and finds its reference bit is 0. Take it as the victim:

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x7,1 | 0x5,0 | 0x6,0 |
+-------+-------+-------+-------+
                    ^
clock_head_idx = 2
```

Other tricks on implementing ring buffer:

- How to iterate through the buffer in a "ring"-style: use mod (`%` in C):

    ```C
    while (1) {
        clock_queue_slot* head = &clock_queue[clock_head_idx];
        // do some work...
        if (some_check())
            break;
        clock_head_idx = (clock_head_idx + 1) % CLOCKSIZE;
    }
    ```
    Note `clock_queue_slot` type above is just for the example. Depends on your implementation, it could just be an integer VPN if you store reference bit in PTE.
    
- The clock queue is a per-process state, not a global state, so you may want to add it to `struct proc`.

- You may also need to handle the case when a page is deallocated. It should be removed from the clock queue. 

- DO NOT put your ring buffer operation code everywhere... Wrap it into some functions and call these functions. This is critical for your code's debuggability and maintainability.

- In `fork`, you may need to copy the queue buffer and head index. Depends on your implementation, you may use either shadow copy or deep copy if it contains some context-dependent pointer.

## Other tips

- Before starting, update Makefile first. set `CPUS` to 1 and `-O2` to `-O0` (not `-Og`!)
