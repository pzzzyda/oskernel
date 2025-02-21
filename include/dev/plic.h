#ifndef _PLIC_H
#define _PLIC_H

void plic_init(void);
void plic_init_hart(void);
int plic_claim(void);
void plic_complete(int irq);

#endif
