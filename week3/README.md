# COMP SCI 537 Discussion Week 3

## Xv6 Source Code Walk Through (cont)

### How xv6 starts

All the C program starts with `main`, including an operating system:

In `main.c`:

```C
// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  kvmalloc();      // kernel page table
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  uartinit();      // serial port
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk 
  startothers();   // start other processors
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  userinit();      // first user process
  mpmain();        // finish this processor's setup
}
```

It basically does some initialization work, including setting up data structures for page table, trap, and files, detecting other CPUs, creating the first process (init), etc. Sic mundus creatus est.

But what happens before `main()` gets called? Checkout `bootasm.S` and `bootmain.c` if interested.

Eventually, `main()` calls `mpmain()`, which then calls `scheduler()`:

```C
// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
}
```

The `scheduler()` is a forever-loop that keeps picking the next process to run (defined in `proc.c`):

```C
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
```

Spoiler Alert: You have spent quite a while with `scheduler()` in p4 :)

### From user-space to kernel-space

Let's come back to our p2 now. As we have talked about last week, the entry point of syscalls is written in `usys.S`:

```assembly
#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
# ...
```

It first declares a macro `SYSCALL`, which takes an argument `name` and expands it.

```assembly
#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret
```

For example, let `name` be `getpid`, the macro above will be expanded into

```assembly
.globl getpid
getpid:
    movl $SYS_getpid, %eax
    int $T_SYSCALL
    ret
```

Recall we have seen `SYS_getpid` defined in `syscall.h`:

```C
// ...
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
// ...
```

and `T_SYSCALL` is defined in `trap.h`:

```C
// ...
// These are arbitrarily chosen, but with care not to overlap
// processor defined exceptions or interrupt vectors.
#define T_SYSCALL       64      // system call
#define T_DEFAULT      500      // catchall
// ...
```

so it will be further expanded into

```assembly
.globl getpid
getpid:
    movl $11, %eax
    int $64
    ret
```

You could see how it gets expanded by `gcc -S usys.S`:

```console
$ gcc -S usys.S
// ...
.globl dup; dup: movl $10, %eax; int $64; ret
.globl getpid; getpid: movl $11, %eax; int $64; ret
.globl sbrk; sbrk: movl $12, %eax; int $64; ret
// ...
```

`.globl getpid` declares a label `getpid` for compiler/linker; when someone in the user-space executes `call getpid`, the linker knows which line it should jump to.

It then moves 11 into register `%eax` and issues an interrupt with an operand 64.

### Inside the kernel

The user-space just issues an interrupt with an operand 64. The CPU then starts to run the kernel's interrupt handler. It begins with (again) assembly :) In `trapasm.S`：

```assembly
  # vectors.S sends all traps here.
.globl alltraps
alltraps:
  # Build trap frame.
  pushl %ds
  pushl %es
  pushl %fs
  pushl %gs
  pushal
  
  # Set up data segments.
  movw $(SEG_KDATA<<3), %ax
  movw %ax, %ds
  movw %ax, %es

  # Call trap(tf), where tf=%esp
  pushl %esp
  call trap
  addl $4, %esp
```

It builds what we call a trap frame, which basically saves some registers into the stack and makes them into a data structure `trapframe`. 

`trapframe` is defined in `xv6.h`:

```C
struct trapframe {
  // registers as pushed by pusha
  uint edi;
  uint esi;
  uint ebp;
  uint oesp;      // useless & ignored
  uint ebx;
  uint edx;
  uint ecx;
  uint eax;

  // rest of trap frame
  ushort gs;
  ushort padding1;
  ushort fs;
  ushort padding2;
  ushort es;
  ushort padding3;
  ushort ds;
  ushort padding4;
  uint trapno;

  // below here defined by x86 hardware
  uint err;
  uint eip;
  ushort cs;
  ushort padding5;
  uint eflags;

  // below here only when crossing rings, such as from user to kernel
  uint esp;
  ushort ss;
  ushort padding6;
};
```

`alltraps` eventually executes `call trap`. The function `trap()` is defined in `trap.c`:

```C
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }
  // ...
}
```

The 64 we just saw is `tf->trapno` here. `trap()` checks whether `trapno` is `T_SYSCALL`, if yes, it calls `syscall()` to handle it.

If you are interested in how the operand 64 ends up in `trapno` and how `alltraps` gets called, you could take a look at `vectors.S`, which is generated by `vectors.pl`

### Inside `syscall()`

`syscall()` is defined in `syscall.c`:

```C
void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
```

It gets the syscall number from saved register `%eax`, and uses this number to index the function pointer array `syscalls`.

For example, if `num` is `SYS_getpid`, then `syscalls[num]` gives you the address of function `sys_getpid`. `syscalls[num]()` will then become `sys_getpid()`. The return value is saved into the register `%eax`. This is an xv6 function calling convention that the return value is passed by registers.

Notice that, all `sys_xxx()` function takes no argument, but many syscalls do take arguments. Let use `kill` as the example. `syscalls[num]()` will become `sys_kill`, which is defined `sysproc.h`.

```C
int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}
```

This function is essentially a wrapper that pops the argument and passes it to the real implementation `kill` in the kernel. `argint` is defined in `syscall.c`:

```C
// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
  struct proc *curproc = myproc();

  if(addr >= curproc->sz || addr+4 > curproc->sz)
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}
```

What it does is, first figure out what is the saved stack pointer (`%esp`), then find the arguments that were pushed to the stack previously. This "pushing arguments to the stack" is the function arguments passing conventions in xv6. This is done by the compilers. Not only syscalls do this, but all the function calls follow this convention.
