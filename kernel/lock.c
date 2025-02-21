#include "lock.h"
#include "printk.h"
#include "riscv.h"
#include "sched/cpu.h"

void spin_lock_init(struct spin_lock *lock, const char *name)
{
	lock->locked = false;
	lock->cpuid = -1;
	lock->name = name;
}

void spin_lock_acquire(struct spin_lock *lock)
{
	push_off();
	if (spin_lock_holding(lock)) {
		printk("spin lock name: %s\n", lock->name);
		panic("repeatedly acquire lock");
	}
	while (__sync_lock_test_and_set(&lock->locked, true))
		continue;
	__sync_synchronize();
	lock->cpuid = current_cpuid();
}

void spin_lock_release(struct spin_lock *lock)
{
	if (!spin_lock_holding(lock)) {
		printk("spin lock name: %s\n", lock->name);
		panic("release unheld lock");
	}
	lock->cpuid = -1;
	__sync_synchronize();
	__sync_lock_release(&lock->locked);
	pop_off();
}

bool spin_lock_holding(struct spin_lock *lock)
{
	return lock->locked && lock->cpuid == current_cpuid();
}

void sleep_lock_init(struct sleep_lock *lock, const char *name)
{
	spin_lock_init(&lock->lock, "sleep_lock");
	lock->locked = false;
	lock->pid = -1;
	lock->name = name;
}

void sleep_lock_acquire(struct sleep_lock *lock)
{
	if (sleep_lock_holding(lock)) {
		printk("sleep lock name: %s\n", lock->name);
		panic("repeatedly acquire lock");
	}
	spin_lock_acquire(&lock->lock);
	while (lock->locked)
		sleep_on(lock, &lock->lock);
	lock->locked = true;
	lock->pid = running_proc()->pid;
	spin_lock_release(&lock->lock);
}

void sleep_lock_release(struct sleep_lock *lock)
{
	if (!sleep_lock_holding(lock)) {
		printk("sleep lock name: %s\n", lock->name);
		panic("release unheld lock");
	}
	spin_lock_acquire(&lock->lock);
	lock->locked = false;
	lock->pid = -1;
	wake_up(lock);
	spin_lock_release(&lock->lock);
}

bool sleep_lock_holding(struct sleep_lock *lock)
{
	bool is_holding;
	spin_lock_acquire(&lock->lock);
	is_holding = lock->locked && (running_proc()->pid == lock->pid);
	spin_lock_release(&lock->lock);
	return is_holding;
}
