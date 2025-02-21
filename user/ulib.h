#ifndef _ULIB_H
#define _ULIB_H

#include "fs/stat.h"

extern char **environ;

int brk(void *addr);
pid_t fork(void);
pid_t wait(int *pstate);
void exit(int state) __attribute__((noreturn));
void sleep(uint64_t ticks);
int kill(pid_t pid);
pid_t getpid(void);
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
int open(const char *path, int omode);
int close(int fd);
int mknod(const char *path, uint16_t major, uint16_t minor);
int execve(const char *name, char *const *argv, char *const *env);
int dup(int fd);
int mkdir(const char *path);
int fstat(int fd, struct stat *st);
int chdir(const char *path);
pid_t getppid(void);
void *sbrk(int64_t increment);
int pipe(int pipefd[2]);
int link(const char *from, const char *to);
int unlink(const char *name);
void shutdown(void) __attribute__((noreturn));
off_t lseek(int fd, off_t offset, int whence);
int dup2(int oldfd, int newfd);
int stat(const char *name, struct stat *st);
int execvp(const char *name, char *const *argv);
char *getcwd(char *buf, size_t max_len);

#define va_start(ap, list) (__builtin_va_start(ap, list))
#define va_arg(ap, type) (__builtin_va_arg(ap, type))
#define va_end(ap) (__builtin_va_end(ap))
#define va_copy(d, s) (__builtin_va_copy(d, s))
typedef __builtin_va_list va_list;

int printf(const char *fmt, ...);
int dprintf(int fd, const char *fmt, ...);

size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strtok(char *str, const char *delim);
size_t strspn(const char *str, const char *accept);
size_t strcspn(const char *str, const char *reject);
char *strchr(const char *str, int c);
char *strrchr(const char *str, int c);
char *strstr(const char *haystack, const char *needle);

void *memset(void *ptr, int v, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

int atoi(const char *nptr);

void *malloc(size_t size);
void free(void *ptr);

#endif
