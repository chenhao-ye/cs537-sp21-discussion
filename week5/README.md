# COMP SCI 537 Discussion Week 5

## File Descriptor: How It Works

If you have event printed out a file descriptor, you may notice it is just an integer

```C
int fd = open("filename", O_RDONLY);
printf("fd: %d\n", fd);
// you may get output "3\n"
```

However, the file-related operation is stateful: e.g. it must keep a record what is the current read/write offset, etc. How does an integer encoding this information?

It turns out there is a level of indirection here: the kernel maintains such states and the file descriptor it returns is essentially a "handler" for later index back to these states. The kernel provides the guarantee that "the user provides the file descriptor and the operations to perform, the kernel will look up the file states that corresponding to the file descriptor and operate on it".

It would be helpful to understand what happens with some actual code from a kernel. For simplicity, we use ths xv6 code below to illustrate how file descriptor works, but remember, your p3 is on Linux. Linux has a very similar implementation.

For every process state (`struct proc` in xv6), it has an array of `struct file` (see the field  `proc.ofile`) to keep a record of the files this process has opened.

```C
// In proc.h
struct proc {
  // ...
  int pid;
  // ...
  struct file *ofile[NOFILE];  // Open files
};

// In file.h
struct file {
  // ...
  char readable;    // these two variables are actually boolean, but C doesn't have `bool` type,
  char writable;    // so the authors use `char`
  // ...
  struct inode *ip; // this is a pointer to another data structure called `inode`
  uint off;         // offset
};
```

The file descriptor is essentially an index for `proc.ofile`. In the example above, when opening a file named "filename", the kernel allocates a `struct file` for all the related state of this file. Then it stores the address of this `struct file` into `proc.ofile[3]`. In the future, when providing the file descript `3`, the kernel could get `struct file` by using `3` to index `proc.ofile`. This also gives you a reason why you should `close()` a file after done with it: the kernel will not free `struct file` until you `close()` it; also `proc.ofile` is a fixed-size array, so it has limit on how many files a process can open at max (`NOFILE`).

In addition, file descriptors `0`, `1`, `2` are reserved for stdin, stdout, and stderr.

### File Descriptors after `fork()`

During `fork()`, the new child process will copy `proc.ofile` (i.e. copying the pointers to `struct file`), but not `struct file` themselves. In other words, after `fork()`, both parent and child will share `struct file`. If the parent changes the offset, the change will also be visible to the child.

```
struct proc: parent {
	+---+
	| 0 | ------------+---------> [struct file: stdin]
	+---+             |
	| 1 | --------------+-------> [struct file: stdout]
	+---+             | |
	| 2 | ----------------+-----> [struct file: stderr]
	+---+             | | |
	...               | | |
}                     | | |
                      | | |
struct proc: child {  | | |
	+---+             | | |
	| 0 | ------------+ | |
	+---+               | |
	| 1 | --------------+ |
	+---+                 |
	| 2 | ----------------+
	+---+
	...
}
```

## Redirection

### High-level Ideas of Redirection: What to Do

When a process writes to stdout, what it actually does it is writing data to the file that is associated with the file descriptor `1`. So the trick of redirection is, we could replace the `struct file` pointer at `proc.ofile[1]` with another `struct file`.

For example, when handling the shell command `ls > log.txt`, what the file descriptors eventually should look like:

```
struct proc: parent {                                      <= `mysh` process
	+---+
	| 0 | ------------+---------> [struct file: stdin]
	+---+             |
	| 1 | ----------------------> [struct file: stdout]
	+---+             | 
	| 2 | ----------------+-----> [struct file: stderr]
	+---+             |   |
	...               |   |
}                     |   |
                      |   |
struct proc: child {  |   |                                <= `ls` process
	+---+             |   |
	| 0 | ------------+   |
	+---+                 |
	| 1 | ----------------|-----> [struct file: log.txt]   <= this is a new stdout!
	+---+                 |
	| 2 | ----------------+
	+---+
	...
}
```

### `dup2()`: How to Do

The trick to implement the redirection is the syscall `dup2`. This syscall performs the task of "duplicating a file descriptor".

```C
int dup2(int oldfd, int newfd);
```

`dup2` takes two file descriptors as the arguments. It performs these tasks (with some pseudo-code):

1. if the file descriptor `newfd` has some associated files, close it. (`close(newfd)`)
2. copy the file associated with `oldfd` to `newfd`. (`proc.ofile[newfd] = proc.ofile[oldfd]`)

Consider the provious example with `dup2`:

```C
int pid = fork();
if (pid == 0) { // child;
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644); // [1]
    dup2(fd, fileno(stdout));                                     // [2]
    // execv(...)
}
```

Here `fileno(stdout)` will give the file descriptor associated with the current stdout. After executing `[1]`, you should have:

```
struct proc: parent {                                     <= `mysh` process
	+---+
	| 0 | ------------+---------> [struct file: stdin]
	+---+             |
	| 1 | --------------+-------> [struct file: stdout]
	+---+             | |
	| 2 | ----------------+-----> [struct file: stderr]
	+---+             | | |
	...               | | |
}                     | | |
                      | | |
struct proc: child {  | | |                               <= child process (before execv "ls")
	+---+             | | |
	| 0 | ------------+ | |
	+---+               | |
	| 1 | --------------+ |
	+---+                 |
	| 2 | ----------------+
	+---+
	| 3 | ----------------------> [struct file: log.txt]  <= open a file "log.txt"
	+---+
	...
}
```

After executing `[2]`, you should have

```
struct proc: parent {                                     <= `mysh` process
	+---+
	| 0 | ------------+---------> [struct file: stdin]
	+---+             |
	| 1 | ----------------------> [struct file: stdout]
	+---+             | 
	| 2 | ----------------+-----> [struct file: stderr]
	+---+             |   |
	...               |   |
}                     |   |
                      |   |
struct proc: child {  |   |                               <= child process (before execv "ls")
	+---+             |   |
	| 0 | ------------+   |
	+---+                 |
	| 1 | --------------+ |
	+---+               | |
	| 2 | ----------------+
	+---+               |
	| 3 | --------------+-------> [struct file: log.txt]
	+---+
	...
}
```

Alas, compared to the figure of what we want, it has a redudent file descriptor `3`. We should close it. The modified code should look like this:

```C
int pid = fork();
if (pid == 0) { // child;
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644); // [1]
    dup2(fd, fileno(stdout));                                     // [2]
    close(fd);                                                    // [3]
    // execv(...)
}
```

After we finishing `dup2`, the previous `fd` is no longer useful. We close it at `[3]`. Then the file descriptors should look like this:

```
struct proc: parent {                                     <= `mysh` process
	+---+
	| 0 | ------------+---------> [struct file: stdin]
	+---+             |
	| 1 | ----------------------> [struct file: stdout]
	+---+             | 
	| 2 | ----------------+-----> [struct file: stderr]
	+---+             |   |
	...               |   |
}                     |   |
                      |   |
struct proc: child {  |   |                               <= child process (before execv "ls")
	+---+             |   |
	| 0 | ------------+   |
	+---+                 |
	| 1 | --------------+ |
	+---+               | |
	| 2 | ----------------+
	+---+               |
	| X |               +-------> [struct file: log.txt]
	+---+
	...
}
```

Despite the terrible drawing, we now have what we want! You can imagine doing it in a similar way for stdin redirection.

Again, you should read the [document](https://man7.org/linux/man-pages/man2/dup2.2.html) yourself to understand how to use it.

Fun fact, piping is actually also implemented in a similar way.

## Difference Between `FILE*` and File Descriptors

Other than `open`, you may also see functions like `fopen`:

```C
FILE *fopen(const char *pathname, const char *mode);
```

The return value of `fopen` is a `FILE` pointer, not an integer (file descriptor).

Note that, `fopen()` is not a syscall, but a library function (in `stdio.h`. It is a wrapper built on top of `open()`. Similarly, `FILE` is also defined in `stdio.h`, which implements some buffering for IO. When calling `fopen()`, the library allocates a `FILE` object for you on the heap. When calling `fclose()`, it then frees `FILE` for you. Note that you should never free `FILE` yourself. Whatever allocated by the library, unless otherwise specified, you should leave it to the library to free them.

## Some Exam Preparation Tips

1. Be sure to walk through the slides! That's always the first-hand materials that you should care about.
2. Do practice exams yourself. If you have enough time, you could do all of them; if not, which is likely to happen, be sure to do at least one of them yourself, and quickly go through with others.

For next week's discussion, we will discuss p4, which is on xv6. Before coming to the next week's discussion, be sure to read the project specification watch week3 video if you haven't.

## Q & A?