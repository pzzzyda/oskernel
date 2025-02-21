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
#define PF_PADDING (1 << 1)
#define PF_ACCURACY (1 << 2)
#define PF_ALIGN (1 << 3)
#define PF_ZERO_PADDING (1 << 4)
#define PF_LONG (1 << 5)
#define PF_UPPER (1 << 6)
#define PF_POINTER (1 << 7)
#define PF_LONG_LONG (1 << 8)

static const char digits_l[] = "0123456789abcdef";
static const char digits_u[] = "0123456789ABCDEF";

static void dprint_int(int fd, long long x, int base, int left, int right,
		       int flag)
{
	char buf[70];
	const char *digits;
	int pos, len;
	bool is_negative;
	char padding_char;

	is_negative = (flag & PF_SIGNED) && (int64_t)x < 0;

	if (flag & PF_UPPER)
		digits = digits_u;
	else
		digits = digits_l;

	pos = sizeof(buf);
	do {
		--pos;
		buf[pos] = digits[x % base];
		x /= base;
	} while (x);
	len = sizeof(buf) - pos;

	if (flag & PF_POINTER) {
		--pos;
		buf[pos] = 'x';
		--pos;
		buf[pos] = '0';
		len += 2;
	}

	if ((flag & PF_ACCURACY) && !(flag & PF_POINTER) && (len < right)) {
		while (len < right) {
			--pos;
			buf[pos] = '0';
			++len;
		}
	}

	if (is_negative) {
		--pos;
		buf[pos] = '-';
		++len;
	}

	if (flag & PF_ALIGN) {
		write(fd, buf + pos, len);
		while (len < left) {
			write(fd, " ", 1);
			++len;
		}
		return;
	}

	if (flag & PF_PADDING) {
		if ((flag & PF_ZERO_PADDING) && !(flag & PF_POINTER))
			padding_char = '0';
		else
			padding_char = ' ';
		while (len < left) {
			--pos;
			buf[pos] = padding_char;
			++len;
		}
	}

	write(fd, buf + pos, len);
}

int vdprintf(int fd, const char *fmt, va_list args)
{
	int i, j, k;
	long long x;
	char c, *s;
	bool parsing;
	int flag, left, right;

	parsing = false;
	flag = 0;
	left = 0;
	right = 0;
	k = 0;
	for (i = 0; fmt[i]; i++) {
		c = fmt[i];

		if (!parsing) {
			if (c == '%') {
				parsing = true;
				k = i;
				/* check '-' */
				if (fmt[i + 1] == '-') {
					i++;
					flag |= PF_ALIGN;
				}
			} else {
				write(fd, &c, 1);
			}
			continue;
		}

		switch (c) {
		case 'i':
		case 'd':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			flag |= PF_SIGNED;
			dprint_int(fd, x, 10, left, right, flag);
			break;
		case 'u':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			dprint_int(fd, x, 10, left, right, flag);
			break;
		case 'o':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			dprint_int(fd, x, 8, left, right, flag);
			break;
		case 'b':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			dprint_int(fd, x, 2, left, right, flag);
			break;
		case 'p':
			flag |= PF_LONG;
			flag |= PF_POINTER;
		case 'x':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			dprint_int(fd, x, 16, left, right, flag);
			break;
		case 'X':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			flag |= PF_UPPER;
			dprint_int(fd, x, 16, left, right, flag);
			break;
		case 's':
			s = va_arg(args, char *);
			if (!s) {
				write(fd, "null", 4);
			} else {
				if (flag & PF_ACCURACY) {
					for (j = 0; j < right; j++) {
						if (s[j])
							write(fd, s + j, 1);
						else
							write(fd, " ", 1);
					}
				} else {
					write(fd, s, strlen(s));
				}
			}
			break;
		case 'c':
			c = va_arg(args, int);
			write(fd, &c, 1);
			break;
		case 'l':
			if ((flag & PF_LONG) == 0)
				flag |= PF_LONG;
			else
				flag |= PF_LONG_LONG;
			continue;
		case '.':
			flag |= PF_ACCURACY;
			continue;
		case '-':
			/* ignore */
			continue;
		case '0':
			if (left == 0) {
				flag |= PF_ZERO_PADDING;
				/* skip the other '0's after the first '0' */
				while (fmt[i + 1] == '0')
					i++;
				continue;
			}
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			flag |= PF_PADDING;
			if (flag & PF_ACCURACY)
				right = right * 10 + c - '0';
			else
				left = left * 10 + c - '0';
			continue;
		default:
			write(fd, fmt + k, i - k + 1);
			break;
		}
		parsing = false;
		flag = 0;
		left = 0;
		right = 0;
		k = 0;
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
