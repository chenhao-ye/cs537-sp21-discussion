# COMP SCI 537 Discussion Week 2

The plan for this week and next week:

- Both are for p2
- This week is designed for utility purposes. After taking this discussion, you should be able to do this project
- Next week we will talk more on xv6 to better understand how things work. You don't have to know them for this project, but they might help in the future projects

## Xv6 Compilation & GDB

To get the source code:

```shell
cp ~cs537-1/projects/xv6.tar.gz .
tar -xvzf xv6.tar.gz
```

To compile the xv6:

```shell
make
```

Recall an operating system is special software that manages all the hardware, so to run an OS, you need hardware resources like CPUs, memory, storage, etc. To do this, you could either have a real physical machine, or a virtualization software that "fakes" these hardwares. QEMU is an emulator that virtualizes some hardware to run xv6.

To compile the xv6 and run it in QEMU:

```shell
make qemu-nox
```

`-nox` here means no graphical interface, so it won't pop another window for xv6 console but directly show everything in your current terminal. The xv6 doesn't have a `shutdown` command; to quit from xv6, you need to kill the QEMU: press `ctrl-a` then `x`.

After compiling and running xv6 in QEMU, you could play walk around inside xv6 by `ls`.

```console
$ make qemu-nox

iPXE (http://ipxe.org) 00:03.0 CA00 PCI2.10 PnP PMM+1FF8CA10+1FECCA10 CA00
                                                                            


Booting from Hard Disk..xv6...
cpu1: starting 1
cpu0: starting 0
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap8
init: starting sh
$ ls
.              1 1 512
..             1 1 512
README         2 2 2286
cat            2 3 16272
echo           2 4 15128
forktest       2 5 9432
grep           2 6 18492
init           2 7 15712
kill           2 8 15156
ln             2 9 15012
ls             2 10 17640
mkdir          2 11 15256
rm             2 12 15232
sh             2 13 27868
stressfs       2 14 16148
usertests      2 15 67252
wc             2 16 17008
zombie         2 17 14824
console        3 18 0
stressfs0      2 19 10240
stressfs1      2 20 10240
stressfs3      2 21 10240
stressfs2      2 22 10240
stressfs4      2 23 10240
$ echo hello world
hello world
```

To run xv6 with gdb (another advantage of having a virtualization software!): in one window

```shell
make qemu-nox-gdb
```

then in another window:

```shell
gdb
```

You might get the error message blow:

```console
$ gdb
GNU gdb (Ubuntu 9.2-0ubuntu1~20.04) 9.2
Copyright (C) 2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word".
warning: File "/dir/xv6/.gdbinit" auto-loading has been declined by your `auto-load safe-path' set to "$debugdir:$datadir/auto-load".
To enable execution of this file add
        add-auto-load-safe-path /dir/xv6/.gdbinit
line to your configuration file "/u/c/h/chenhaoy/.gdbinit".
To completely disable this security protection add
        set auto-load safe-path /
line to your configuration file "/u/c/h/chenhaoy/.gdbinit".
For more information about this security protection see the
--Type <RET> for more, q to quit, c to continue without paging--
```

Recent gdb versions will not automatically load `.gdbinit` for security purposes. You could either:

- `echo "add-auto-load-safe-path $(pwd)/.gdbinit" >> ~/.gdbinit`. This enables the autoloading of `.gdbinit` in the current working directory.
- `echo "set auto-load safe-path /" >> ~/.gdbinit`. This enables the autoloading of every `.gdbinit`

After either operation, you should be able to launch gdb. Specify you want to attach to the kernel

```console
(gdb) file kernel
A program is being debugged already.
Are you sure you want to change the file? (y or n) y
Load new symbol table from "kernel"? (y or n) y
Reading symbols from kernel...
```

Let set a breakpoint at `allocproc`:

```console
(gdb) b allocproc
Breakpoint 1 at 0x80103790: file proc.c, line 79.
```

You will need to take a screenshot of using gdb as part of p2.

## Xv6 Source Code Walk Through

The key to do this project is (and most projects with an existing large code base), looking at the existing code and find something similar; copy it and do some modification.

The majority of the part will be done by walking through the code. Below is just a checklist:

- `Makefile`:
    -  `CPUS`: # CPUS for QEMU to virtualize
    -  `UPROGS`: what user-space program to build with the kernel. Note that xv6 is a very simple kernel that doesn't have a compiler, so every executable binary replies on the host Linux machine to build it along with the kernel.
    - TRY: take a tour on some user-space applications' code and compilation flags
    - TRY: add a new user-space application called `hello` which prints `"hello world\n"` to the stdout.

- `usys.S`: where syscalls interfaces are defined in the assembly

    - TRY: `gcc -S usys.S`:

        ```console
        $ gcc -S usys.S
        # 1 "usys.S"
        # 1 "<built-in>"
        # 1 "<command-line>"
        # 31 "<command-line>"
        # 1 "/usr/include/stdc-predef.h" 1 3 4
        # 32 "<command-line>" 2
        # 1 "usys.S"
        # 1 "syscall.h" 1
        # 2 "usys.S" 2
        # 1 "traps.h" 1
        # 3 "usys.S" 2
        # 11 "usys.S"
        .globl fork; fork: movl $1, %eax; int $64; ret
        .globl exit; exit: movl $2, %eax; int $64; ret
        .globl wait; wait: movl $3, %eax; int $64; ret
        .globl pipe; pipe: movl $4, %eax; int $64; ret
        .globl read; read: movl $5, %eax; int $64; ret
        .globl write; write: movl $16, %eax; int $64; ret
        .globl close; close: movl $21, %eax; int $64; ret
        .globl kill; kill: movl $6, %eax; int $64; ret
        .globl exec; exec: movl $7, %eax; int $64; ret
        .globl open; open: movl $15, %eax; int $64; ret
        .globl mknod; mknod: movl $17, %eax; int $64; ret
        .globl unlink; unlink: movl $18, %eax; int $64; ret
        .globl fstat; fstat: movl $8, %eax; int $64; ret
        .globl link; link: movl $19, %eax; int $64; ret
        .globl mkdir; mkdir: movl $20, %eax; int $64; ret
        .globl chdir; chdir: movl $9, %eax; int $64; ret
        .globl dup; dup: movl $10, %eax; int $64; ret
        .globl getpid; getpid: movl $11, %eax; int $64; ret
        .globl sbrk; sbrk: movl $12, %eax; int $64; ret
        .globl sleep; sleep: movl $13, %eax; int $64; ret
        .globl uptime; uptime: movl $14, %eax; int $64; ret
        ```

        Q: where does the syscalls arguments goes?

- `trap.c`: recall a syscall is implemented as an interrupt (sometimes also called "trap")

    - `void trap(struct trapframe *tf)`: how trap is handled
    - trap frame?

- `syscall.c` and `syscall.h`:

    - `void syscall(void)`: what does it do?
    - `static int (*syscalls[])(void) = { ... }`: what the hell is this?
    - Let's search around an existing syscall: `kill`

- `sysproc.c`:

    - `int sys_kill(void)`: why it takes `void` not `int`? `argtint`?

- `proc.c` and `proc.h`:

    - `struct proc`: per-process state
    - `struct context`: saved registers
    - `ptable.proc`: a pool of `struct proc`
    - `static struct proc* allocproc(void)`: where a `struct proc` gets allocated from `ptable.proc` and **initialized**
    - `int kill(int)`: how it gets implemented?

- `user.h`: The header for the user-space program to include. Otherwise?

- `defs.h`: what the difference compared to `user.h`?

Some other xv6 stuff: (maybe we leave them for the discussion section next week)

- `bootasm.S`: how the machine gets booted
- `main.c`: the `main()` function of the xv6 kernel (yes, the kernel also has its `main()` function)
- `vectors.S` and `trapasm.S`

## Notes on xv6

- In a user-space program, use `exit()` to terminate the process instead of `return 0` in the `main()`. It turns out that xv6's stack layout doesn't really handle `return` from `main()` so you have to use `exit()` to explicitly terminate the current process and release resources. We will talk more about it when in the future project.
- Be careful when using `best-linux`: to open another window for gdb, you need to ensure the new window connecting to the same physical machine. `best-linux` may rounte your new windows to another machine.

## Reference Links

- If you want to use Visual Studio Code remote ssh on CSL machines, [this link](https://shawnzhong.com/2019/10/16/remote-ssh-to-cs-lab-with-vscode/) provides a tutorial on how to set things up.

- You could also refer to the [xv6 document](https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf). Do not read it sequentially from the first page. Refer to a specific chapter when you are curious about a specific piece. Technically, read the xv6 code alone should be sufficient to do all the projects.
- [Here](https://courses.cs.washington.edu/courses/csep551/17wi/info/build.html) is another tutorial from the University of Washington. It talks about how to setting gdb for xv6.