#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_ARM) && defined(__ELF__)

.fpu neon
.text

.align 4
.global openssl_poly1305_neon2_blocks
.hidden openssl_poly1305_neon2_blocks
.type openssl_poly1305_neon2_blocks STT_FUNC
openssl_poly1305_neon2_blocks:
vpush {q4,q5,q6,q7}
mov r12,sp
sub sp,sp,#192
bic sp,sp,#31

mov r3,r3

vld1.8 {d0},[r1]!

vld1.8 {d2-d3},[r1]!

vld1.8 {d4-d5},[r1]!

add r1,r1,#8

vld1.8 {d6},[r1]!

vld1.8 {d8-d9},[r1]!

vld1.8 {d10-d11},[r1]!

vmov.i64 q6,#0xffffffff

vmov.i64 q7,#0xff

vld1.8 {d16-d17},[r0,: 128]!

vld1.8 {d18-d19},[r0,: 128]!

vld1.8 {d20},[r0,: 64]

sub r0,r0,#32

vshr.u64 q6,q6,#6

vshr.u64 q7,q7,#7

vshl.i32 q11,q1,#2

vshl.i32 q12,q2,#2

vadd.i32 q11,q11,q1

vadd.i32 q12,q12,q2

vshl.i64 q7,q7,#24

vshl.i32 q13,q4,#2

vshl.i32 q14,q5,#2

vadd.i32 q13,q13,q4

vadd.i32 q14,q14,q5

add r1,sp,#0

vst1.8 {d14-d15},[r1,: 128]

vmov q15,q7

vmov q7,q7

add r1,sp,#16

vst1.8 {d0-d1},[r1,: 128]

add r1,sp,#32

vst1.8 {d2-d3},[r1,: 128]

add r1,sp,#48

vst1.8 {d4-d5},[r1,: 128]

add r1,sp,#96

vst1.8 {d6-d7},[r1,: 128]

add r1,sp,#112

vst1.8 {d8-d9},[r1,: 128]

add r1,sp,#128

vst1.8 {d10-d11},[r1,: 128]

add r1,sp,#64

vst1.8 {d22-d23},[r1,: 128]

add r1,sp,#80

vst1.8 {d24-d25},[r1,: 128]

add r1,sp,#144

vst1.8 {d26-d27},[r1,: 128]

add r1,sp,#160

vst1.8 {d28-d29},[r1,: 128]

cmp r3,#64

bls ._below64bytes

add r1,r2,#32

._mainloop2:

vld1.8 {d0-d1},[r1]!

vld1.8 {d2-d3},[r1]!

vmlal.u32 q15,d16,d11

add r2,sp,#112

vld1.8 {d4-d5},[r2,: 128]

vmlal.u32 q15,d17,d10

add r2,sp,#96

vld1.8 {d6-d7},[r2,: 128]

vmlal.u32 q15,d18,d5

vtrn.32 d1,d3

vmlal.u32 q15,d19,d4

vmlal.u32 q15,d20,d6

vshll.u32 q4,d3,#18

vtrn.32 d0,d2

vmlal.u32 q4,d16,d10

vmlal.u32 q4,d17,d5

vext.32 d14,d14,d0,#1

vmlal.u32 q4,d18,d4

sub r1,r1,#64

vmlal.u32 q4,d19,d6

add r2,sp,#160

vld1.8 {d10-d11},[r2,: 128]

vmlal.u32 q4,d20,d11

vrev64.i32 q7,q7

vshll.u32 q13,d1,#12

vld1.8 {d22-d23},[r1]!

vmlal.u32 q13,d16,d5

vmlal.u32 q13,d17,d4

vmlal.u32 q13,d18,d6

vmlal.u32 q13,d19,d11

vmlal.u32 q13,d20,d10

vext.32 d15,d0,d15,#1

vshll.u32 q14,d2,#6

vmlal.u32 q14,d16,d4

vmlal.u32 q14,d17,d6

vmlal.u32 q14,d18,d11

vmlal.u32 q14,d19,d10

add r2,sp,#144

vld1.8 {d0-d1},[r2,: 128]

vmlal.u32 q14,d20,d1

vld1.8 {d2-d3},[r1]!

add r1,r1,#32

vmlal.u32 q7,d20,d0

vmlal.u32 q7,d18,d10

vswp d2,d23

vmlal.u32 q7,d19,d1

vmlal.u32 q7,d16,d6

vshr.u64 q3,q1,#40

vext.32 d0,d22,d2,#1

vshrn.u64 d19,q1,#14

vext.32 d1,d23,d3,#1

vshrn.u64 d21,q11,#26

vext.32 d20,d22,d22,#1

vmlal.u32 q7,d17,d11

vext.32 d20,d20,d23,#1

vshrn.u64 d18,q0,#20

vtrn.32 d6,d7

vand.i32 q10,#0x03ffffff

add r2,sp,#48

vld1.8 {d4-d5},[r2,: 128]

vand.i32 q9,#0x03ffffff

add r2,sp,#32

vld1.8 {d2-d3},[r2,: 128]

vorr.i32 q3,#0x01000000

add r2,sp,#16

vld1.8 {d0-d1},[r2,: 128]

vmlal.u32 q15,d20,d5

vmlal.u32 q15,d21,d4

vmlal.u32 q15,d18,d3

vmlal.u32 q15,d19,d2

vmlal.u32 q15,d6,d0

add r2,sp,#80

vld1.8 {d24-d25},[r2,: 128]

vmlal.u32 q4,d20,d4

vmlal.u32 q4,d21,d3

vmlal.u32 q4,d18,d2

vmlal.u32 q4,d19,d0

vmlal.u32 q4,d6,d25

add r2,sp,#64

vld1.8 {d22-d23},[r2,: 128]

vmlal.u32 q7,d6,d22

vmlal.u32 q7,d18,d24

vmlal.u32 q7,d19,d23

vmlal.u32 q7,d20,d0

vmlal.u32 q7,d21,d25

vmlal.u32 q14,d20,d2

vmlal.u32 q14,d21,d0

vmlal.u32 q14,d18,d25

vmlal.u32 q14,d19,d24

vmlal.u32 q14,d6,d23

vmlal.u32 q13,d20,d3

vmlal.u32 q13,d21,d2

vmlal.u32 q13,d18,d0

vmlal.u32 q13,d19,d25

vmlal.u32 q13,d6,d24

add r2,sp,#0

vshr.u64 q3,q7,#26

sub r3,r3,#64

vand q5,q7,q6

vadd.i64 q3,q14,q3

vshr.u64 q7,q4,#26

vand q4,q4,q6

vadd.i64 q7,q15,q7

vld1.8 {d30-d31},[r2,: 128]

vshr.u64 q8,q3,#26

vand q3,q3,q6

vshr.u64 q9,q7,#26

vadd.i64 q8,q13,q8

vand q10,q7,q6

vadd.i64 q5,q5,q9

vld1.8 {d14-d15},[r2,: 128]

add r2,sp,#128

vshl.i64 q9,q9,#2

vshr.u64 q13,q8,#26

vadd.i64 q14,q5,q9

vld1.8 {d10-d11},[r2,: 128]

vand q9,q8,q6

vadd.i64 q4,q4,q13

add r1,r1,#32

vshr.u64 q13,q14,#26

vtrn.32 d18,d19

vand q8,q14,q6

vadd.i64 q3,q3,q13

vshr.u64 q13,q4,#26

vtrn.32 d16,d17

vand q4,q4,q6

vtrn.32 d6,d7

vadd.i64 q10,q10,q13

vtrn.32 d8,d9

vext.32 d17,d6,d6,#0

vext.32 d19,d8,d8,#0

vtrn.32 d20,d21

cmp r3,#64

bhi ._mainloop2

sub r2,r1,#32

._below64bytes:

cmp r3,#32

bls ._end

._mainloop:

add r1,sp,#0

vld1.8 {d8-d9},[r1,: 128]

vld1.8 {d10-d11},[r1,: 128]

vld1.8 {d14-d15},[r2]!

vmlal.u32 q4,d16,d5

vld1.8 {d26-d27},[r2]!

vmlal.u32 q4,d17,d4

vext.32 d6,d10,d14,#1

vmlal.u32 q4,d18,d3

vext.32 d7,d10,d26,#1

vmlal.u32 q4,d19,d2

vrev64.i32 q3,q3

vmlal.u32 q4,d20,d0

vmlal.u32 q3,d20,d22

vmlal.u32 q3,d18,d24

vmlal.u32 q3,d19,d23

vtrn.32 q7,q13

vmlal.u32 q3,d16,d0

vshll.u32 q5,d27,#18

vmlal.u32 q3,d17,d25

vmlal.u32 q5,d16,d4

vmlal.u32 q5,d17,d3

vmlal.u32 q5,d18,d2

vmlal.u32 q5,d19,d0

vshll.u32 q13,d26,#6

vmlal.u32 q5,d20,d25

vmlal.u32 q13,d16,d2

vmlal.u32 q13,d17,d0

vmlal.u32 q13,d18,d25

vmlal.u32 q13,d19,d24

vshll.u32 q7,d15,#12

vmlal.u32 q13,d20,d23

vmlal.u32 q7,d16,d3

vmlal.u32 q7,d17,d2

vmlal.u32 q7,d18,d0

vmlal.u32 q7,d19,d25

vmlal.u32 q7,d20,d24

vshr.u64 q8,q3,#26

vand q3,q3,q6

vadd.i64 q8,q13,q8

vshr.u64 q9,q5,#26

vand q5,q5,q6

vadd.i64 q4,q4,q9

vshr.u64 q9,q8,#26

vand q10,q8,q6

vshr.u64 q8,q4,#26

vadd.i64 q7,q7,q9

vand q4,q4,q6

vadd.i64 q3,q3,q8

vshl.i64 q8,q8,#2

vshr.u64 q13,q7,#26

vadd.i64 q3,q3,q8

vand q9,q7,q6

vadd.i64 q5,q5,q13

vshr.u64 q7,q3,#26

vand q8,q3,q6

vadd.i64 q3,q10,q7

vshr.u64 q7,q5,#26

vand q5,q5,q6

vadd.i64 q10,q4,q7

sub r3,r3,#32

vtrn.32 d16,d17

vtrn.32 d18,d19

vtrn.32 d6,d7

vtrn.32 d10,d11

vtrn.32 d20,d21

vext.32 d17,d6,d6,#0

vext.32 d19,d10,d10,#0

cmp r3,#32

bhi ._mainloop

._end:

vst1.8 {d16-d17},[r0]!

vst1.8 {d18-d19},[r0]!

vst1.8 d20,[r0]

mov r0,r3

mov sp,r12
vpop {q4,q5,q6,q7}
bx lr

.align 2
.global openssl_poly1305_neon2_addmulmod
.hidden openssl_poly1305_neon2_addmulmod
.type openssl_poly1305_neon2_addmulmod STT_FUNC
openssl_poly1305_neon2_addmulmod:
sub sp,sp,#0

vmov.i64 q0,#0xffffffff

vld1.8 {d2-d3},[r2,: 128]!

vshl.i32 q2,q1,#2

vld1.8 {d6-d7},[r2,: 128]!

vshl.i32 q8,q3,#2

vld1.8 {d18},[r2,: 64]

vshl.i32 q10,q9,#2

vld1.8 {d22-d23},[r1,: 128]!

vadd.i32 q2,q2,q1

vld1.8 {d24-d25},[r1,: 128]!

vadd.i32 q8,q8,q3

vadd.i32 q10,q10,q9

vld1.8 {d26-d27},[r3,: 128]!

vadd.i32 q11,q11,q13

vld1.8 {d26-d27},[r3,: 128]!

vadd.i32 q12,q12,q13

vld1.8 {d26},[r1,: 64]

vshr.u64 q0,q0,#6

vld1.8 {d28},[r3,: 64]

vadd.i32 q13,q13,q14

vmull.u32 q14,d22,d2

vmlal.u32 q14,d23,d20

vmlal.u32 q14,d24,d17

vmlal.u32 q14,d25,d16

vmlal.u32 q14,d26,d5

vmull.u32 q2,d22,d3

vmlal.u32 q2,d23,d2

vmlal.u32 q2,d24,d20

vmlal.u32 q2,d25,d17

vmlal.u32 q2,d26,d16

vmull.u32 q15,d22,d6

vmlal.u32 q15,d23,d3

vmlal.u32 q15,d24,d2

vmlal.u32 q15,d25,d20

vmlal.u32 q15,d26,d17

vmull.u32 q8,d22,d7

vmlal.u32 q8,d23,d6

vmlal.u32 q8,d24,d3

vmlal.u32 q8,d25,d2

vmlal.u32 q8,d26,d20

vmull.u32 q9,d22,d18

vmlal.u32 q9,d23,d7

vmlal.u32 q9,d24,d6

vmlal.u32 q9,d25,d3

vmlal.u32 q9,d26,d2

vshr.u64 q1,q14,#26

vand q3,q14,q0

vadd.i64 q1,q2,q1

vshr.u64 q2,q8,#26

vand q8,q8,q0

vadd.i64 q2,q9,q2

vshr.u64 q9,q1,#26

vand q1,q1,q0

vshr.u64 q10,q2,#26

vadd.i64 q9,q15,q9

vand q2,q2,q0

vadd.i64 q3,q3,q10

vshl.i64 q10,q10,#2

vshr.u64 q11,q9,#26

vadd.i64 q3,q3,q10

vand q9,q9,q0

vadd.i64 q8,q8,q11

vshr.u64 q10,q3,#26

vtrn.32 d18,d19

vand q3,q3,q0

vadd.i64 q1,q1,q10

vshr.u64 q10,q8,#26

vtrn.32 d6,d7

vand q0,q8,q0

vtrn.32 d2,d3

vadd.i64 q2,q2,q10

vtrn.32 d0,d1

vext.32 d7,d2,d2,#0

vext.32 d19,d0,d0,#0

vtrn.32 d4,d5

vst1.8 {d6-d7},[r0,: 128]!

vst1.8 {d18-d19},[r0,: 128]!

vst1.8 d4,[r0,: 64]

add sp,sp,#0
bx lr

#endif  /* !OPENSSL_NO_ASM && OPENSSL_ARM && __ELF__ */
