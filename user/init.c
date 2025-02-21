#include "fs/fcntl.h"
#include "fs/file.h"
#include "ulib.h"

static char *argv[] = { "/sh", 0 };

int main(void)
{
	pid_t pid;

	if (open("console", O_RDWR) < 0) {
		mknod("console", CONSOLE, 0);
		open("console", O_RDWR);
	}
	dup(0);
	dup(0);

	while (1) {
		pid = fork();
		if (pid == 0) {
			execve(argv[0], argv, environ);
			dprintf(2, "exec sh failed\n");
			exit(1);
		}
		while (1) {
			if (wait(NULL) == pid)
				break;
		}
	}
}
