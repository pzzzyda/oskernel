#include "sched/proc.h"
#include "fs/file.h"
#include "fs/fs.h"
#include "fs/inode.h"
#include "fs/log.h"
#include "lib/string.h"
#include "memlayout.h"
#include "printk.h"
#include "riscv.h"
#include "sched/cpu.h"
#include "sched/proc.h"
#include "trap/trap.h"

static struct process procs[N_PROC];

static struct process *init_proc;

static struct spin_lock pid_lock;
static uint8_t pid_bitmap[4096];

static struct spin_lock wait_lock;

extern void context_switch(struct context *from, struct context *to);

#define FIRST_PROC (&procs[0])
#define LAST_PROC (&procs[N_PROC - 1])

static pid_t pid_alloc(void)
{
	static int byte = 0;
	static int shift = 0;
	int b = byte;
	int s = shift;
	spin_lock_acquire(&pid_lock);
	do {
		for (; s < 8; s++) {
			if ((pid_bitmap[byte] & (1 << s)) == 0) {
				pid_bitmap[byte] |= (1 << s);
				byte = b;
				shift = s + 1;
				spin_lock_release(&pid_lock);
				return b * 8 + s;
			}
		}
		b = (b + 1) % 4096;
		s = 0;
	} while (!(b == byte && s == shift));
	spin_lock_release(&pid_lock);
	panic("no free pid");
}

static void pid_free(pid_t pid)
{
	int b = pid / 8;
	int s = pid % 8;
	spin_lock_acquire(&pid_lock);
	pid_bitmap[b] &= ~(1 << s);
	spin_lock_release(&pid_lock);
}

void proc_init(void)
{
	uint32_t i;
	spin_lock_init(&pid_lock, "pid_lock");
	spin_lock_init(&wait_lock, "wait_lock");
	for (i = 0; i < N_PROC; i++) {
		spin_lock_init(&procs[i].lock, "process");
		procs[i].pid = -1;
		procs[i].state = PROC_UNUSED;
		procs[i].kernel_stack = KERNEL_STACK(i);
	}
}

static void fork_return(void)
{
	static bool first = true;
	spin_lock_release(&running_proc()->lock);
	if (first) {
		first = false;
		fs_init(ROOT_DEV);
	}
	user_trap_return();
}

struct process *proc_alloc(void)
{
	struct process *p;
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		spin_lock_acquire(&p->lock);
		if (p->state == PROC_UNUSED) {
			p->pid = pid_alloc();
			p->state = PROC_USED;
			p->tf = pm_alloc();
			if (!p->tf) {
				proc_free(p);
				spin_lock_release(&p->lock);
				return NULL;
			}
			p->page_table = get_user_page_table(p);
			if (!p->page_table) {
				proc_free(p);
				spin_lock_release(&p->lock);
				return NULL;
			}
			memset(&p->ctx, 0, sizeof(p->ctx));
			p->ctx.ra = (uint64_t)fork_return;
			p->ctx.sp = p->kernel_stack + PAGE_SIZE;
			return p;
		}
		spin_lock_release(&p->lock);
	}
	return NULL;
}

void proc_free(struct process *p)
{
	if (p->tf)
		pm_free(p->tf);
	p->tf = NULL;
	if (p->page_table)
		free_user_page_table(p->page_table, p->size);
	p->page_table = NULL;
	p->size = 0;
	pid_free(p->pid);
	p->pid = -1;
	p->parent = NULL;
	p->chan = NULL;
	p->killed = false;
	p->xstate = 0;
	p->state = PROC_UNUSED;
}

int proc_grow(uint64_t size)
{
	struct process *p = running_proc();
	uint64_t new_size = 0;
	if (size > p->size)
		new_size = uvm_alloc(p->page_table, p->size, size, PTE_W);
	else
		new_size = uvm_dealloc(p->page_table, p->size, size);
	if (new_size != 0) {
		p->size = new_size;
		return 0;
	} else {
		return -1;
	}
}

void proc_dump(void)
{
	struct process *p;
	char *state;
	printk("\n");
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		if (p->state == PROC_UNUSED)
			continue;
		switch (p->state) {
		case PROC_USED:
			state = "used    ";
			break;
		case PROC_RUNNABLE:
			state = "runnable";
			break;
		case PROC_RUNNING:
			state = "running ";
			break;
		case PROC_SLEEPING:
			state = "sleeping";
			break;
		case PROC_ZOMBIE:
			state = "zombie  ";
			break;
		default:
			state = "???     ";
			break;
		}
		printk("%s %d %s\n", state, p->pid, p->name);
	}
}

void scheduler(void)
{
	struct process *p;
	bool found;
	struct cpu *c;

	c = current_cpu();
	c->proc = NULL;
	while (true) {
		/*
		 * The most recent process to run may have had interrupts turned
		 * off; enable them to avoid a deadlock if all processes are
		 * waiting.
		 */
		intr_on();
		found = false;
		for (p = FIRST_PROC; p <= LAST_PROC; p++) {
			spin_lock_acquire(&p->lock);
			if (p->state == PROC_RUNNABLE) {
				found = true;
				/*
				 * Switch to chosen process.  It is the
				 * process's job to release its lock and then
				 * reacquire it before jumping back to us.
				 */
				p->state = PROC_RUNNING;
				c->proc = p;
				context_switch(&c->ctx, &p->ctx);
				/*
				 * Process is done running for now.
				 * It should have changed its p->state before
				 * coming back.
				 */
				c->proc = NULL;
			}
			spin_lock_release(&p->lock);
		}
		if (!found) {
			/*
			 * Nothing to run; stop running on this core until an
			 * interrupt.
			 */
			intr_on();
			asm volatile("wfi");
		}
	}
}

void sched(void)
{
	struct process *p;
	struct cpu *c;
	bool intr_ena;

	p = running_proc();
	c = current_cpu();

	if (!spin_lock_holding(&p->lock))
		panic("the process lock is not held during scheduling");
	if (c->n_off != 1)
		panic("locked during scheduling");
	if (p->state == PROC_RUNNING)
		panic("schedule a running process");
	if (intr_get())
		panic("scheduling can be interrupted");

	intr_ena = c->intr_ena;
	context_switch(&p->ctx, &c->ctx);
	c->intr_ena = intr_ena;
}

bool killed(struct process *p)
{
	bool is_killed;
	spin_lock_acquire(&p->lock);
	is_killed = p->killed;
	spin_lock_release(&p->lock);
	return is_killed;
}

void set_killed(struct process *p)
{
	spin_lock_acquire(&p->lock);
	p->killed = true;
	spin_lock_release(&p->lock);
}

void yield(void)
{
	struct process *p = running_proc();
	spin_lock_acquire(&p->lock);
	p->state = PROC_RUNNABLE;
	sched();
	spin_lock_release(&p->lock);
}

pid_t fork(void)
{
	pid_t pid;
	int fd;
	struct process *parent, *child;

	child = proc_alloc();
	if (!child)
		return -1;

	parent = running_proc();

	/* copy page table */
	if (copy_user_page_table(child->page_table, parent->page_table,
				 parent->size)) {
		proc_free(child);
		spin_lock_release(&child->lock);
		return -1;
	}
	child->size = parent->size;

	/* copy trap frame */
	memmove(child->tf, parent->tf, sizeof(*(parent->tf)));

	/* the return value of child process */
	child->tf->a0 = 0;

	/* copy all open files */
	for (fd = 0; fd < N_OFILE; fd++)
		if (parent->ofile[fd])
			child->ofile[fd] = file_dup(parent->ofile[fd]);
	child->cwd = idup(parent->cwd);

	/* copy process name */
	strncpy(child->name, parent->name, sizeof(child->name));

	/* the return value of process */
	pid = child->pid;

	spin_lock_release(&child->lock);

	/*
	 * set the parent process of the child process
	 * need to acquire the 'wait lock'
	 */
	spin_lock_acquire(&wait_lock);
	child->parent = parent;
	spin_lock_release(&wait_lock);

	spin_lock_acquire(&child->lock);
	child->state = PROC_RUNNABLE;
	spin_lock_release(&child->lock);

	return pid;
}

int kill(pid_t pid)
{
	struct process *p;
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		spin_lock_acquire(&p->lock);
		if (p->pid == pid) {
			p->killed = true;
			if (p->state == PROC_SLEEPING)
				p->state = PROC_RUNNABLE;
			spin_lock_release(&p->lock);
			return 0;
		}
		spin_lock_release(&p->lock);
	}
	return -1;
}

int wait(uint64_t pstate)
{
	struct process *parent, *child;
	bool have_kids;
	pid_t pid;

	parent = running_proc();
	spin_lock_acquire(&wait_lock);
	while (true) {
		have_kids = false;
		for (child = FIRST_PROC; child <= LAST_PROC; child++) {
			if (child->parent == parent) {
				have_kids = true;
				spin_lock_acquire(&child->lock);
				if (child->state == PROC_ZOMBIE) {
					pid = child->pid;
					if (pstate &&
					    copy_out(parent->page_table, pstate,
						     &child->xstate,
						     sizeof(child->xstate))) {
						spin_lock_release(&child->lock);
						spin_lock_release(&wait_lock);
						return -1;
					}
					proc_free(child);
					spin_lock_release(&child->lock);
					spin_lock_release(&wait_lock);
					return pid;
				}
				spin_lock_release(&child->lock);
			}
		}
		if (!have_kids || killed(parent)) {
			spin_lock_release(&wait_lock);
			return -1;
		}
		sleep_on(parent, &wait_lock);
	}
}

void do_exit(int state)
{
	struct process *p, *child;
	int fd;

	p = running_proc();
	if (p == init_proc)
		panic("'init' exit");

	for (fd = 0; fd < N_OFILE; fd++) {
		if (p->ofile[fd]) {
			file_close(p->ofile[fd]);
			p->ofile[fd] = NULL;
		}
	}
	begin_op();
	iput(p->cwd);
	end_op();
	p->cwd = NULL;

	spin_lock_acquire(&wait_lock);
	for (child = FIRST_PROC; child <= LAST_PROC; child++) {
		if (child->state != PROC_UNUSED && child->parent == p)
			child->parent = init_proc;
	}
	spin_lock_release(&wait_lock);
	wake_up(init_proc);

	spin_lock_acquire(&p->lock);
	p->xstate = state;
	p->state = PROC_ZOMBIE;
	wake_up(p->parent);
	sched();
	panic("zombie process");
}

void sleep_on(void *chan, struct spin_lock *lock)
{
	struct process *p = running_proc();
	spin_lock_acquire(&p->lock);
	spin_lock_release(lock);
	p->chan = chan;
	p->state = PROC_SLEEPING;
	sched();
	p->chan = NULL;
	spin_lock_release(&p->lock);
	spin_lock_acquire(lock);
}

void wake_up(void *chan)
{
	struct process *p, *rp;
	rp = running_proc();
	for (p = FIRST_PROC; p <= LAST_PROC; p++) {
		if (p != rp) {
			spin_lock_acquire(&p->lock);
			if (p->state == PROC_SLEEPING && p->chan == chan)
				p->state = PROC_RUNNABLE;
			spin_lock_release(&p->lock);
		}
	}
}

static uint8_t initcode[] = {
	0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0xc5, 0x02, 0x97, 0x05, 0x00, 0x00,
	0x93, 0x85, 0x65, 0x03, 0x17, 0x06, 0x00, 0x00, 0x13, 0x06, 0xe6, 0x03,
	0x93, 0x08, 0xd0, 0x00, 0x73, 0x00, 0x00, 0x00, 0x93, 0x08, 0x40, 0x00,
	0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
	0x74, 0x00, 0x00, 0x53, 0x48, 0x45, 0x4c, 0x4c, 0x3d, 0x2f, 0x73, 0x68,
	0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00
};

void user_init(void)
{
	struct process *p = proc_alloc();
	void *mem = pm_alloc();
	memset(mem, 0, PAGE_SIZE);
	map_pages(p->page_table, 0, (uint64_t)mem, PAGE_SIZE,
		  PTE_U | PTE_R | PTE_W | PTE_X);
	memmove(mem, initcode, sizeof(initcode));
	p->size = PAGE_SIZE;
	p->tf->epc = 0;
	p->tf->sp = USER_STACK_TOP;
	p->state = PROC_RUNNABLE;
	p->cwd = namei("/");
	init_proc = p;
	strncpy(p->name, "init", sizeof(p->name));
	spin_lock_release(&p->lock);
}
