#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

int main() {
	int pid = fork();
	if (pid == 0) {
	    // the child process will execute this
	    char *const argv[3] = {
	        "/bin/ls", // string literial is of type "const char*"
	        "-l",
	        NULL
	    };
	    int ret = execv(argv[0], argv);
	    // if succeeds, execve should never return
	    // if it returns, it must fail (e.g. cannot find the executable file)
	    printf("Fails to execute %s", argv[0]);
	    exit(1); // QUIZ: what if we don't have exit here?
	}
	return 0;
}
