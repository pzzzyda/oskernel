#include "dev/timer.h"
#include "lock.h"
#include "riscv.h"
#include "sched/cpu.h"

#define INTERVAL 1000000

struct timer {
	uint64_t ticks;
	struct spin_lock lock;
};

static struct timer sys_timer;

void timer_init(void)
{
	sys_timer.ticks = 0;
	spin_lock_init(&sys_timer.lock, "timer");
}

void timer_intr(void)
{
	if (current_cpuid() == 0) {
		spin_lock_acquire(&sys_timer.lock);
		sys_timer.ticks++;
		spin_lock_release(&sys_timer.lock);
		wake_up(&sys_timer.ticks);
	}
	timer_set_next();
}

/* Set the next timer interrupt. */
void timer_set_next(void)
{
	uint64_t next = read_time() + INTERVAL;
	asm volatile("li a7, 0");
	asm volatile("mv a0, %0" : : "r"(next));
	asm volatile("ecall");
}

/* Sleep n ticks on the timer. */
int timer_sleep(uint64_t n)
{
	uint64_t ticks;
	spin_lock_acquire(&sys_timer.lock);
	ticks = sys_timer.ticks;
	while (sys_timer.ticks - ticks < n) {
		if (killed(running_proc())) {
			spin_lock_release(&sys_timer.lock);
			return -1;
		}
		sleep_on(&sys_timer.ticks, &sys_timer.lock);
	}
	spin_lock_release(&sys_timer.lock);
	return 0;
}
