#include "dev/console.h"
#include "dev/plic.h"
#include "dev/virtio_disk.h"
#include "fs/buf.h"
#include "fs/file.h"
#include "fs/inode.h"
#include "mm/mm.h"
#include "printk.h"
#include "sched/cpu.h"
#include "sched/proc.h"
#include "trap/trap.h"

extern char _entry[];

static volatile bool started = false;
static volatile bool first = true;

#define SBI_HSM_EXTENSION 0x48534D /* "HSM" in hex */
#define SBI_HSM_HART_START 0x0

static int hart_start(unsigned long hartid, unsigned long start_addr,
		      unsigned long opaque)
{
	register int ret;
	register unsigned long a7 asm("a7") = SBI_HSM_EXTENSION;
	register unsigned long a6 asm("a6") = SBI_HSM_HART_START;
	register unsigned long a0 asm("a0") = hartid;
	register unsigned long a1 asm("a1") = start_addr;
	register unsigned long a2 asm("a2") = opaque;

	asm volatile("ecall"
		     : "=r"(ret)
		     : "r"(a0), "r"(a1), "r"(a2), "r"(a6), "r"(a7)
		     : "memory");

	return ret;
}

static void wake_up_other_harts(void)
{
	unsigned long id;

	for (id = 0; id < N_CPU; id++) {
		if (id != current_cpuid())
			hart_start(id, (unsigned long)_entry, 0);
	}
}

void main(void)
{
	if (first) {
		first = false;
		console_init();
		printk_init();
		printk("kernel is booting\n");
		printk("hart %d starting\n", current_cpuid());
		pm_init();
		kvm_init();
		kvm_init_hart();
		proc_init();
		trap_init();
		trap_init_hart();
		plic_init();
		plic_init_hart();
		binit();
		iinit();
		file_init();
		virtio_disk_init();
		user_init();
		__sync_synchronize();
		started = true;
		wake_up_other_harts();
	} else {
		while (!started)
			continue;
		__sync_synchronize();
		printk("hart %d starting\n", current_cpuid());
		kvm_init_hart();
		trap_init_hart();
		plic_init_hart();
	}

	scheduler();

	panic("never come back");
}
