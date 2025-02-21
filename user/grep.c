#include "fs/fcntl.h"
#include "ulib.h"

char buf[1024];

void grep(char *pattern, int fd)
{
	ssize_t n, i;
	char *line, *b;

	i = 0;
	while ((n = read(fd, buf + i, 1023 - i)) > 0) {
		n = i + n;
		buf[n] = 0;
		i = 0;
		line = buf;
		while ((b = strchr(line, '\n'))) {
			*b = 0;
			i = i + (b - line) + 1;
			if (strstr(line, pattern)) {
				*b = '\n';
				write(1, line, (b - line) + 1);
			}
			line = b + 1;
		}
		memmove(buf, buf + i, n - i);
		i = n - i;
	}
}

int main(int argc, char *argv[])
{
	int fd, i;

	if (argc < 2) {
		dprintf(2, "usage: %s [pattern] [file]\n", argv[0]);
		exit(1);
	}

	if (argc == 2) {
		grep(argv[1], 0);
		exit(0);
	}

	for (i = 2; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			dprintf(2, "grep: open %s failed\n", argv[i]);
			exit(1);
		}
		grep(argv[1], fd);
		close(fd);
	}

	return 0;
}
