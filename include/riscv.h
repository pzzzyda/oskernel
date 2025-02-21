#ifndef _RISCV_H
#define _RISCV_H

#ifndef __ASSEMBLER__

#include "types.h"

static inline uint64_t read_tp(void)
{
	uint64_t v;
	asm volatile("mv %0, tp" : "=r"(v));
	return v;
}

static inline void write_tp(uint64_t v)
{
	asm volatile("mv tp, %0" : : "r"(v));
}

#define SSTATUS_SPP (1 << 8)  /* Previous mode, 1=Supervisor, 0=User */
#define SSTATUS_SPIE (1 << 5) /* Supervisor Previous Interrupt Enable */
#define SSTATUS_UPIE (1 << 4) /* User Previous Interrupt Enable */
#define SSTATUS_SIE (1 << 1)  /* Supervisor Interrupt Enable */
#define SSTATUS_UIE (1 << 0)  /* User Interrupt Enable */

static inline uint64_t read_sstatus(void)
{
	uint64_t v;
	asm volatile("csrr %0, sstatus" : "=r"(v));
	return v;
}

static inline void write_sstatus(uint64_t v)
{
	asm volatile("csrw sstatus, %0" : : "r"(v));
}

static inline uint64_t read_satp(void)
{
	uint64_t v;
	asm volatile("csrr %0, satp" : "=r"(v));
	return v;
}

#define SATP_SV39 (8ul << 60)
#define MAKE_SATP(page_table) (SATP_SV39 | (((uint64_t)(page_table)) >> 12))

static inline void write_satp(uint64_t v)
{
	asm volatile("csrw satp, %0" : : "r"(v));
}

static inline void sfence_vma(void)
{
	asm volatile("sfence.vma zero, zero");
}

static inline uint64_t read_stvec(void)
{
	uint64_t v;
	asm volatile("csrr %0, stvec" : "=r"(v));
	return v;
}

static inline void write_stvec(uint64_t v)
{
	asm volatile("csrw stvec, %0" : : "r"(v));
}

static inline uint64_t read_scause(void)
{
	uint64_t v;
	asm volatile("csrr %0, scause" : "=r"(v));
	return v;
}

static inline uint64_t read_sepc(void)
{
	uint64_t v;
	asm volatile("csrr %0, sepc" : "=r"(v));
	return v;
}

static inline void write_sepc(uint64_t v)
{
	asm volatile("csrw sepc, %0" : : "r"(v));
}

static inline uint64_t read_stval(void)
{
	uint64_t v;
	asm volatile("csrr %0, stval" : "=r"(v));
	return v;
}

#define SIE_SEIE (1L << 9) /* External */
#define SIE_STIE (1L << 5) /* Timer */
#define SIE_SSIE (1L << 1) /* Software */

static inline uint64_t read_sie(void)
{
	uint64_t v;
	asm volatile("csrr %0, sie" : "=r"(v));
	return v;
}

static inline void write_sie(uint64_t v)
{
	asm volatile("csrw sie, %0" : : "r"(v));
}

static inline void intr_on(void)
{
	write_sstatus(read_sstatus() | SSTATUS_SIE);
}

static inline void intr_off(void)
{
	write_sstatus(read_sstatus() & ~SSTATUS_SIE);
}

static inline bool intr_get(void)
{
	uint64_t v = read_sstatus();
	return (v & SSTATUS_SIE) != 0;
}

static inline uint64_t read_time(void)
{
	uint64_t v;
	asm volatile("csrr %0, time" : "=r"(v));
	return v;
}

#endif /* __ASSEMBLER__ */

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define PAGE_ROUND_UP(n) (((n) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ROUND_DOWN(n) (((n)) & ~(PAGE_SIZE - 1))

#define PXMASK 0x1fful
#define PXSHIFT(level) (PAGE_SHIFT + (9 * (level)))
#define PX(va, level) ((((uint64_t)(va)) >> PXSHIFT(level)) & PXMASK)

#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PA2PTE(pa) (((uint64_t)(pa) >> 12) << 10)

#define PTE_FLAGS(pte) ((pte) & 0x3fful)

#define PTE_V (1ul << 0) /* Valid */
#define PTE_R (1ul << 1) /* Read */
#define PTE_W (1ul << 2) /* Write */
#define PTE_X (1ul << 3) /* Execute */
#define PTE_U (1ul << 4) /* User */
#define PTE_G (1ul << 5) /* Global*/
#define PTE_A (1ul << 6) /* Accessed */
#define PTE_D (1ul << 7) /* Dirty */

#endif
