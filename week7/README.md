# COMP SCI 537 Discussion Week 7

Let's start this week's discussion with a quiz.

To wait for a condition variable (using pthread API for example):

```C
int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex)
```

Why we need a mutex here? Maybe combine with the example below.

```C
// Suppose they are all initialized properly
int shared_data = 0;
pthread_mutex_t mutex;
pthread_cond_t cv;

// Consider: what if we don't have mutex in [1-6]
void procA() {                           //     => void procA() {
    pthread_mutex_lock(&mutex);          // [1] =>
    while (shard_data == 1) {            //     =>     while (shard_data == 1) {
        pthread_cond_wait(&cv, &mutex);  // [2] =>         pthread_cond_wait(&cv);
    }                                    //     =>     }
    pthread_mutex_unlock(&mutex);        // [3] =>
};                                       //     => }

void procB() {                           //     => void procB() {
    pthread_mutex_lock(&mutex);          // [4] =>
    shard_data = 1;                      //     =>    shard_data = 1;
    pthread_mutex_unlock(&mutex);        // [5] =>
};                                       //     => };
```

[Watch discussion video for the solution]



## `sleep()`: is not really just sleeping

The `proc.c:sleep()` function in xv6 is a terrible name... What it does is more like "waiting" instead of sleeping.

```C
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}
```

What does this do? `sleep()` works just like `pthread_cond_wait`: it requires the function caller to first acquire a lock (first argument `lk`) that guards a shared data structure `chan` (the second argument); then `sleep()` puts the current process into `SLEEPING` state and *then* release the lock. 

What does this mean (what is the semantics of this function)? "Channel" here is a waiting mechanism which could be any address. When process A updates a data structure and expects that some other processes could be waiting on a change of this data structure, A can scan and check other SLEEPING processes' `chan; `if process B's `chan` holds the address of the data structure process A just updated, then A would wake B up.

It might be helpful to read `sleep()` with its duality function `wakeup()`:

```C
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}
```

Now let's take a closer look of `sleep()`:

```C
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  // [skip some checking]...

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // [Mark as sleeping and then call sched()]
  // [At some point this process gets waken up and resume]

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}
```

You may notice it has a conditional checking to see if `lk` refers to `ptable.lock`. Why? Pause here for a second to think about it. Hint: we are going to call `sched()` in the middle.

[Spoiler Alert!]

The reason is, `sched()` requires `ptable.lock` gets held. So here we are actually dealing with two locks: `lk` held by caller and `ptable.lock` that `sleep()` should hold. In a boundary case, these two locks are the same, which acquire special handling and that is why see the condition checking above.

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
    // ...
}
```

Remember this condition checking... It will save you from a lot of kernel panic I promise...

## `sys_sleep()`: A Use Case of `sleep()`

After understanding `sleep()`, we are now able to understand how `sleep()` works.

```C
// In sysproc.c
int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
```

Here the global variable `ticks` is used as the wait channel (recall global variable `ticks` is defined in `trap.c`), and the corresponding guard lock is `ticklock`.

```C
// In trap.c
struct spinlock tickslock;
uint ticks;

void
trap(struct trapframe *tf)
{
  // ... 
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
  // ...
  }
  // ...
}
```

So what really happens is: a process that calls **`sleep()` syscall** will be wakened up every time that the global variable `ticks` changes. It then replies on the while loop in `sys_sleep()` to ensure it has slept long enough.

```C
  while(ticks - ticks0 < n){
    // ...
    sleep(&ticks, &tickslock);
  }
```

This is not really a smart implementation of `sleep` syscall, because this process will jump between `RUNNABLE`, `RUNNING`, and `SLEEPING` back and forth until it sleeps long enough, which is inefficient and will mess up our compensation mechanism.

To make it more efficient, you need to make `wakeup1()` smarter and treat a process that waits on `ticks` differently to avoid falsely wake up.

For example, one implementation could be:

```C
// In sysproc.c
int
sys_sleep(void)
{
  // ...
  acquire(&tickslock);
  ticks0 = ticks;

  // Before sleep, mark this process's some fields to denote when this process should wake up
  MARK_SOME_FIELDS_OF_PROC();
  sleep(&ticks, &tickslock);
 
  release(&tickslock);
}

// In proc.c
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {
      // Add more checking to see are we really going to wake this process up
      if (chan == &ticks) { // this process waits on ticks... it could be put to sleep by sys_sleep
         MORE_CHECKING();
      } else {
         p->state = RUNNABLE;
      }
    }
}
```

## One More (Important) Hint

Whenever you need to access the global variable `ticks`, think twice whether the `ticklock` has already be held or not. As you may see in the quiz, a process who tries to acquire the lock that it has already acquired will cause a kernel panic.

For example, suppose you want to access `ticks` in `sleep()`. Instead of

```C
sleep(void *chan, struct spinlock *lk)
{
  // ...
  uint now_ticks;
  acquire(&tickslock);
  now_ticks = ticks;
  release(&tickslock);
  // ...
}
```

Make sure you don't double-acquire `ticklock`:

```C
sleep(void *chan, struct spinlock *lk)
{
  // ...
  if (lk == &tickslock) { // already hold
    now_ticks = ticks;
  } else {
    acquire(&tickslock);
    now_ticks = ticks;
    release(&tickslock);
  }
  // ...
    
  // same for release...
}
```

Keep this in mind. It will save you from a huge amount of kernel panic...