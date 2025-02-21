#include "dev/plic.h"
#include "memlayout.h"
#include "sched/cpu.h"

void plic_init(void)
{
	/* Set desired IRQ priorities non-zero (otherwise disabled). */
	*(uint32_t *)(PLIC + UART0_IRQ * 4) = 1;
	*(uint32_t *)(PLIC + VIRTIO0_IRQ * 4) = 1;
}

void plic_init_hart(void)
{
	int hart = current_cpuid();

	/*
	 * Set enable bits for this hart's S-mode
	 * for the uart and virtio disk.
	 */
	*(uint32_t *)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

	/* Set this hart's S-mode priority threshold to 0. */
	*(uint32_t *)PLIC_SPRIORITY(hart) = 0;
}

/* Ask the PLIC what interrupt we should serve. */
int plic_claim(void)
{
	int hart = current_cpuid();
	int irq = *(uint32_t *)PLIC_SCLAIM(hart);
	return irq;
}

/* Tell the PLIC we've served this IRQ. */
void plic_complete(int irq)
{
	int hart = current_cpuid();
	*(uint32_t *)PLIC_SCLAIM(hart) = irq;
}
