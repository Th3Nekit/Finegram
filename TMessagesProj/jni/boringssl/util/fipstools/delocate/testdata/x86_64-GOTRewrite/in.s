	.text
foo:
bar:

	pushq stderr@GOTPCREL(%rip)
	pushq foo@GOTPCREL(%rip)

	movq stderr@GOTPCREL(%rip), %r11
	movq foo@GOTPCREL(%rip), %r11

	vmovq stderr@GOTPCREL(%rip), %xmm0
	vmovq foo@GOTPCREL(%rip), %xmm0

	cmoveq stderr@GOTPCREL(%rip), %r11
	cmoveq foo@GOTPCREL(%rip), %r11
	cmovneq stderr@GOTPCREL(%rip), %r11
	cmovneq foo@GOTPCREL(%rip), %r11

	movsd foo@GOTPCREL(%rip), %xmm0
	vmovsd foo@GOTPCREL(%rip), %xmm0

	movsd

	movq BORINGSSL_bcm_text_start@GOTPCREL(%rip), %r11
	movq foobar_bss_get@GOTPCREL(%rip), %r11

	vpbroadcastq stderr@GOTPCREL(%rip), %xmm0
	vpbroadcastq foo@GOTPCREL(%rip), %xmm0

	movq gcm_gmult_clmul@GOTPCREL(%rip), %xmm0
	movhps gcm_ghash_clmul@GOTPCREL(%rip), %xmm0
	movaps %xmm0, (%rsp)

	movhps gcm_ghash_clmul@GOTPCREL(%rip), %xmm0
	movlps gcm_gmult_clmul@GOTPCREL(%rip), %xmm0
	movaps %xmm0, (%rsp)

	movhps foo@GOTPCREL(%rip), %xmm0
	movlps bar@GOTPCREL(%rip), %xmm0
	movaps %xmm0, (%rsp)

	cmpq foo@GOTPCREL(%rip), %rax
	cmpq %rax, foo@GOTPCREL(%rip)

	leaq _GLOBAL_OFFSET_TABLE_(%rip), %rcx

.comm foobar,64,32
