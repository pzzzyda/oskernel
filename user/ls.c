#include "fs/fcntl.h"
#include "fs/fs.h"
#include "ulib.h"

struct file_info {
	char name[DIR_SIZE + 1];
	struct stat st;
	struct file_info *next;
};

void *malloc1(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr) {
		dprintf(2, "malloc failed\n");
		exit(1);
	}
	return ptr;
}

size_t numlen(uint32_t num)
{
	size_t len = 0;
	do {
		num /= 10;
		len++;
	} while (num > 0);
	return len;
}

size_t max(size_t x, size_t y)
{
	return x > y ? x : y;
}

void padding(size_t len, size_t width)
{
	while (len < width) {
		printf(" ");
		len++;
	}
}

void ls(char *path)
{
	char buf[512], *p;
	int fd;
	struct dir_entry de;
	struct stat st;
	struct file_info *infos, *info;
	struct file_info **pinfos = &infos;
	size_t w0, w1, tot;

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
		printf("%u %u %s\n", st.nlink, st.size, path);
		break;
	case FT_DIR:
		strcpy(buf, path);
		p = buf;
		while (*p && *p != '/')
			p++;
		if (*p != '/')
			*p++ = '/';
		w0 = 0;
		w1 = 0;
		tot = 0;
		while (read(fd, &de, sizeof(de)) == sizeof(de)) {
			if (de.ino == 0)
				continue;
			memmove(p, de.name, DIR_SIZE);
			p[DIR_SIZE] = 0;
			if (stat(buf, &st) < 0) {
				printf("cannot stat %s\n", buf);
				continue;
			}
			*pinfos = malloc1(sizeof(*infos));
			memmove((**pinfos).name, de.name, DIR_SIZE);
			(**pinfos).name[DIR_SIZE] = 0;
			memmove(&(**pinfos).st, &st, sizeof(st));
			(**pinfos).next = NULL;
			pinfos = &((**pinfos).next);
			w0 = max(w0, numlen(st.nlink));
			w1 = max(w1, numlen(st.size));
			tot += (st.size / BLOCK_SIZE) + 1;
		}
		printf("total %lu\n", tot);
		for (info = infos; info; info = info->next) {
			padding(numlen(info->st.nlink), w0);
			printf("%u ", info->st.nlink);
			padding(numlen(info->st.size), w1);
			printf("%u ", info->st.size);
			printf("%s\n", info->name);
		}
		while (infos) {
			info = infos;
			infos = infos->next;
			free(info);
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
