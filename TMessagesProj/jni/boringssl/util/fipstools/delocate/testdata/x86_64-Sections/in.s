	.text
	movq %rax, %rax

	.section .text.foo,"ax",@progbits
	.globl foo
foo:
	ret

	.section .rodata
	.long 42
	.string "Hello world, esc\ape characters are \"fun\"\\"

	.section .rodata.str1.1,"aMS",@progbits,1
	.string "NIST P-256"
	.text

	.section	.init_array,"aw"
	.align 8
	.quad	foo
	.section	.rodata
	.align 16
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x1b35e
	.value	0x4
	.long	.L1
	.byte	0x8
	.uleb128 0x1
	.long	.L2
	.byte	0x1
	.long	.L3
