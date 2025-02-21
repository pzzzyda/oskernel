#include "trap/trap.h"
#include "dev/plic.h"
#include "dev/timer.h"
#include "dev/uart.h"
#include "dev/virtio_disk.h"
#include "memlayout.h"
#include "printk.h"
#include "riscv.h"
#include "sched/cpu.h"
#include "syscall/syscall.h"

extern void kernel_trap_vector(void);
extern char trampoline[];
extern char return_to_user_space[];
extern char user_trap_vector[];

void trap_init(void)
{
	timer_init();
}

void trap_init_hart(void)
{
	write_stvec((uint64_t)kernel_trap_vector);
	timer_set_next();
	intr_on();
}

static void external_intr(void)
{
	int irq = plic_claim();
	switch (irq) {
	case 0:
		return;
	case UART0_IRQ:
		uart_intr();
		break;
	case VIRTIO0_IRQ:
		virtio_disk_intr();
		break;
	default:
		printk("unexpected interrupt irq: %d\n", irq);
		break;
	}
	plic_complete(irq);
}

void kernel_trap_handler(void)
{
	uint64_t sepc = read_sepc();
	uint64_t sstatus = read_sstatus();
	uint64_t scause = read_scause();

	if ((sstatus & SSTATUS_SPP) == 0)
		panic("the kernel trap is not from S-mode");
	if (intr_get())
		panic("interrupts are enabled when processing interruptions");

	if ((scause & 0x8000000000000000)) { /* interrupts */
		switch (scause) {
		case 0x8000000000000005:
			timer_intr();
			if (running_proc())
				yield();
			break;
		case 0x8000000000000009:
			external_intr();
			break;
		default:
			printk("scause=0x%lx\n", scause);
			panic("kernel trap");
			break;
		}
	} else { /* exception */
		printk("scause=0x%lx\n", scause);
		panic("kernel trap");
	}
	write_sepc(sepc);
	write_sstatus(sstatus);
}

void user_trap_handler(void)
{
	uint64_t scause = read_scause();
	uint64_t sstatus = read_sstatus();
	struct process *p = running_proc();

	if ((sstatus & SSTATUS_SPP) != 0)
		panic("the user trap is not from U-mode");

	write_stvec((uint64_t)kernel_trap_vector);

	p->tf->epc = read_sepc();

	if ((scause & 0x8000000000000000)) { /* interrupts */
		switch (scause) {
		case 0x8000000000000005:
			timer_intr();
			yield();
			break;
		case 0x8000000000000009:
			external_intr();
			break;
		default:
			set_killed(p);
			break;
		}
	} else { /* exception */
		switch (scause) {
		case 8: /* ecall */
			if (killed(p))
				do_exit(1);
			p->tf->epc += 4;
			intr_on();
			syscall();
			break;
		default:
			set_killed(p);
			break;
		}
	}

	if (killed(p))
		do_exit(1);

	user_trap_return();
}

void user_trap_return(void)
{
	struct process *p;
	uint64_t utvec_va, ret_va, satp, x;

	p = running_proc();

	intr_off();

	utvec_va = TRAMPOLINE + (user_trap_vector - trampoline);
	write_stvec(utvec_va);

	p->tf->kernel_satp = read_satp();
	p->tf->kernel_sp = p->kernel_stack + PAGE_SIZE;
	p->tf->kernel_trap = (uint64_t)user_trap_handler;
	p->tf->kernel_hartid = read_tp();

	x = read_sstatus();
	x &= ~SSTATUS_SPP;
	x |= SSTATUS_SPIE;
	write_sstatus(x);

	write_sepc(p->tf->epc);

	satp = MAKE_SATP(p->page_table);

	ret_va = TRAMPOLINE + (return_to_user_space - trampoline);
	/* return_to_user_space(uint64_t user_page_table); */
	((void (*)(uint64_t))ret_va)(satp);
}
