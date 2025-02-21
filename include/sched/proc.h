#ifndef _PROC_H
#define _PROC_H

#include "lock.h"
#include "mm/mm.h"
#include "param.h"

struct trap_frame {
	/*   0 */ uint64_t kernel_satp;	  /* Kernel page table */
	/*   8 */ uint64_t kernel_sp;	  /* Top of process's kernel stack */
	/*  16 */ uint64_t kernel_trap;	  /* user_trap() */
	/*  24 */ uint64_t kernel_hartid; /* Saved kernel tp */
	/*  32 */ uint64_t epc;		  /* Saved user program counter */
	/*  40 */ uint64_t ra;
	/*  48 */ uint64_t sp;
	/*  56 */ uint64_t gp;
	/*  64 */ uint64_t tp;
	/*  72 */ uint64_t t0;
	/*  80 */ uint64_t t1;
	/*  88 */ uint64_t t2;
	/*  96 */ uint64_t s0;
	/* 104 */ uint64_t s1;
	/* 112 */ uint64_t a0;
	/* 120 */ uint64_t a1;
	/* 128 */ uint64_t a2;
	/* 136 */ uint64_t a3;
	/* 144 */ uint64_t a4;
	/* 152 */ uint64_t a5;
	/* 160 */ uint64_t a6;
	/* 168 */ uint64_t a7;
	/* 176 */ uint64_t s2;
	/* 184 */ uint64_t s3;
	/* 192 */ uint64_t s4;
	/* 200 */ uint64_t s5;
	/* 208 */ uint64_t s6;
	/* 216 */ uint64_t s7;
	/* 224 */ uint64_t s8;
	/* 232 */ uint64_t s9;
	/* 240 */ uint64_t s10;
	/* 248 */ uint64_t s11;
	/* 256 */ uint64_t t3;
	/* 264 */ uint64_t t4;
	/* 272 */ uint64_t t5;
	/* 280 */ uint64_t t6;
};

/* Saved registers for kernel context switches. */
struct context {
	uint64_t ra;
	uint64_t sp;

	/* callee-saved */
	uint64_t s0;
	uint64_t s1;
	uint64_t s2;
	uint64_t s3;
	uint64_t s4;
	uint64_t s5;
	uint64_t s6;
	uint64_t s7;
	uint64_t s8;
	uint64_t s9;
	uint64_t s10;
	uint64_t s11;
};

#define PROC_UNUSED 0
#define PROC_USED 1
#define PROC_RUNNABLE 2
#define PROC_RUNNING 3
#define PROC_SLEEPING 4
#define PROC_ZOMBIE 5

struct m_inode;
struct file;

struct process {
	struct spin_lock lock;

	/* p->lock must be held when using these: */
	int state;   /* Process state */
	void *chan;  /* If not NULL, sleeping on chan */
	bool killed; /* If true, have been killed */
	int xstate;  /* Exit state to be returned to parent's wait */
	pid_t pid;   /* Process ID */

	/* wait_lock must be held when using these: */
	struct process *parent; /* Parent process */

	/* These are private fields */
	uint64_t kernel_stack;	     /* Virtual address of kernel stack */
	uint64_t size;		     /* Size of process memory */
	pte_t *page_table;	     /* User page table */
	struct trap_frame *tf;	     /* Data page for trampoline.S */
	struct context ctx;	     /* context_switch() here to run process */
	struct file *ofile[N_OFILE]; /* Open files */
	struct m_inode *cwd;	     /* Current directory */
	char name[20];		     /* Process name */
};

void user_init(void);
void proc_init(void);
struct process *proc_alloc(void);
void proc_free(struct process *p);
int proc_grow(uint64_t size);
void proc_dump(void);
void scheduler(void);
void sched(void);
bool killed(struct process *p);
void set_killed(struct process *p);
void yield(void);
pid_t fork(void);
int kill(pid_t pid);
int wait(uint64_t state);
void do_exit(int state);
void sleep_on(void *chan, struct spin_lock *lock);
void wake_up(void *chan);

#endif
