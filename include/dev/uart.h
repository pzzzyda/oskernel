#ifndef _UART_H
#define _UART_H

void uart_init(void);
void uart_intr(void);
void uart_putc(int c);
void uart_putc_sync(int c);
int uart_getc(void);

#endif
