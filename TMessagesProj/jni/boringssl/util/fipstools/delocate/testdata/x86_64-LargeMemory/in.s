	.text

.L0:
        leaq    .L0(%rip), %rax
        movabsq $_GLOBAL_OFFSET_TABLE_-.L0, %rcx
        addq    %rax, %rcx
        movabsq $_Z1gv@GOTOFF, %rax
        addq    %rcx, %rax
        jmpq    *%rax

.L0$pb:
        leaq    .L0$pb(%rip), %rax
        movabsq $_GLOBAL_OFFSET_TABLE_-.L0$pb, %rcx
        addq    %rax, %rcx
        movabsq $h@GOT, %rax
        movq    (%rcx,%rax), %rax
        movl    (%rax), %eax
        retq

        #
