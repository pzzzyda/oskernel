#include "fs/fcntl.h"
#include "ulib.h"

char buf[1024];

void cat(int fd)
{
	ssize_t n;

	while ((n = read(fd, buf, 1024)) > 0) {
		if (write(1, buf, n) != n) {
			dprintf(2, "write error\n");
			exit(1);
		}
	}
	if (n < 0) {
		dprintf(2, "read error\n");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	int fd, i;

	if (argc < 2) {
		cat(0);
		exit(0);
	}

	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			dprintf(2, "failed to open %s\n", argv[i]);
			exit(1);
		}
		cat(fd);
		close(fd);
	}

	return 0;
}
