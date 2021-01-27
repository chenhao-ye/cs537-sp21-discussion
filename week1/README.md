# COMP SCI 537 Discussion Week 1

## P1 Quick Note

The test scripts have been released. Checkout `~cs537-1/tests/p1/README.md`. Do not rely on the released test cases, but write your own test cases. Only ~80% of test cases are released.

>  Remember, the most dangerous bug is the one that crashes your program but the one working silently fine for now. Passing the given test cases is the purpose but a means. Do not do testcase-oriented programming.
>
> -- Mr. someone-I-just-made-up

Checkout project specificationsfor the late submission and slip days policy.

## CSL AFS Permission

CSL Linux machines use AFS (Andrew File System). It is a distributed filesystem (that why you see the same content when logging intodifferent CSL machines) with additional access control.

To see the permission of a directory

```shell
fs la /path/to/dir
```

You could also omit `/path/to/dir` to show the permission in the current working directory. You might get output like this:

```console
$ fs la ~cs537-1/handin/
Access list for /u/c/s/cs537-1/handin/ is
Normal rights:
  dusseau:cs537-tas rlidwka
  system:administrators rlidwka
  system:anyuser l
  remzi rlidwka
  dusseau rlidwka
```

- `r` (read): allow to read the content of files
- `l` (lookup): allow to list what in a directory
- `i` (insert): allow to create files
- `d` (delete): allow to delete files
- `w` (write): allow to modify the content of a file or `chmod`
- `a` (administer): allow to change access control

Check [this](https://computing.cs.cmu.edu/help-support/afs-acls) out for more how to interpret this output.

You should test all your code in CSL.

## Compilation

For this course, we use `gcc` as the standard compiler.

```shell
gcc -o my-code my-code.c -Wall -Werror
# be careful! DO NOT type "gcc -o my-code.c ..."
```

- `-Wall` flag means `gcc` will show most of the warnings it detects.
-  `-Werror` flag means `gcc` will treat the warnings as errors and reject compiling.

You should always fix the warnings because they could potentially be bugs. As you might have experienced, fixing a bug might only take a minute but finding a bug could take over your Friday night. It is always a good thing to capture and fix them during compilation.

If you want to be meaner when checking your code:

- `-Wextra`: It will be stricter on your code and show warnings not included in `-Wall`.
- `-pedantic`: As the name suggests... It could even complain if your source code file is not `\n`-terminated.

There might also be other compiling flags that you need to know.

- `-O1`/`-O2`/`-O3`: These are the level of optimization that your compiler will run. `-O3` is the strongest optimization and it could make your code much faster than `-O1`. However, the higher level of optimization also means the compiler would make more (reasonable) assumptions on your code; it should be fine for a well-written code but might cause problems for not well-written ones.

What you should do in CS 537 projects:

- Use as mean/strict flags as possible when writing our code. It could save a huge amount of time in debugging.
- Use the flags suggested in the project specification when submitting your Makefile.

Fun fact: in OS X, command `gcc` is actually an alias to `clang`, another compiler in the program llvm. Try `gcc -v` and you will see.

## GDB (GNU Debugger)

The best ways to be a bug-free programmer is simply not to write bug :) Unfortunately... so that's why we need a debugger. If you have used some IDE's breakpoints, you might have already used gdb (with a graphical interface).

To run gdb on an executable file, you should compile the executable file with `-g` to preserve debugging information.

```shell
gcc -g -o my-code my-code.c -Wall -Werror
gdb ./my-code
```

To list the code, use `list` or just `l`:

```
(gdb) l
1	#include <stdio.h>
2
3	int inc(int* p) {
4		int x = *p;
5		return x+1;
6	}
7
8	int main() {
9		int y = 4;
10		int z = inc(&y);
```

To set a breakpoint on the function `inc`, use `break` or just `b`:

```
(gdb) b inc
Breakpoint 1 at 0x844: file my-code.c, line 4.
```

You could also specify the line number:

```
(gdb) b 10
Breakpoint 1 at 0x884: file my-code.c, line 10.
```

Run until it hits the breakpoint, use `run` or just `r`:

```
(gdb) r
Starting program: /dir/my-code

Breakpoint 1, main () at my-code.c:10
10		int z = inc(&y);
```

To step in the next line without diving into the function, use `next` or just `n`:

```
(gdb) r
Starting program: /dir/my-code

Breakpoint 1, main () at my-code.c:10
10		int z = inc(&y);
(gdb) n
11		printf("z: %d\n", z);
```

To step to the next line and dive into the function, use `step` or just `s`:

```
(gdb) s

Breakpoint 2, inc (p=0xfffffffff2a0) at my-code.c:4
4		int x = *p;
```

To print the content oif a variable or event an expression, use `print` or just `p`:

```
(gdb) s
5		return x+1;
(gdb) p x
$1 = 4
(gdb) p *p
$2 = 4
(gdb) p x+3
$3 = 7
```

To continue until the current function returns, use `finish`:

```
(gdb) finish
Run till exit from #0  inc (p=0xfffffffff2a0) at my-code.c:5
0x0000aaaaaaaaa88c in main () at my-code.c:10
10		int z = inc(&y);
Value returned is $4 = 5
```

To continue normal execution until hitting the next breakpoint, use `continue` or just `c`:

```
(gdb) c
Continuing.
z: 5
[Inferior 1 (process 34771) exited normally]
```

To show some information, use `info`:

```
(gdb) info breakpoints
Num     Type           Disp Enb Address            What
1       breakpoint     keep y   0x0000aaaaaaaaa844 in inc at my-code.c:4
        breakpoint already hit 1 time
2       breakpoint     keep y   0x0000aaaaaaaaa884 in main at my-code.c:10
        breakpoint already hit 1 time
(gdb) info locals
x = 4
```

To quit gdb, use `quit` or just `q`:

```
(gdb) q
```

Gdb has many other powerful tools e.g. watch points, backtrace. Feel free to check it out. As a bonus, try `ctrl-x a` in gdb :)

## Sanitizer

Sanitizers could be a life-saver, not just in a pandemic, but also in programming :) Sanitizers are a set of tools to inject some checking instructions during the compilation. These instructions could provide warnings at the runtime.

### Memory Leak

To detect memory leak, use `-fsanitize=address`:

```
$ gcc -fsanitize=address -o leak leak.c
$ ./leak

=================================================================
==36716==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 4 byte(s) in 1 object(s) allocated from:
    #0 0xffff9686ca30 in __interceptor_malloc (/lib/aarch64-linux-gnu/libasan.so.5+0xeda30)
    #1 0xaaaace937868 in leak (/dir/leak+0x868)
    #2 0xaaaace937884 in main (/dir/leak+0x884)
    #3 0xffff9663208c in __libc_start_main (/lib/aarch64-linux-gnu/libc.so.6+0x2408c)
    #4 0xaaaace937780  (/dir/leak+0x780)

SUMMARY: AddressSanitizer: 4 byte(s) leaked in 1 allocation(s).
```

Note that compilation with sanitizer flags could significantly slow down your code (typically 3x to 10x); do not include it in your submission Makefile.

### Access Illegal Memory

`-fsanitize=address` could actually be helpful in many memory-related bugs, for example, illegal memory access:

```
$ gcc -fsanitize=address -o buf_overflow buf_overflow.c
$ ./buf_overflow
0
1
2
3
=================================================================
==37234==ERROR: AddressSanitizer: stack-buffer-overflow on address 0xffffe2598f40 at pc 0xaaaac056bdf4 bp 0xffffe2598ea0 sp 0xffffe2598ec0
READ of size 4 at 0xffffe2598f40 thread T0
    #0 0xaaaac056bdf0 in buf_overflow (/dir/buf_overflow+0xdf0)
    #1 0xaaaac056beac in main (/dir/buf_overflow+0xeac)
    #2 0xffffa5f1808c in __libc_start_main (/lib/aarch64-linux-gnu/libc.so.6+0x2408c)
    #3 0xaaaac056bac0  (/dir/buf_overflow+0xac0)

Address 0xffffe2598f40 is located in stack of thread T0 at offset 48 in frame
    #0 0xaaaac056bba8 in buf_overflow (/dir/buf_overflow+0xba8)

  This frame has 1 object(s):
    [32, 48) 'array' (line 5) <== Memory access at offset 48 overflows this variable
HINT: this may be a false positive if your program uses some custom stack unwind mechanism, swapcontext or vfork
      (longjmp and C++ exceptions *are* supported)
SUMMARY: AddressSanitizer: stack-buffer-overflow (/dir/buf_overflow+0xdf0) in buf_overflow
Shadow bytes around the buggy address:
  0x200ffc4b3190: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b31a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b31b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b31c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b31d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x200ffc4b31e0: 00 00 f1 f1 f1 f1 00 00[f3]f3 00 00 00 00 00 00
  0x200ffc4b31f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b3200: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b3210: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b3220: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x200ffc4b3230: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
  Shadow gap:              cc
==37234==ABORTING
```

## Valgrind

Valgrind could also be helpful for a memory leak. Different from sanitizers, valgrind does not require special compilation command.

```
$ gcc -o leak leak.c
$ valgrind ./leak
==38337== Memcheck, a memory error detector
==38337== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==38337== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==38337== Command: ./leak
==38337==
==38337==
==38337== HEAP SUMMARY:
==38337==     in use at exit: 4 bytes in 1 blocks
==38337==   total heap usage: 1 allocs, 0 frees, 4 bytes allocated
==38337==
==38337== LEAK SUMMARY:
==38337==    definitely lost: 4 bytes in 1 blocks
==38337==    indirectly lost: 0 bytes in 0 blocks
==38337==      possibly lost: 0 bytes in 0 blocks
==38337==    still reachable: 0 bytes in 0 blocks
==38337==         suppressed: 0 bytes in 0 blocks
==38337== Rerun with --leak-check=full to see details of leaked memory
==38337==
==38337== For lists of detected and suppressed errors, rerun with: -s
==38337== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

## The Art of Troubleshooting and Asking Questions

There is no stupid question, but some questions are more likely to get help than the others:

- Be informative: Describe the problems with necessary information
    - Do not: "Hey TA, my code doesn't work."
    - Do: "My code is supposed to do X, but it does Y."
- Be concrete: Use gdb to at least locate which line(s) of code goes wrong; typically checking variables' value and track down at which point it has an expected value and you can't understand why it happens.
    - Do not: Send a 200-line source code file to TAs and ask them to debug for you.
    - Do: Use gdb to narrow down the problems.

Also, try the tools we discuss today. They could save you from a lot of trouble.

## Reference Links

- Never forget `man` in your command line :)

- [Cppreference](https://en.cppreference.com/w/c) is primary for C++ reference, but it also has pretty references for C (with examples!).
- [This git tutorial](https://git-scm.com/docs/gittutorial) could be helpful if you are not familiar with git.

- If you want to use Visual Studio Code remote ssh on CSL machines, [this link](https://shawnzhong.com/2019/10/16/remote-ssh-to-cs-lab-with-vscode/) provides a tutorial on how to set things up.

