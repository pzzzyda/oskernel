#ifndef _CPU_H
#define _CPU_H

#include "sched/proc.h"

struct cpu {
	/* The process running on this cpu, or NULL */
	struct process *proc;
	/* context_switch() here to enter scheduler() */
	struct context ctx;
	/* Depth of push_off() nesting */
	int n_off;
	/* Were interrupts enabled before push_off()? */
	bool intr_ena;
};

struct cpu *current_cpu(void);
int current_cpuid(void);
struct process *running_proc(void);
void push_off(void);
void pop_off(void);

#endif
