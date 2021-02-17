# COMP SCI 537 Discussion Week 4

Agenda: we will talk about some basic shell designs this week, and go deeper into more advanced functionality like redirection and built-in commands.

## Overview: What is a Shell

To begin with, we first discuss what is shell and what is not. Shell is a program that takes input as "commands" line by line, parses them, then

- For most of the commands, the shell will create a new process and load it with an executable file to run. E.g. `gcc`, `ls`
- For some commands, the shell will react to them directly without creating a new process. These are called "built-in" commands. E.g. `alias`

If you are curious about which catalog a command falls into, try `which cmd_name`:

```console
# The output of the following commands may vary depending on what shell you are running
# Below is from zsh. If you are using bash, it may look different
$ which gcc
/usr/bin/gcc
$ which alias
alias: shell built-in command
$ which which
which: shell built-in command
```

Fun fact: Some commands might fall in both catalogs (e.g. `echo`) because these commands are usually very easy to implement but commonly used, so shell just do it by itself to avoid the overhead of creating another process.

Thus, for commands in the first catalog, you should NOT try to implement them yourself! The only thing your shell should do is just creating a new process and load it with executable files.

### `fork`: So Is the Process Created

The syscall `fork()` creates a copy of the current process. We call the original process "parent" and the newly created process "child". But how are we going to tell which is which if they are the same??? Luckily we are not in the sci-fi story so we don't have to make it into a philosophy problem. The child process will get the return value 0 from `fork` while the parent will get the child's pid as the return value.

```C
pid_t pid = fork();
if (pid == 0) { // the child process will execute this
    printf("I am child with pid %d. I got return value from fork: %d\n", getpid(), pid);
    exit(0); // we exit here so the child process will not keep running
} else { // the parent process will execute this
    printf("I am parent with pid %d. I got return value from fork: %d\n", getpid(), pid);
}
```

You could find the code above in the repo as `fork_example.c`. After executing it, you will see output like this:

```console
$ ./fork_example
I am parent with pid 46565. I got return value from fork: 46566
I am child with pid 46566. I got return value from fork: 0
```

## `exec`: Only Then, We Are Not Just Clones

`fork` itself is not sufficient to run an operating system. It can only create a copy of the previous program, but we don't want the exact same program all the time. That's when `exec` shines. `exec` is actually a family of functions, including `execve`, `execl`, `execle`, `execlp`, `execv`, `execvp`, `execvP`... The key one is `execve` (which is the actual syscall) and the rest of them are just some wrappers in the glibc that does some work and then calls `execve`. For this project, `execv` is probably what you need. It is slightly less powerful than `execve`, but is enough for this project.

What `exec` does is: it accepts a path/filename and finds this file. If it is an executable file, it then destroys the current address space (including code, stack, heap, etc), loads in the new code, and starts executing the new code. To view it in a nerdy (or cool!) way, `fork` creates a new body and `exec` gives it a new soul.

```C
int execv(const char *path, char *const argv[])
```

Here is how `execv` works: it takes a first argument `path` to specify where the executable file locates, and then provides the command-line arguments `argv`, which will eventually become what the target program receives in their main function `int main(int argc, char* argv[])`.

```C
pid_t pid = fork();
if (pid == 0) {
    // the child process will execute this
    char *const argv[3] = {
        "/bin/ls", // string literial is of type "const char*"
        "-l",
        NULL // it must have a NULL in the end of argv
    };
    int ret = execv(argv[0], argv);
    // if succeeds, execve should never return
    // if it returns, it must fail (e.g. cannot find the executable file)
    printf("Fails to execute %s\n", argv[0]);
    exit(1); // QUIZ: what if we don't have exit here?
}
// do parent's work
```

You could find the code above in the repo as `exec_example.c`. After executing it, you will see output exactly like executing `ls -l`.

There is a few points worth more attention:

- `argv` must end with a `NULL`. Otherwise, the kernel has no idea how many `char*` you want to pass in. The consequence of missing `NULL` is likely to be a segfault because your program will interpret some random bytes in memory as char pointers.
- You may notice there is some redundancy between `path` and `argv`: if `path` is always the same as `argv[0]`, why bother to have `path`? You don't have to worry about this for this project, but in reality, these `path` and `argv[0]` have different semantics and not necessarily the same. `path` is to tell `execv` where to find the executable file while `argv[0]` is letting the target program how it was invoked.
- There is also something about path/filename...

As the last word of this section, I strongly recommend you to read the [document](https://linux.die.net/man/3/execv) yourself to understand how it works.

## `waitpid`: How to Be a Good Parent

Now you know the shell is the parent of all the processes it creates. The next question is, when executing a command, the shell should suspend and not asking for new inputs until the child process finishes executing.

```console
$ sleep 10 # this will create a new process executing /usr/bin/sleep
```

The command above will create a child process that does nothing other than sleeping for 10 seconds. During this period, you may notice your shell is also not printing a new prompt. This is what the shell does: it waits until the child process to terminate (no matter voluntarily or not). If you use `ctrl-c` to terminate `sleep`, you should see the shell would print the prompt immediately.

This is also be done by a syscall `waitpid`:

```C
pid_t waitpid(pid_t pid, int *status, int options);
```

This syscall will suspend the parent process and resume again when the child is done. It takes three arguments: `pid`, a pointer to an int `status`, and some flags `options`. `pid` is the pid of the child that the parent wants to waiit. `status` is a pointer pointing a piece of memory that `waitpid` could write the status to. `options` allows you to configure when `waitpid` should return. By default (`options = 0`), it only returns when the child terminates. This should be sufficient for this project.

Side note: NEVER write code like this:

```C
// assume we know pid
int* status;
waitpid(pid, status, 0); // QUIZ: what is wrong?
```

`*status` will be filled in the status that `waitpid` returns. This allows you to have more info about what happens to the child. You could use some macro defined in the library to check it, e.g. `WIFEXITED(status)` is true if the child terminated normally (i.e. not killed by signals); `WEXITSTATUS(status)` could tell you what is the exit code from the child process (e.g. if the child `return 0` from the main function, `WEXITSTATUS(status)` will give you zero).

Again, you should read the [document](https://linux.die.net/man/2/waitpid) yourself carefully.

Putting what we have discussed together, you should now have some idea of the skeleton of the shell

```C
pid_t pid = fork(); // create a new child process
if (pid == 0) { // this is child
    // do some preparation
    execv(path, argv);
    exit(1); // this means execv() fails
}
// parent
int status;
waitpid(pid, &status, 0); // wait the child to finish its work before keep going
// continue to handle the next command
```

## Parsing

What we have discussed is basically the backend of the shell. We now briefly talk about how the front-end works. There are two functions that you might be interesting:

- `fgets`: this function allows you to read a line from a file (recall, `stdin`, to some extend, is also an abstract file). Read the [document](https://linux.die.net/man/3/fgets) for more.

- `strtok`: this function would help you to split a line into words (i.e. delimits by whitespace). Read the [document](https://linux.die.net/man/3/strtok) for more. You may want to pay more attention of the lifecycle of a string. `strtok` will modify the string in-place, so if the original string gets destroyed (or overwritten), you can't access what you get from `strtok` (or you will have a segfault/garbage).

We will talk about redirection and built-in commands next week. As a reminder, you have a warm-up quiz for p3 due next Tuesday.

