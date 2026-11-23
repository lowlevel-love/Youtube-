
//test専用rb
//io_uringとarrayにおいてarrayの競合のために必要なrb関数
//io_uringはarrayをrbで見てSQEを処理していく
//arrayに格納するためにはスレッドで競合する
//しかし、arrayにも独立したrbが必要
//user_rbで予約してio_uringのtailで確定をする機構によってuser_rb側のheadを削減する。

//実際にはアドレスし変わらないため今回はアドレスをリンクする想定
.extern sq_head
.extern sq_tail
.extern array

.section .hot.text,"ax",@progbits
	.global __set_index
	.type __set_index,@function
__set_index:
	//address load
	movq sq_head(%rip),%r8
	movq sq_tail(%rip),%r9
	movq array(%rip),%r10
	.p2align 6
.loop:
	movl (%rdi),%eax
	movl (%r8),%ecx
	leal 1(%eax),%edx
	//head+max >= tailならだめ
	//MAX_ENTRIES
	addl $128,%ecx
	cmpl %ecx,%eax
	jae .ends
	movl %eax,%ecx
	//mask
	andl $127,%ecx
	lock cmpxchgl %edx,(%rdi)
	jne .loop
	//予約で成功した
	movl %esi,(%r10,%rcx,4)
	lock incl (%r9)
	movl $1,%eax
	ret
.ends:
	//0にする
	xorl %eax,%eax
	ret
.size __set_index,.-__set_index

.section .note.GNU-stack,"",@progbits
