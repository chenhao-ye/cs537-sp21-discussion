# COMP SCI 537 Discussion Week 13

Today we will talk about p7. It will be relatively easier. As a reminder, this is due on Apr 29th (next Thursday), and the warm-up quiz is due on Apr. 23rd (this Friday).


## Mutex

Below are some APIs that you need to know for pthread mutex.

```C
#include <pthread.h>

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_init(pthread_mutexattr_t *attr);

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
```

To create a mutex you need to declare a `pthread_mutex_t` variable and initialize it. Recall in C, you must explicitly initialize the variable otherwise it will just contain some random bits.

```C
pthread_mutex_t mutex;
pthread_mutexattr_t mutex_attr;

// Initialize the attr first
pthread_mutexattr_init(&mutex_attr);
// If more fancy settings needed, you may need to call `pthread_mutexattr_setXXX(&mutex_attr, ...)` to further configure the attr

// Finally, use the attr to initialize the mutex
pthread_mutex_init(&mutex, &mutex_attr);

// Now that you are done with the attr, you should destroy it
pthread_mutexattr_destroy(&mutex_attr);
```

Alternatively, if you only want the default settings, you could just use the macro `PTHREAD_MUTEX_INITIALIZER` and assignment to initialize it.

```C
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
```

To lock/unlock the mutex

```C
pthread_mutex_lock(&mutex);
pthread_mutex_unlock(&mutex);
```

Finally, when you are done with the mutex, you should destroy it.

```C
pthread_mutex_destroy(&mutex);
```

After destroying the mutex, all calls to this mutex variable will fail. Sometimes it won't cause any problem without explicitly destroy it, but it is always a good practice to do so. See this [link](https://stackoverflow.com/questions/14721229/is-it-necessary-to-call-pthread-mutex-destroy-on-a-mutex) for more details.


## Condition Variable

Below are some functions (from the man page) that you may need to know for this project.

```C
#include <pthread.h>

int pthread_condattr_init(pthread_condattr_t *attr);
int pthread_condattr_destroy(pthread_condattr_t *attr);

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cond_attr);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_destroy(pthread_cond_t *cond);
```

To create a condition variable, you need to declare a `pthread_cond_t` variable and initialize it. pthread supports more complicated settings, which can be set by `pthread_condattr_t`. For this project, you don't need these settings, so just use the default one.

```C
pthread_cond_t cond_var;
pthread_condattr_t cond_var_attr;

pthread_condattr_init(cond_var_attr);
pthread_cond_init(&cond_var, &cond_var_attr);
```

Alternatively, if you only want the default settings, you could use the macro `PTHREAD_COND_INITIALIZER`.

```C
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
```

To wait on the condition variable, recall you must acquire the mutex first

```C
pthread_mutex_lock(&mutex);
// access/modify some shared variables

while (some_check()) // [QUIZ]: why while-loop here?
    pthread_cond_wait(&cond_var, &mutex);

pthread_mutex_unlock(&mutex);
```

To signal the waiting thread:

```C
pthread_cond_signal(&cond_var);
```

Depends on your implementation, you may or may not acquire the mutex when signaling the condition variable.

Again, you should destroy the condition variable after done with it.

```C
pthread_cond_destroy(&cond_var);
```

You should read the man page to fully understand the details.

## Shared Memory

We know that different threads of a process share the address space so they could access each other's memory. In fact, memory could also be shared across processes. Thanks to the idea of "virtual memory", shared memory could be implemented by mapping the same piece of physical memory to more than one process's address space. Then, these processes could communicate by read/write this shared memory. Note that, this physical memory could be mapped to different virtual addresses in different processes, so you probably don't want to store any pointer to the shared memory because a pointer is a virtual address; it means nothing on the other process's address space.

To create a shared memory object, use `shm_open`

```C
int shm_fd = shm_open("shm-chenhaoy", O_RDWR | O_CREAT, 0660);
```

This call will create a file as `/dev/shm/shm-chenhaoy`. This is a Unix philosophy that ["everything is a file"](https://en.wikipedia.org/wiki/Everything_is_a_file). Under this philosophy, many operations expose a file-like interface e.g. you have seen that the network IO is also done by a file descriptor. This means you could read/write to this object using a file operation. For example, you could remove this shared memory object by simply running `rm /dev/shm/shm-chenhaoy` from the shell.

To remove the shared memory object, you need to call `shm_unlink`.

`shm_open` will create a "fake" file and available to access it using file-like API, but it is not convenient to use `read`, `write`, and `lseek` to jump back and forth (image after reading the 12th bytes and you want to read the 6th bytes, you have to call `lseek` to rewind and then `read`; and you have to allocate some buffer for `read` syscall). This also has a lot of syscall overhead. This comes with another great idea: memory-mapped I/O.

## Memory-mapped I/O

The idea of memory-mapped I/O is, we know both "file" and "memory" are just a chunk of bits, why not fuse them together! We could just **map** the data of a file into the address space and read/write it just like read/write the memory. To do this, use `mmap`:

```C
void* ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
```

`mmap` will map the range `[offset, offset + length)` in the file `fd` into the address space and return the pointer of that chunk of memory. This means, if you modify a bytes at `ptr + 5`, this modification will be applied to the file at `offset + 5` (with some buffering for performance reasons). Note this is implemented by memory mapping (using page table), so `length` must be multiple of the page size.

Note `mmap` could be applied to normal files, but in this project, we apply it to a "fake" file (a shared memory object). You can view the shared memory object as a real file but not on disk, so it has great performance but not persistent.

`mmap` returns a raw pointer `void*`, and you need to make some rules for both processes accessing it (say, on what location storing what kind of data). We recommend doing it by casting this pointer into an array.

```C
typedef struct {
    int pid;
    // ...
} slot_t;
slot_t* shm_slot_ptr = (slot_t*) shm_ptr;

// for process A1:
shm_slot_ptr[0].pid = getpid();
// for process A2:
shm_slot_ptr[1].pid = getpid();

// for process B:
pid_a1 = shm_slot_ptr[0].pid;
pid_a2 = shm_slot_ptr[1].pid;
```

After you done with the shared memory (e.g. when you are going to exit), you need to call `munmap`.

## Signal

The idea of signaling is, the system provides a way to interrupt what you are executing and jump to handle an event. We probably won't have time to dive into the deatils. Instead, you could read the [document](https://www.gnu.org/software/libc/manual/html_node/Signal-Actions.html) yourself.


## Tips

- To use pthread and `shm_open`, you need to link with `pthread` and `rt` library during compilation. To do that, add `-lpthread` and `-lrt` in your makefile.

- You don't need to synchronize across two processes in this project. It is okay for the stat process to read some half-updated data by the web server. This means you don't need to put a mutex on the shared memory.


Finally, thanks for coming to my discussion section this semester! Good luck with your last project and the final exam.