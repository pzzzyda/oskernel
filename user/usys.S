#include "syscall/sysnum.h"

.global fork
fork:
	li a7, SYS_fork
	ecall
	ret

.global read
read:
	li a7, SYS_read
	ecall
	ret

.global write
write:
	li a7, SYS_write
	ecall
	ret

.global open
open:
	li a7, SYS_open
	ecall
	ret

.global close
close:
	li a7, SYS_close
	ecall
	ret

.global wait
wait:
	li a7, SYS_wait
	ecall
	ret

.global exit
exit:
	li a7, SYS_exit
	ecall
	ret

.global execve
execve:
	li a7, SYS_execve
	ecall
	ret

.global fstat
fstat:
	li a7, SYS_fstat
	ecall
	ret

.global mkdir
mkdir:
	li a7, SYS_mkdir
	ecall
	ret

.global getpid
getpid:
	li a7, SYS_getpid
	ecall
	ret

.global brk
brk:
	li a7, SYS_brk
	ecall
	ret

.global chdir
chdir:
	li a7, SYS_chdir
	ecall
	ret

.global getppid
getppid:
	li a7, SYS_getppid
	ecall
	ret

.global mknod
mknod:
	li a7, SYS_mknod
	ecall
	ret

.global dup
dup:
	li a7, SYS_dup
	ecall
	ret

.global kill
kill:
	li a7, SYS_kill
	ecall
	ret

.global sleep
sleep:
	li a7, SYS_sleep
	ecall
	ret

.global sbrk
sbrk:
	li a7, SYS_sbrk
	ecall
	ret

.global pipe
pipe:
	li a7, SYS_pipe
	ecall
	ret

.global link
link:
	li a7, SYS_link
	ecall
	ret

.global unlink
unlink:
	li a7, SYS_unlink
	ecall
	ret

.global shutdown
shutdown:
	li a7, SYS_shutdown
	ecall
	ret

.global lseek
lseek:
	li a7, SYS_lseek
	ecall
	ret

.global dup2
dup2:
	li a7, SYS_dup2
	ecall
	ret
