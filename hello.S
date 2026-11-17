.section .rodata,"a",@progbits
_hello:
	.ascii "Hello World!!\n"
.size _hello,.-_hello

.section .text,"ax",@progbits
	.global _start
	.type _start,@function
_start:
	movl $1,%eax
	movl $1,%edi
	leaq _hello(%rip),%rsi
	movl $14,%edx
	syscall
	movl $0x3c,%eax
	syscall
	ret

.size _start,.-_start
.section .note.GNU-stack,"",@progbits
