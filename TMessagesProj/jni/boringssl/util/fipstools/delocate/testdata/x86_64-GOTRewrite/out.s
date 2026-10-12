.text
.file 1 "inserted_by_delocate.c"
.loc 1 1 0
BORINGSSL_bcm_text_start:
	.text
.Lfoo_local_target:
foo:
.Lbar_local_target:
bar:

	pushq %rax
	leaq -128(%rsp), %rsp
	pushf
	leaq stderr_GOTPCREL_external(%rip), %rax
	addq (%rax), %rax
	movq (%rax), %rax
	popf
	leaq	128(%rsp), %rsp
	xchg %rax, (%rsp)

	pushq %rax
	leaq	.Lfoo_local_target(%rip), %rax
	xchg %rax, (%rsp)

	leaq -128(%rsp), %rsp
	pushf
	leaq stderr_GOTPCREL_external(%rip), %r11
	addq (%r11), %r11
	movq (%r11), %r11
	popf
	leaq	128(%rsp), %rsp

	leaq	.Lfoo_local_target(%rip), %r11

	leaq -128(%rsp), %rsp
	pushq %rax
	pushf
	leaq stderr_GOTPCREL_external(%rip), %rax
	addq (%rax), %rax
	movq (%rax), %rax
	popf
	vmovq %rax, %xmm0
	popq %rax
	leaq 128(%rsp), %rsp

	leaq -128(%rsp), %rsp
	pushq %rax
	leaq	.Lfoo_local_target(%rip), %rax
	vmovq %rax, %xmm0
	popq %rax
	leaq 128(%rsp), %rsp

	jne 999f
	leaq -128(%rsp), %rsp
	pushf
	leaq stderr_GOTPCREL_external(%rip), %r11
	addq (%r11), %r11
	movq (%r11), %r11
	popf
	leaq	128(%rsp), %rsp
999:

	jne 999f
	leaq	.Lfoo_local_target(%rip), %r11
999:

	je 999f
	leaq -128(%rsp), %rsp
	pushf
	leaq stderr_GOTPCREL_external(%rip), %r11
	addq (%r11), %r11
	movq (%r11), %r11
	popf
	leaq	128(%rsp), %rsp
999:

	je 999f
	leaq	.Lfoo_local_target(%rip), %r11
999:

	leaq -128(%rsp), %rsp
	pushq %rax
	leaq	.Lfoo_local_target(%rip), %rax
	movq %rax, %xmm0
	popq %rax
	leaq 128(%rsp), %rsp

	leaq -128(%rsp), %rsp
	pushq %rax
	leaq	.Lfoo_local_target(%rip), %rax
	vmovq %rax, %xmm0
	popq %rax
	leaq 128(%rsp), %rsp

	movsd

	leaq	BORINGSSL_bcm_text_start(%rip), %r11

	leaq	foobar_bss_get(%rip), %r11

	leaq -128(%rsp), %rsp
	pushq %rax
	pushf
	leaq stderr_GOTPCREL_external(%rip), %rax
	addq (%rax), %rax
	movq (%rax), %rax
	popf
	vmovq %rax, %xmm0
	popq %rax
	leaq 128(%rsp), %rsp
	vpbroadcastq %xmm0, %xmm0

	leaq -128(%rsp), %rsp
	pushq %rax
	leaq	.Lfoo_local_target(%rip), %rax
	vmovq %rax, %xmm0
	popq %rax
	leaq 128(%rsp), %rsp
	vpbroadcastq %xmm0, %xmm0

	leaq -128(%rsp), %rsp
	pushq %rax
	pushf
	leaq gcm_gmult_clmul_GOTPCREL_external(%rip), %rax
	addq (%rax), %rax
	movq (%rax), %rax
	popf
	movq %rax, %xmm0
	popq %rax
	leaq 128(%rsp), %rsp

	leaq -128(%rsp), %rsp
	pushq %rax
	pushf
	leaq gcm_ghash_clmul_GOTPCREL_external(%rip), %rax
	addq (%rax), %rax
	movq (%rax), %rax
	popf
	pushq %rax
	movhps (%rsp), %xmm0
	leaq 8(%rsp), %rsp
	popq %rax
	leaq 128(%rsp), %rsp
	movaps %xmm0, (%rsp)

	leaq -128(%rsp), %rsp
	pushq %rax
	pushf
	leaq gcm_ghash_clmul_GOTPCREL_external(%rip), %rax
	addq (%rax), %rax
	movq (%rax), %rax
	popf
	pushq %rax
	movhps (%rsp), %xmm0
	leaq 8(%rsp), %rsp
	popq %rax
	leaq 128(%rsp), %rsp

	leaq -128(%rsp), %rsp
	pushq %rax
	pushf
	leaq gcm_gmult_clmul_GOTPCREL_external(%rip), %rax
	addq (%rax), %rax
	movq (%rax), %rax
	popf
	pushq %rax
	movlps (%rsp), %xmm0
	leaq 8(%rsp), %rsp
	popq %rax
	leaq 128(%rsp), %rsp
	movaps %xmm0, (%rsp)

	leaq -128(%rsp), %rsp
	pushq %rax
	leaq	.Lfoo_local_target(%rip), %rax
	pushq %rax
	movhps (%rsp), %xmm0
	leaq 8(%rsp), %rsp
	popq %rax
	leaq 128(%rsp), %rsp

	leaq -128(%rsp), %rsp
	pushq %rax
	leaq	.Lbar_local_target(%rip), %rax
	pushq %rax
	movlps (%rsp), %xmm0
	leaq 8(%rsp), %rsp
	popq %rax
	leaq 128(%rsp), %rsp
	movaps %xmm0, (%rsp)

	leaq -128(%rsp), %rsp
	pushq %rbx
	leaq	.Lfoo_local_target(%rip), %rbx
	cmpq %rbx, %rax
	popq %rbx
	leaq 128(%rsp), %rsp

	leaq -128(%rsp), %rsp
	pushq %rbx
	leaq	.Lfoo_local_target(%rip), %rbx
	cmpq %rax, %rbx
	popq %rbx
	leaq 128(%rsp), %rsp

	leaq	.Lboringssl_got_delta(%rip), %rcx
	addq .Lboringssl_got_delta(%rip), %rcx

.comm foobar,64,32
.text
.loc 1 2 0
BORINGSSL_bcm_text_end:
.type foobar_bss_get, @function
foobar_bss_get:
	leaq	foobar(%rip), %rax
	ret
.type gcm_ghash_clmul_GOTPCREL_external, @object
.size gcm_ghash_clmul_GOTPCREL_external, 8
gcm_ghash_clmul_GOTPCREL_external:
	.long gcm_ghash_clmul@GOTPCREL
	.long 0
.type gcm_gmult_clmul_GOTPCREL_external, @object
.size gcm_gmult_clmul_GOTPCREL_external, 8
gcm_gmult_clmul_GOTPCREL_external:
	.long gcm_gmult_clmul@GOTPCREL
	.long 0
.type stderr_GOTPCREL_external, @object
.size stderr_GOTPCREL_external, 8
stderr_GOTPCREL_external:
	.long stderr@GOTPCREL
	.long 0
.Lboringssl_got_delta:
	.quad _GLOBAL_OFFSET_TABLE_-.Lboringssl_got_delta
.type BORINGSSL_bcm_text_hash, @object
.size BORINGSSL_bcm_text_hash, 32
BORINGSSL_bcm_text_hash:
.byte 0xae
.byte 0x2c
.byte 0xea
.byte 0x2a
.byte 0xbd
.byte 0xa6
.byte 0xf3
.byte 0xec
.byte 0x97
.byte 0x7f
.byte 0x9b
.byte 0xf6
.byte 0x94
.byte 0x9a
.byte 0xfc
.byte 0x83
.byte 0x68
.byte 0x27
.byte 0xcb
.byte 0xa0
.byte 0xa0
.byte 0x9f
.byte 0x6b
.byte 0x6f
.byte 0xde
.byte 0x52
.byte 0xcd
.byte 0xe2
.byte 0xcd
.byte 0xff
.byte 0x31
.byte 0x80
