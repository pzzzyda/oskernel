#include "dev/uart.h"
#include "dev/console.h"
#include "lock.h"
#include "memlayout.h"
#include "printk.h"
#include "sched/cpu.h"

#define RHR 0 /* Receiver Holding Register */
#define THR 0 /* Transmitter Holding Register */
#define DLL 0 /* Divisor Latch Register */
#define IER 1 /* Interrupt Enable Register */
#define IER_RX_ENABLE (1 << 0)
#define IER_TX_ENABLE (1 << 1)
#define DLM 1 /* Divisor Latch Register */
#define FCR 2 /* FIFO Control Register */
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)
#define ISR 2 /* Interrupt Status Register */
#define LCR 3 /* Line Control Register */
#define LCR_EIGHT_BITS (3 << 0)
#define LCR_BAUD_LATCH (1 << 7)
#define MCR 4 /* Modem Control Register */
#define LSR 5 /* Line Status Register */
#define LSR_RX_READY (1 << 0)
#define LSR_TX_IDLE (1 << 5)
#define MSR 6 /* Modem Status Register */
#define SPR 7 /* ScratchPad Register */

#define REG(r) ((volatile uint8_t *)(UART0 + (r)))
#define READ_REG(r) (*(REG(r)))
#define WRITE_REG(r, v) (*(REG(r)) = (v))

struct uart_tx {
	struct spin_lock lock;
	uint32_t r;
	uint32_t w;
#define UART_TX_BUF_SIZE 32
	char buf[UART_TX_BUF_SIZE];
};

extern bool panicked;
static struct uart_tx tx;

static void uart_start(void)
{
	int c;

	while (true) {
		if (tx.w == tx.r)
			break;

		/*
		 * the UART transmit holding register is full, so we cannot give
		 * it another byte. it will interrupt when it's ready for a new
		 * byte.
		 */
		if (!(READ_REG(LSR) & LSR_TX_IDLE))
			break;

		c = tx.buf[tx.r];
		tx.r = (tx.r + 1) % UART_TX_BUF_SIZE;

		/* maybe uart_putc() is waiting for space in the buffer. */
		wake_up(&tx.r);

		WRITE_REG(THR, c);
	}
}

void uart_init(void)
{
	/* Disable interrupts. */
	WRITE_REG(IER, 0x00);

	/* Special mode to set baud rate. */
	WRITE_REG(LCR, LCR_BAUD_LATCH);

	/* LSB for baud rate of 38.4K */
	WRITE_REG(DLL, 0x03);

	/* MSB for baud rate of 48.4K */
	WRITE_REG(DLM, 0x00);

	/* Leave set-baud mode,
	 * and set word length to 8 bits, no parity */
	WRITE_REG(LCR, LCR_EIGHT_BITS);

	/* Reset and enable FIFOs */
	WRITE_REG(FCR, FCR_FIFO_CLEAR | FCR_FIFO_ENABLE);

	/* Enable transmit and receive interrupts */
	WRITE_REG(IER, LSR_RX_READY | LSR_TX_IDLE);

	spin_lock_init(&tx.lock, "uart");
}

void uart_putc_sync(int c)
{
	push_off();

	if (panicked)
		while (true)
			continue;

	while (!(READ_REG(LSR) & LSR_TX_IDLE))
		continue;

	WRITE_REG(THR, c);

	pop_off();
}

void uart_putc(int c)
{
	spin_lock_acquire(&tx.lock);

	if (panicked)
		while (true)
			continue;

	/*
	 * buffer is full.
	 * wait for uart_start() to open up space in the buffer.
	 */
	while ((tx.w + 1) % UART_TX_BUF_SIZE == tx.r)
		sleep_on(&tx.r, &tx.lock);

	tx.buf[tx.w] = c;
	tx.w = (tx.w + 1) % UART_TX_BUF_SIZE;
	uart_start();

	spin_lock_release(&tx.lock);
}

int uart_getc(void)
{
	if ((READ_REG(LSR) & LSR_RX_READY))
		return READ_REG(RHR);
	else
		return -1;
}

void uart_intr(void)
{
	int c;

	while (true) {
		c = uart_getc();
		if (c == -1)
			break;
		console_intr(c);
	}

	spin_lock_acquire(&tx.lock);
	uart_start();
	spin_lock_release(&tx.lock);
}
