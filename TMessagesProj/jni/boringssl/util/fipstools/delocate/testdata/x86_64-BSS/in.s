	.text
	movq %rax, %rax

	.comm	aes_128_ctr_generic_storage,64,32
	.lcomm	aes_128_ctr_generic_storage2,64,32

	.section .bss,"awT",@nobits
	.align 4
	.globl x
	.type   x, @object
	.size   x, 4
x:
	.zero 4
.Llocal:
	.quad 0
	.size .Llocal, 4

	.text
not_bss1:
	ret

	.bss
test:
	.quad 0
	.text
not_bss2:
	ret

	.section .bss,"awT",@nobits
y:
	.quad 0

	.section .rodata
	.quad 0

	.section .bss,"awT",@nobits
z:
	.quad 0
