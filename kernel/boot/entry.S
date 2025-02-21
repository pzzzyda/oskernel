.section .text
.global _entry

_entry:
	la sp, boot_stack + 4096
	mv t0, a0
	slli t0, t0, 12
	add sp, sp, t0
	call start

spin:
	wfi
	j spin
