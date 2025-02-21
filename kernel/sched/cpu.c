#include "sched/cpu.h"
#include "printk.h"
#include "riscv.h"

static struct cpu cpus[N_CPU];

struct cpu *current_cpu(void)
{
	int id = current_cpuid();
	return &cpus[id];
}

int current_cpuid(void)
{
	int id = read_tp();
	return id;
}

struct process *running_proc(void)
{
	struct cpu *c;
	struct process *p;

	push_off();
	c = current_cpu();
	p = c->proc;
	pop_off();
	return p;
}

void push_off(void)
{
	struct cpu *c = current_cpu();
	bool intr_ena = intr_get();
	intr_off();
	if (c->n_off == 0)
		c->intr_ena = intr_ena;
	c->n_off++;
}

void pop_off(void)
{
	struct cpu *c = current_cpu();
	if (intr_get())
		panic("interrupts enabled");
	if (c->n_off < 1)
		panic("pop off");
	c->n_off--;
	if (c->n_off == 0 && c->intr_ena)
		intr_on();
}
