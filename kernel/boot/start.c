#include "param.h"
#include "riscv.h"

extern void main(void);

__attribute__((aligned(16))) uint8_t boot_stack[4096 * N_CPU];

void start(uint64_t hartid, uint64_t dbt_entry)
{
	write_satp(0);
	write_tp(hartid);
	write_sie(read_sie() | SIE_SEIE | SIE_SSIE | SIE_STIE);
	main();
}
