#include "ulib.h"
#include "fs/fcntl.h"
#include "fs/fs.h"
#include "fs/stat.h"

void _start()
{
	extern int main();
	exit(main());
}

/* printf flags */
#define PF_SIGNED (1 << 0)
#define PF_LONG (1 << 5)
#define PF_LONG_LONG (1 << 8)

static const char digits[] = "0123456789abcdef";

static void dprint_str(int fd, const char *str)
{
	if (!str)
		write(fd, "(null)", 6);
	else
		write(fd, str, strlen(str));
}

static void dprint_ptr(int fd, unsigned long long x)
{
	char buf[32];
	size_t pos = sizeof(buf);
	do {
		buf[--pos] = digits[x % 16];
		x /= 16;
	} while (x > 0);
	write(fd, "0x", 2);
	write(fd, buf + pos, sizeof(buf) - pos);
}

static void dprint_int(int fd, unsigned long long x, unsigned int base,
		       int flag)
{
	char buf[32];
	size_t pos;
	bool is_negative;
	if (flag & PF_SIGNED) {
		if ((long long)x < 0) {
			is_negative = true;
			x = (-(long long)x);
		} else {
			is_negative = false;
		}
	} else {
		is_negative = false;
	}
	pos = sizeof(buf);
	do {
		buf[--pos] = digits[x % base];
		x /= base;
	} while (x > 0);
	if (is_negative)
		write(fd, "-", 1);
	write(fd, buf + pos, sizeof(buf) - pos);
}

static int vdprintf(int fd, const char *fmt, va_list args)
{
	int i, j;
	char c;
	unsigned long long x;
	bool is_valid;
	int flag;
	for (i = 0; fmt[i]; i++) {
		if (fmt[i] != '%') {
			write(fd, fmt + i, 1);
			continue;
		}
		is_valid = false;
		flag = 0;
		for (j = i + 1; fmt[j]; j++) {
			c = fmt[j];
			switch (c) {
			case 'd':
			case 'u':
			case 'x':
				if (c == 'd') {
					flag |= PF_SIGNED;
					if (flag & PF_LONG_LONG)
						x = va_arg(args, long long);
					else if (flag & PF_LONG)
						x = va_arg(args, long);
					else
						x = va_arg(args, int);
				} else {
					if (flag & PF_LONG_LONG)
						x = va_arg(args,
							   unsigned long long);
					else if (flag & PF_LONG)
						x = va_arg(args, unsigned long);
					else
						x = va_arg(args, unsigned int);
				}
				if (c == 'd' || c == 'u')
					dprint_int(fd, x, 10, flag);
				else
					dprint_int(fd, x, 16, flag);
				is_valid = true;
				goto end;
			case 'p':
				x = (unsigned long long)va_arg(args, void *);
				dprint_ptr(fd, x);
				is_valid = true;
				goto end;
			case 's':
				dprint_str(fd, va_arg(args, const char *));
				is_valid = true;
				goto end;
			case 'c':
				c = va_arg(args, int);
				write(fd, &c, 1);
				is_valid = true;
				goto end;
			case '%':
				write(fd, "%", 1);
				is_valid = true;
				goto end;
			case 'l':
				if (!(flag & PF_LONG))
					flag |= PF_LONG;
				else
					flag |= PF_LONG_LONG;
				continue;
			default:
				goto end;
			}
		}

	end:
		if (!is_valid)
			write(fd, "%", 1);
		else
			i = j;
	}
	return i;
}

int printf(const char *fmt, ...)
{
	int i;
	va_list args;
	va_start(args, fmt);
	i = vdprintf(1, fmt, args);
	va_end(args);
	return i;
}

int dprintf(int fd, const char *fmt, ...)
{
	int i;
	va_list args;
	va_start(args, fmt);
	i = vdprintf(fd, fmt, args);
	va_end(args);
	return i;
}

size_t strlen(const char *s)
{
	size_t len = 0;
	while (s[len])
		len++;
	return len;
}

char *strcpy(char *dst, const char *src)
{
	char *s = dst;
	while (*src) {
		*s = *src;
		s++;
		src++;
	}
	*s = 0;
	return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
	char *s = dst;
	while (n > 0 && *src) {
		*s = *src;
		s++;
		src++;
		n--;
	}
	if (n > 0)
		*s = 0;
	return dst;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	while (n > 0 && *s1 && *s1 == *s2) {
		s1++;
		s2++;
		n--;
	}
	return n == 0 ? 0 : *s1 - *s2;
}

char *strtok(char *s, const char *delim)
{
	static char *olds;
	char *end;

	if (s == NULL)
		s = olds;

	if (*s == '\0') {
		olds = s;
		return NULL;
	}

	s += strspn(s, delim);
	if (*s == '\0') {
		olds = s;
		return NULL;
	}

	end = s + strcspn(s, delim);
	if (*end == '\0') {
		olds = end;
		return s;
	}

	*end = '\0';
	olds = end + 1;
	return s;
}

size_t strspn(const char *str, const char *accept)
{
	const char *p, *a;
	size_t count = 0;

	for (p = str; *p != '\0'; ++p) {
		for (a = accept; *a != '\0'; ++a)
			if (*p == *a)
				break;
		if (*a == '\0')
			return count;
		++count;
	}

	return count;
}

size_t strcspn(const char *str, const char *reject)
{
	const char *p, *r;
	size_t count = 0;

	for (p = str; *p != '\0'; ++p) {
		for (r = reject; *r != '\0'; ++r)
			if (*p == *r)
				return count;
		++count;
	}

	return count;
}

char *strchr(const char *str, int c)
{
	for (; *str; str++)
		if (*str == c)
			return (char *)str;
	return NULL;
}

char *strrchr(const char *str, int c)
{
	const char *result = NULL;
	for (; *str; str++)
		if (*str == c)
			result = str;
	return (char *)result;
}

char *strstr(const char *haystack, const char *needle)
{
	const char *h, *n, *p;

	if (*needle == '\0')
		return (char *)haystack;

	for (p = haystack; *p != '\0'; p++) {
		h = p;
		n = needle;

		while (*h != '\0' && *n != '\0' && *h == *n) {
			h++;
			n++;
		}

		if (*n == '\0')
			return (char *)p;
	}

	return NULL;
}

void *memset(void *ptr, int v, size_t n)
{
	while (n > 0)
		((uint8_t *)ptr)[--n] = v;
	return ptr;
}

void *memmove(void *dst, const void *src, size_t n)
{
	size_t i;
	if (dst < src) {
		for (i = 0; i < n; ++i)
			((uint8_t *)dst)[i] = ((const uint8_t *)src)[i];
	} else {
		for (i = n; i--;)
			((uint8_t *)dst)[i] = ((const uint8_t *)src)[i];
	}
	return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	return memmove(dst, src, n);
}

int atoi(const char *nptr)
{
	int n;

	n = 0;
	while ('0' <= *nptr && *nptr <= '9') {
		n *= 10;
		n += *nptr - '0';
		nptr++;
	}
	return n;
}

struct block {
	size_t size;
	bool free;
	struct block *next;
};

static struct block *head = NULL;

void *malloc(size_t size)
{
	struct block *curr, *prev, *new_block;
	size_t tot_size;

	if (size <= 0)
		return NULL;

	curr = head;
	prev = NULL;

	while (curr) {
		if (curr->free && curr->size >= size) {
			curr->free = false;
			return curr + 1;
		}
		prev = curr;
		curr = curr->next;
	}

	tot_size = sizeof(struct block) + size;
	new_block = sbrk(tot_size);
	if (new_block == (void *)(-1))
		return NULL;

	new_block->size = size;
	new_block->free = false;
	new_block->next = NULL;

	if (!prev)
		head = new_block;
	else
		prev->next = new_block;

	return new_block + 1;
}

void free(void *ptr)
{
	struct block *blk, *curr;

	if (!ptr)
		return;

	blk = ((struct block *)(ptr)) - 1;
	blk->free = true;

	curr = head;
	while (curr && curr->next) {
		if (curr->free && curr->next->free) {
			curr->size += sizeof(struct block);
			curr->next = curr->next->next;
		}
		curr = curr->next;
	}
}

char *getcwd(char *buf, size_t max_len)
{
	char cwd[128];
	char path[64];
	size_t i, j, len;
	struct stat st;
	int fd;
	struct dir_entry de;
	uint16_t ino;
	bool found;

	i = sizeof(cwd);
	j = 0;

	while (true) {
		/* Get the inode number of the current directory */
		path[j++] = '.';
		path[j] = 0;
		fd = open(path, O_RDONLY);
		if (!fd)
			return NULL;
		if (fstat(fd, &st) < 0) {
			close(fd);
			return NULL;
		}
		close(fd);
		ino = st.ino;

		/* Whether the current directory is the root directory? */
		if (ino == ROOT_INO) {
			if (i == sizeof(cwd))
				cwd[--i] = '/';
			break;
		}

		/* Open the parent directory */
		path[j++] = '.';
		path[j] = 0;
		fd = open(path, O_RDONLY);
		if (fd < 0)
			return NULL;
		/* Find the current directory in the parent directory */
		found = false;
		while (read(fd, &de, sizeof(de)) == sizeof(de)) {
			if (!de.ino)
				continue;
			if (de.ino == ino) {
				found = true;
				len = strlen(de.name);
				i -= len;
				memmove(cwd + i, de.name, len);
				cwd[--i] = '/';
				break;
			}
		}
		close(fd);
		if (!found)
			return NULL;

		path[j++] = '/';
		path[j] = 0;
	}

	len = sizeof(cwd) - i;
	if (!buf) {
		buf = malloc(len + 1);
		if (!buf)
			return NULL;
		else
			max_len = len + 1;
	} else {
		if (max_len < len)
			return NULL;
	}

	memmove(buf, cwd + i, len);
	if (max_len > len)
		buf[len] = 0;
	return buf;
}

int stat(const char *name, struct stat *st)
{
	int fd, ret;

	fd = open(name, O_RDONLY);
	if (fd < 0)
		return -1;
	ret = fstat(fd, st);
	close(fd);
	return ret;
}

static char *init_environ[16] = { "SHELL=/sh", 0 };
char **environ = init_environ;

int execvp(const char *name, char *const *argv)
{
	int fd;
	char buf[64] = "/";

	if (!strncmp(name, "./", 2) || !strncmp(name, "/", 1)) {
		fd = open(name, O_RDONLY);
		if (fd < 0)
			return -1;
		else
			close(fd);
		execve(name, argv, environ);
		return -1;
	} else {
		strcpy(buf + 1, name);
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			return -1;
		else
			close(fd);
		execve(buf, argv, environ);
		return -1;
	}
}
