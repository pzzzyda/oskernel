#include "syscall/syscall.h"
#include "printk.h"
#include "sched/cpu.h"
#include "syscall/sysnum.h"

extern uint64_t sys_brk(void);
extern uint64_t sys_fork(void);
extern uint64_t sys_wait(void);
extern uint64_t sys_exit(void);
extern uint64_t sys_sleep(void);
extern uint64_t sys_kill(void);
extern uint64_t sys_getpid(void);
extern uint64_t sys_read(void);
extern uint64_t sys_write(void);
extern uint64_t sys_open(void);
extern uint64_t sys_close(void);
extern uint64_t sys_mkdir(void);
extern uint64_t sys_fstat(void);
extern uint64_t sys_execve(void);
extern uint64_t sys_chdir(void);
extern uint64_t sys_getppid(void);
extern uint64_t sys_dup(void);
extern uint64_t sys_mknod(void);
extern uint64_t sys_link(void);
extern uint64_t sys_unlink(void);
extern uint64_t sys_pipe(void);
extern uint64_t sys_sbrk(void);
extern uint64_t sys_shutdown(void);
extern uint64_t sys_lseek(void);
extern uint64_t sys_dup2(void);

static uint64_t (*syscalls[])(void) = {
	[SYS_brk] = sys_brk,	       [SYS_fork] = sys_fork,
	[SYS_wait] = sys_wait,	       [SYS_exit] = sys_exit,
	[SYS_sleep] = sys_sleep,       [SYS_kill] = sys_kill,
	[SYS_getpid] = sys_getpid,     [SYS_read] = sys_read,
	[SYS_write] = sys_write,       [SYS_open] = sys_open,
	[SYS_close] = sys_close,       [SYS_mkdir] = sys_mkdir,
	[SYS_fstat] = sys_fstat,       [SYS_execve] = sys_execve,
	[SYS_chdir] = sys_chdir,       [SYS_getppid] = sys_getppid,
	[SYS_dup] = sys_dup,	       [SYS_mknod] = sys_mknod,
	[SYS_link] = sys_link,	       [SYS_unlink] = sys_unlink,
	[SYS_pipe] = sys_pipe,	       [SYS_sbrk] = sys_sbrk,
	[SYS_shutdown] = sys_shutdown, [SYS_lseek] = sys_lseek,
	[SYS_dup2] = sys_dup2
};

#define N_SYSCALL (sizeof(syscalls) / sizeof(syscalls[0]))

void syscall(void)
{
	struct process *p = running_proc();
	uint64_t num = p->tf->a7;
	if (num > 0 && num < N_SYSCALL && syscalls[num] != 0) {
		p->tf->a0 = syscalls[num]();
	} else {
		printk("pid %d: unknown system call %lu\n", p->pid, num);
		p->tf->a0 = -1;
	}
}

uint64_t arg_raw(uint32_t num)
{
	struct process *p = running_proc();
	switch (num) {
	case 0:
		return p->tf->a0;
	case 1:
		return p->tf->a1;
	case 2:
		return p->tf->a2;
	case 3:
		return p->tf->a3;
	case 4:
		return p->tf->a4;
	case 5:
		return p->tf->a5;
	default:
		panic("illegal argument number");
		return -1;
	}
}

int fetch_str(uint64_t addr, char *buf, size_t len)
{
	return copy_str_in(running_proc()->page_table, buf, addr, len);
}

int fetch_addr(uint64_t addr, uint64_t *buf)
{
	return copy_in(running_proc()->page_table, buf, addr, sizeof(*buf));
}
