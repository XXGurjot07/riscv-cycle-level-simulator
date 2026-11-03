# Spike startup: call main, then exit through HTIF tohost with exit code = main's return value
.section .text.init
.globl _start

_start:
	la   sp, _stack_top
	call main
	slli a0, a0, 1          # tohost = (code << 1) | 1  ->  spike exits with 'code'
	ori  a0, a0, 1
	la   t0, tohost
	sd   a0, 0(t0)
1:	j    1b

.section .tohost, "aw", @progbits
.align 6
.globl tohost
tohost:   .dword 0
.align 6
.globl fromhost
fromhost: .dword 0
