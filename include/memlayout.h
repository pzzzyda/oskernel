#ifndef _MEMLAYOUT_H
#define _MEMLAYOUT_H

/*
 * qemu -machine virt is set up like this,
 * based on qemu's hw/riscv/virt.c:
 *
 * 00001000 -- boot ROM, provided by qemu
 * 02000000 -- CLINT
 * 0C000000 -- PLIC
 * 10000000 -- uart0
 * 10001000 -- virtio disk
 * 80000000 -- boot ROM jumps here in machine mode
 *             -kernel loads the kernel here
 * unused RAM after 80000000.
 */

#define MAX_PADDR 0x88000000ul

#define KERNEL_START 0x80200000ul

#define UART0 0x10000000L
#define UART0_IRQ 10

#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart) * 0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart) * 0x2000)

#define MAX_VADDR (1ul << (9 + 9 + 9 + 12 - 1))

#define TRAMPOLINE (MAX_VADDR - PAGE_SIZE)

#define KERNEL_STACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PAGE_SIZE)

#define TRAP_FRAME (TRAMPOLINE - PAGE_SIZE)

#define USER_STACK_SIZE PAGE_SIZE
#define USER_STACK_TOP (TRAP_FRAME - PAGE_SIZE)
#define USER_STACK_BASE (USER_STACK_TOP - USER_STACK_SIZE)

/*
 *   User Space Memory Layout
 *
 * --------------------------- MAX_VADDR
 *         trampoline
 * --------------------------- MAX_VADDR - 1 * PAGE_SIZE
 *         trap frame
 * --------------------------- MAX_VADDR - 2 * PAGE_SIZE
 *       protected pages
 * --------------------------- MAX_VADDR - 3 * PAGE_SIZE
 * //////// user stack ///////
 * --------------------------- MAX_VADDR - 4 * PAGE_SIZE
 *       protected pages
 * --------------------------- MAX_VADDR - 5 * PAGE_SIZE
 *            ...
 *            ...
 * --------------------------- p->size
 * ///////////////////////////
 * //////// user heap ////////
 * ///////////////////////////
 * ---------------------------
 * ////// text and data //////
 * ---------------------------
 */

#endif
