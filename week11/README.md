# COMP SCI 537 Discussion Week 11

Today we will talk about p6. It's still a memory-related project, so the code walkthrough on week 9 is still helpful for this project. If you haven't watched week 9's video, I will recommend watching it first.

## Clock Algorithm

In p6, all pages are in one of three states:

- encrypted: `present:0, encrypted: 1, access: 0 `
- hot: `present: 1, encrypted: 0, access: 1`: the process is actively accessing this page
- cold: `present: 1, encrypted: 0, access: 0`

The possible state transitions include:

- When a page is created, it should be in the encrypted state.
- When an encrypted page is accessed, it turns to the hot state. This should trigger a page fault and the page fault handles this transition.
- When the clock hand moving over a hot page, it turns to the cold state.
- When the clock hand moving over a cold page, it gets evicted and turns to the encrypted state.
- When a cold page gets accessed, it turns hot. This has already been handled by the hardware MMU, so you don't need to do anything special.

Some tips:

- It is tricky to encrypt the page when it is allocated. It might be difficult to do it in functions like `allocuvm` because the kernel may access/modify these pages after calling `allocuvm`, but we cannot directly operate on encrypted data. These two functions (which are actually callers of `allocuvm`) might be better places to implement this:

    - `exec.c:exec`: it is the implementation for the syscall `exec`. `exec` will initialize memory and load data/code. Make sure you encrypt the pages after that.

    - `proc.c:growproc`: it is called only for growing user address space, typically when growing the heap by syscall `sbrk`.

- Since all the userspace pages are potential pages to encrypt, you may find some pages are always stay in the hot page states: e.g. stack. Don't feel weird if you see them in your clock queue.

## Ring Buffer

In p6, you will need to implement a clock algorithm to decide which page to encrypt/decrypt. Though it's doable to implement the clock algorithm by a linked list, it might be easier to implement using a ring buffer.

Taking example 2 from project spec. Suppose N = 4. Your clock queue (an array) and clock hand (defined as the next process to check) should look like below:

```
Format: (virtual page number, access bit)
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,0 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
    ^
clock_hand_idx = 0
```

**Event 1**: After virtual page 0x4 gets accessed, a page fault is triggered and set the access bit. There is no eviction happen so the clock hand is not moving

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,1 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
    ^
clock_hand_idx = 0
```

**Event 2**: Then virtual page 0x7 gets accessed, which is not in the working set (the clock queue) yet. Thus, we need to enqueue the virtual page 0x7 to the clock queue and evict a victim (and encrypt it) if the queue is full.

The clock hand first checks the virtual page 0x3 and clears its access bit.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,1 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
            ^
clock_hand_idx = 1
```

It keeps scanning and clear 0x4's access bit.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,0 | 0x5,0 | 0x6,1 |
+-------+-------+-------+-------+
                    ^
clock_hand_idx = 2
```

It still hasn't found the victim, so it keeps going. It checks 0x5's access bit, which is 0, so we choose to evict 0x5 and add 0x7.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,0 | 0x7,1 | 0x6,1 |
+-------+-------+-------+-------+
                            ^
clock_hand_idx = 3
```

Note when adding a new virtual page to the queue, the access bit is set to 1 and the hand moving "over" it (so the clock hand is not pointing to 0x7 but 0x6).

There can also be other corner cases. For example, if the clock queue is not full at all (which happens when the process just starts):

**Before:**

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 |       |       |       |
+-------+-------+-------+-------+
    ^
clock_hand_idx = 0
```

 **After:** page 0x4 gets accessed. Enqueue page 0x4. There is no eviction happens, so the clock hand doesn't move.

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,1 |       |       |
+-------+-------+-------+-------+
    ^
clock_hand_idx = 0
```

There can also be the case that all the pages having the access bit set, so you may need to scan through all the pages. Suppose we want to enqueue virtual page 0x7:

**Before:**

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,1 | 0x4,1 | 0x5,1 | 0x6,1 |
+-------+-------+-------+-------+
            ^
clock_hand_idx = 1
```

**After:**

First, it scans through all the pages and clears their access bits:

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x4,0 | 0x5,0 | 0x6,0 |
+-------+-------+-------+-------+
            ^
clock_hand_idx = 1
```

Then it checks 0x4 again, and finds its access bit is 0. Take it as the victim:

```
clock_queue:
+-------+-------+-------+-------+
| 0x3,0 | 0x7,1 | 0x5,0 | 0x6,0 |
+-------+-------+-------+-------+
                    ^
clock_hand_idx = 2
```

Other tricks on implementing ring buffer:

- How to iterate through the buffer in a "ring"-style: use mod (`%` in C):

    ```C
    while (1) {
        clock_queue_slot* hand = &clock_queue[clock_hand_idx];
        // do some work...
        if (some_check())
            break;
        clock_hand_idx = (clock_hand_idx + 1) % CLOCKSIZE;
    }
    ```
    Note `clock_queue_slot` type above is just for the example. Depends on your implementation, it could just be an integer VPN if you store access bit in PTE.
    
- The clock queue is a per-process state, not a global state, so you may want to add it to `struct proc`.

- You may also need to handle the case when a page is deallocated. It should be removed from the clock queue. 

- DO NOT put your ring buffer operation code everywhere... Wrap it into some functions and call these functions. This is critical for your code's debuggability and maintainability.

- In `fork`, you may need to copy the queue buffer and hand index. Depends on your implementation, you may use either shadow copy or deep copy if it contains some context-dependent pointer.

## Other tips

- Before starting, update Makefile first. set `CPUS` to 1 and `-O2` to `-O0` (not `-Og`!)
