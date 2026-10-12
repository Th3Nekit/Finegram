	.type foo, @function
	.globl foo
foo:
	movq $0, %rax
	ret

bar:

	call foo
	jmp foo
	notrack jmp foo
	jbe foo
	jne foo

	call foo1
	call foo2
	call foo3

	call memcpy@PLT
	jmp memcpy@PLT
	notrack jmp memcpy@PLT
	jbe memcpy@PLT

	call foo@PLT
	jmp foo@PLT
	notrack jmp foo@PLT
	jbe foo@PLT

.Llocal_label:
	jbe .Llocal_label
	leaq .Llocal_label+2048(%rip), %r14
	leaq .Llocal_label+2048+1024(%rip), %r14

	.section .rodata
.L1:
	.quad 42
.L2:
	.quad .L2-.L1
	.uleb128 .L2-.L1
	.sleb128 .L2-.L1

	.text
	jmp 1f
1:
	jmp 1b
2:

	.quad 2b - 1b
	.quad 2b - .L2

	.globl foo1
	.globl foo2
	.globl foo3
	.set foo1, foo
	.equ foo2, foo
	.equiv foo3, foo
