#include "fs/fcntl.h"
#include "fs/fs.h"
#include "ulib.h"

void ls(char *path)
{
	char buf[512], *p;
	int fd;
	struct dir_entry de;
	struct stat st;

	if ((fd = open(path, O_RDONLY)) < 0) {
		dprintf(2, "cannot open %s\n", path);
		return;
	}

	if (fstat(fd, &st) < 0) {
		dprintf(2, "cannot stat %s\n", path);
		close(fd);
		return;
	}

	switch (st.type) {
	case FT_DEVICE:
	case FT_FILE:
		printf("%-2d %-4d %-6d %s\n", st.type, st.ino, st.size, path);
		break;
	case FT_DIR:
		if (strlen(path) + 1 + DIR_SIZE + 1 > sizeof buf) {
			printf("path too long\n");
			break;
		}
		strcpy(buf, path);
		p = buf + strlen(buf);
		*p++ = '/';
		while (read(fd, &de, sizeof(de)) == sizeof(de)) {
			if (de.ino == 0)
				continue;
			memmove(p, de.name, DIR_SIZE);
			p[DIR_SIZE] = 0;
			if (stat(buf, &st) < 0) {
				printf("cannot stat %s\n", buf);
				continue;
			}
			printf("%-2d %-4d %-6d %s\n", st.type, st.ino, st.size,
			       de.name);
		}
		break;
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	int i;

	if (argc < 2) {
		ls(".");
		exit(0);
	}
	for (i = 1; i < argc; i++)
		ls(argv[i]);
	return 0;
}
