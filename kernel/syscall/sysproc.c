#include "dev/timer.h"
#include "sched/cpu.h"
#include "syscall/syscall.h"

uint64_t sys_brk(void)
{
	return proc_grow(ARG(0, uint64_t));
}

uint64_t sys_fork(void)
{
	return fork();
}

uint64_t sys_wait(void)
{
	return wait(ARG(0, uint64_t));
}

uint64_t sys_exit(void)
{
	do_exit(ARG(0, int));
	return 0;
}

uint64_t sys_sleep(void)
{
	return timer_sleep(ARG(0, uint64_t));
}

uint64_t sys_kill(void)
{
	return kill(ARG(0, int));
}

uint64_t sys_getpid(void)
{
	return running_proc()->pid;
}

uint64_t sys_getppid(void)
{
	return running_proc()->parent->pid;
}

uint64_t sys_sbrk(void)
{
	int64_t increment;
	uint64_t old_size, new_size;

	increment = ARG(0, int64_t);
	old_size = running_proc()->size;
	if (increment < 0)
		new_size = old_size - (uint64_t)(-increment);
	else if (increment > 0)
		new_size = old_size + (uint64_t)(increment);
	else
		return old_size;

	if (proc_grow(new_size) == 0)
		return old_size;
	else
		return -1;
}

uint64_t sys_shutdown(void)
{
	asm volatile("li a7, 8");
	asm volatile("ecall");
	return 0;
}
