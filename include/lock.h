#ifndef _LOCK_H
#define _LOCK_H

#include "types.h"

struct spin_lock {
	bool locked;
	int cpuid;
	const char *name;
};

void spin_lock_init(struct spin_lock *lock, const char *name);
void spin_lock_acquire(struct spin_lock *lock);
void spin_lock_release(struct spin_lock *lock);
bool spin_lock_holding(struct spin_lock *lock);

struct sleep_lock {
	struct spin_lock lock;
	bool locked;
	pid_t pid;
	const char *name;
};

void sleep_lock_init(struct sleep_lock *lock, const char *name);
void sleep_lock_acquire(struct sleep_lock *lock);
void sleep_lock_release(struct sleep_lock *lock);
bool sleep_lock_holding(struct sleep_lock *lock);

#endif
