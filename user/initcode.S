# Initial process that execs /init.
# This code runs in user space.

#include "syscall/sysnum.h"

# execve(init, argv, env)
.globl start
start:
	la a0, init
	la a1, argv
	la a2, env
	li a7, SYS_execve
	ecall

# for(;;) exit();
exit:
	li a7, SYS_exit
	ecall
	jal exit

# char init[] = "/init\0";
init:
	.string "/init\0"

# char sh[] = "SHELL=/sh\0";
sh:
	.string "SHELL=/sh\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
	.quad init
	.quad 0

# char *env[] = { sh, 0 };
.p2align 2
env:
	.quad sh
	.quad 0
