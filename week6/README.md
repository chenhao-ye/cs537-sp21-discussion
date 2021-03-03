# COMP SCI 537 Discussion Week 6

Agenda: This week we will mainly focus on some basic xv6 scheduling code. Next week we may discuss the more complicated sleep/wakeup mechanism.

Before we start, here are some tips:

- Make sure you set Makefile correct (e.g. `CPUS := 1`, `-Og`) before starting this project.
- Find a partner... 

## `scheduler()`: Scheduling Decision Making

Recall in [week 3](https://github.com/chenhao-ye/cs537-sp21-discussion/tree/main/week3#how-xv6-starts), we have seen how xv6 starts and eventually enters `scheduler()`. We now take a closer look at `scheduler()`.

```C
//PAGEBREAK: 42
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

The most interesting piece is what inside the while-loop:

```C
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
```

What it does is scanning through the ptable and find a `RUNNABLE` process `p`, then

1. set current CPU's running process to `p`: `c->proc = p`
2. switch userspace page table to `p`: `switchuvm(p)`
3. set `p` to `RUNNING`: `p->state = RUNNING`
4. `swtch(&(c->scheduler), p->context)`: This is the most tricky and magic piece. What it does is saving the current registers into `c->scheduler`, and loading the saved registers from `p->context`. After executing this line, the CPU is actually starting to executing `p`'s code (strictly speaking, it's in `p`'s kernel space, not jumping back to `p`'s userspace yet), and the next line `switchkvm()` will not be executed until later this process traps back to kernel again.

Both `c->scheduler` and `p->context` are of type `struct context`. `c` is of type `struct cpu`.

```C
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

// Per-process state
struct proc {
  // ...
  int pid;                     // Process ID
  //...
  struct context *context;     // swtch() here to run process
  // ...
};
```

[QUIZ] Before we finishing this section, consider this question: what is the current xv6's scheduling policy?

## `sched()`: From User Process to Scheduler

As you see in the scheduler, when a scheduler decision is made, `swtch(&(c->scheduler), p->context)` will switch to that process. Then we will see how a process switches into the scheduler.

```C
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}
```

Skip some uninteresting sanity checking, the major work is really just one single line: `swtch(&p->context, mycpu()->scheduler)`.

It will also be interesting to see who will call `sched()`. Try it yourself by searching inside the xv6 codebase.

```C
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  // ... (some cleanup)
  sched();
  panic("zombie exit");
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  //... (prepare to sleep)
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();
  //... (after wakeup)
}
```

It's unsurprising to see `sched()` gets called in these three cases... `exit()` is expected. `sleep()` here is a bad name... It actually means "blocked" or "wait". Any process that is waiting for some events (e.g. IO) will call this. When handling compensation ticks, we will play more with `sleep()` (in next week's discussion). `yield()` here is another bad name... xv6 doesn't have a syscall named `yield()`. You should search in the codebase again to see who calls `yield()`.

## Timer Interrupt

Scheduling will be less useful without considering timer interrupt. As you have seen in p2, all the interrupts are handled in `trap.c: trap()`.

```C
struct spinlock tickslock;
uint ticks;

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
  // something you have already seen in p2
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  // other cases...
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
```

There are two interesting things going on here:

1. If it is a timer interrupt `case T_IRQ0 + IRQ_TIMER`: the global variable `ticks` gets incremented
2. If it is a timer interrupt satisfying `myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER`: call `yield()`, which then relinquish CPU and gets into scheduler

## Tips for p4

1. Read spec carefully, especially the "Tips" section.
2. Make sure you understand every question in the warm-up quiz. The warm-up quiz is really more hints than a quiz...
3. Find a partner...
4. Be sure to come to next week's discussion section...