.text
.file 1 "inserted_by_delocate.c"
.loc 1 1 0
BORINGSSL_bcm_text_start:
	.type foo, @function
	.globl foo
.Lfoo_local_target:
foo:
	movq $0, %rax
	ret

.Lbar_local_target:
bar:

	call	.Lfoo_local_target

	jmp	.Lfoo_local_target

	notrack	jmp	.Lfoo_local_target

	jbe	.Lfoo_local_target

	jne	.Lfoo_local_target

	call	.Lfoo1_local_target

	call	.Lfoo2_local_target

	call	.Lfoo3_local_target

	call	bcm_redirector_memcpy

	jmp	bcm_redirector_memcpy

	notrack	jmp	bcm_redirector_memcpy

	jbe	bcm_redirector_memcpy

	call	.Lfoo_local_target

	jmp	.Lfoo_local_target

	notrack	jmp	.Lfoo_local_target

	jbe	.Lfoo_local_target

.Llocal_label:

	jbe .Llocal_label
	leaq .Llocal_label+2048(%rip), %r14
	leaq .Llocal_label+2048+1024(%rip), %r14

.text
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
	.set	.Lfoo1_local_target, foo
	.equ foo2, foo
	.equ	.Lfoo2_local_target, foo
	.equiv foo3, foo
	.equiv	.Lfoo3_local_target, foo

.Llocal_label_BCM_1:

	jbe	.Llocal_label_BCM_1

	leaq	.Llocal_label_BCM_1+2048(%rip), %r14

	leaq	.Llocal_label_BCM_1+2048+1024(%rip), %r14

.text
.L1_BCM_1:

	.quad 42
.L2_BCM_1:

	.quad	.L2_BCM_1-.L1_BCM_1

	.uleb128	.L2_BCM_1-.L1_BCM_1

	.sleb128	.L2_BCM_1-.L1_BCM_1

	.byte	(.LBB231_40_BCM_1-.LBB231_19_BCM_1)>>2, 4, .Lfoo_BCM_1, (.Lfoo_BCM_1), .Lfoo_BCM_1<<400, (.Lfoo_BCM_1)<<66
.byte   421

	.set	.Llocally_set_symbol1_BCM_1, 1

	.equ	.Llocally_set_symbol2_BCM_1, 2

	.equiv	.Llocally_set_symbol3_BCM_1, 3

	.set	alias_to_local_label, .Llocal_label_BCM_1
	.set	.Lalias_to_local_label_local_target, .Llocal_label_BCM_1

	.equ	alias_to_local_label, .Llocal_label_BCM_1
	.equ	.Lalias_to_local_label_local_target, .Llocal_label_BCM_1

	.equiv	alias_to_local_label, .Llocal_label_BCM_1
	.equiv	.Lalias_to_local_label_local_target, .Llocal_label_BCM_1

	.set	.Llocal_alias_to_local_label_BCM_1, .Llocal_label_BCM_1

	.equ	.Llocal_alias_to_local_label_BCM_1, .Llocal_label_BCM_1

	.equiv	.Llocal_alias_to_local_label_BCM_1, .Llocal_label_BCM_1

	vpcmpneqq	.Llabel_BCM_1(%rip){1to8}, %zmm1, %k0
.text
.loc 1 2 0
BORINGSSL_bcm_text_end:
.type bcm_redirector_memcpy, @function
bcm_redirector_memcpy:
	jmp	memcpy@PLT
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
