#ifndef IA32_INSN_H
#define IA32_INSN_H

#include <sys/types.h>
#include "libdis.h"

#define GET_BYTE( buf, buf_len ) buf_len ? *buf : 0

#define OP_SIZE_16	1
#define OP_SIZE_32	2
#define ADDR_SIZE_16	4
#define ADDR_SIZE_32	8

#define MAX_INSTRUCTION_SIZE 20

#define INVALID_INSN ((size_t) -1)
#define MAKE_INVALID( i, buf )                          \
                strcpy( i->mnemonic, "invalid" );       \
                x86_oplist_free( i );                   \
                i->size = 1;                            \
                i->group = insn_none;                   \
                i->type = insn_invalid;                 \
                memcpy( i->bytes, buf, 1 );

size_t ia32_disasm_addr( unsigned char * buf, size_t buf_len,
		x86_insn_t *insn);

typedef struct {
   unsigned int table;
   unsigned int mnem_flag;
   unsigned int notes;
   unsigned int dest_flag, src_flag, aux_flag;
   unsigned int cpu;
   char mnemonic[16];
   char mnemonic_att[16];
   int32_t dest;
   int32_t src;
   int32_t aux;
   unsigned int flags_effected;
   unsigned int implicit_ops;
} ia32_insn_t;

#define PREFIX_LOCK       0x0004
#define PREFIX_REPNZ      0x0002
#define PREFIX_REPZ       0x0001
#define PREFIX_OP_SIZE    0x0010
#define PREFIX_ADDR_SIZE  0x0020
#define PREFIX_CS         0x0100
#define PREFIX_SS         0x0200
#define PREFIX_DS         0x0300
#define PREFIX_ES         0x0400
#define PREFIX_FS         0x0500
#define PREFIX_GS         0x0600
#define PREFIX_TAKEN      0x1000
#define PREFIX_NOTTAKEN   0x2000
#define PREFIX_REG_MASK   0x0F00
#define BRANCH_HINT_MASK  0x3000
#define PREFIX_PRINT_MASK 0x000F
#define PREFIX_MASK       0xFFFF

#define cpu_8086         0x0001
#define cpu_80286        0x0002
#define cpu_80386        0x0003
#define cpu_80387        0x0004
#define cpu_80486        0x0005
#define cpu_PENTIUM      0x0006
#define cpu_PENTPRO      0x0007
#define cpu_PENTIUM2     0x0008
#define cpu_PENTIUM3     0x0009
#define cpu_PENTIUM4     0x000A
#define cpu_K6		 0x0010
#define cpu_K7		 0x0020
#define cpu_ATHLON	 0x0030
#define CPU_MODEL_MASK	 0xFFFF
#define CPU_MODEL(cpu)	 (cpu & CPU_MODEL_MASK)

#define isa_GP		 0x10000
#define isa_FPU		 0x20000
#define isa_FPUMGT	 0x30000
#define isa_MMX		 0x40000
#define isa_SSE1	 0x50000
#define isa_SSE2	 0x60000
#define isa_SSE3	 0x70000
#define isa_3DNOW	 0x80000
#define isa_SYS		 0x90000
#define ISA_SUBSET_MASK	 0xFFFF0000
#define ISA_SUBSET(isa)	(isa & ISA_SUBSET_MASK)

#define ARG_NONE         0

#define   OPFLAGS_MASK 	0x0000FFFF

#define   ADDRMETH_MASK	0x00FF0000

#define   ADDRMETH_A  	0x00010000
#define   ADDRMETH_C   	0x00020000
#define   ADDRMETH_D   	0x00030000
#define   ADDRMETH_E   	0x00040000
#define   ADDRMETH_F   	0x00050000
#define   ADDRMETH_G   	0x00060000
#define   ADDRMETH_I   	0x00070000
#define   ADDRMETH_J   	0x00080000
#define   ADDRMETH_M   	0x00090000
#define   ADDRMETH_O   	0x000A0000
#define   ADDRMETH_P   	0x000B0000
#define   ADDRMETH_Q   	0x000C0000
#define   ADDRMETH_R   	0x000D0000
#define   ADDRMETH_S   	0x000E0000
#define   ADDRMETH_T   	0x000F0000
#define   ADDRMETH_V   	0x00100000
#define   ADDRMETH_W   	0x00110000
#define   ADDRMETH_X   	0x00120000
#define   ADDRMETH_Y   	0x00130000
#define	  ADDRMETH_RR  	0x00140000
#define	  ADDRMETH_RS  	0x00150000
#define	  ADDRMETH_RT  	0x00160000
#define	  ADDRMETH_RF  	0x00170000
#define	  ADDRMETH_II  	0x00180000
#define   ADDRMETH_PP   0x00190000
#define   ADDRMETH_VV   0x001A0000

#define OPTYPE_MASK	0xFF000000

#define OPTYPE_a	0x01000000
#define OPTYPE_b   	0x02000000
#define OPTYPE_c   	0x03000000
#define OPTYPE_d   	0x04000000
#define OPTYPE_dq   	0x05000000
#define OPTYPE_p   	0x06000000
#define OPTYPE_pi   	0x07000000
#define OPTYPE_ps   	0x08000000
#define OPTYPE_q   	0x09000000
#define OPTYPE_s   	0x0A000000
#define OPTYPE_ss   	0x0B000000
#define OPTYPE_si   	0x0C000000
#define OPTYPE_v   	0x0D000000
#define OPTYPE_w   	0x0E000000
#define OPTYPE_m   	0x0F000000
#define OPTYPE_none 0xFF000000

#define OPTYPE_fs	0x10000000
#define OPTYPE_fd	0x20000000
#define OPTYPE_fe	0x30000000
#define OPTYPE_fb	0x40000000
#define OPTYPE_fv	0x50000000
#define OPTYPE_ft	0x60000000
#define OPTYPE_fx       0x70000000
#define OPTYPE_fp       0x80000000

#define OPTYPE_sd	0x90000000
#define OPTYPE_pd	0xA0000000

enum ia32_tbl_type_id {
	tbl_opcode = 0,
	tbl_prefix,
	tbl_suffix,
	tbl_extension,
	tbl_ext_ext,
	tbl_fpu,
	tbl_fpu_ext
 };

typedef struct {
   ia32_insn_t *table;
   enum ia32_tbl_type_id type;
   unsigned char shift;
   unsigned char mask;
   unsigned char minlim,maxlim;
} ia32_table_desc_t;

#define OP_R         0x001
#define OP_W         0x002
#define OP_RW        0x003
#define OP_X         0x004

#define OP_PERM_MASK 0x0000007
#define OP_PERM( type )       (type & OP_PERM_MASK)

#define OP_SIGNED    0x010

#define OP_FLAG_MASK  0x0F0
#define OP_FLAGS( type )        (type & OP_FLAG_MASK)

#define OP_REG_MASK    0x0000FFFF
#define OP_REGTBL_MASK 0xFFFF0000
#define OP_REGID( type )      (type & OP_REG_MASK)
#define OP_REGTYPE( type )    (type & OP_REGTBL_MASK)

#define INS_FLAG_PREFIX		0x10000000
#define INS_FLAG_SUFFIX		0x20000000
#define INS_FLAG_MASK    	0xFF000000

#define INS_NOTE_RING0		0x00000001
#define INS_NOTE_SMM		0x00000002
#define INS_NOTE_SERIAL		0x00000004
#define INS_NOTE_NONSWAP    0x00000008
#define INS_NOTE_NOSUFFIX   0x00000010

#define INS_INVALID 	0

#define INS_EXEC	0x1000
#define INS_ARITH	0x2000
#define INS_LOGIC	0x3000
#define INS_STACK	0x4000
#define INS_COND	0x5000
#define INS_LOAD	0x6000
#define INS_ARRAY	0x7000
#define INS_BIT		0x8000
#define INS_FLAG	0x9000
#define INS_FPU		0xA000
#define INS_TRAPS	0xD000
#define INS_SYSTEM	0xE000
#define INS_OTHER	0xF000

#define INS_GROUP_MASK	0xF000
#define INS_GROUP( type )     ( type & INS_GROUP_MASK )

#define INS_BRANCH	(INS_EXEC | 0x01)
#define INS_BRANCHCC	(INS_EXEC | 0x02)
#define INS_CALL	(INS_EXEC | 0x03)
#define INS_CALLCC	(INS_EXEC | 0x04)
#define INS_RET		(INS_EXEC | 0x05)

#define INS_ADD 	(INS_ARITH | 0x01)
#define INS_SUB		(INS_ARITH | 0x02)
#define INS_MUL		(INS_ARITH | 0x03)
#define INS_DIV		(INS_ARITH | 0x04)
#define INS_INC		(INS_ARITH | 0x05)
#define INS_DEC		(INS_ARITH | 0x06)
#define INS_SHL		(INS_ARITH | 0x07)
#define INS_SHR		(INS_ARITH | 0x08)
#define INS_ROL		(INS_ARITH | 0x09)
#define INS_ROR		(INS_ARITH | 0x0A)
#define INS_MIN		(INS_ARITH | 0x0B)
#define INS_MAX		(INS_ARITH | 0x0C)
#define INS_AVG		(INS_ARITH | 0x0D)
#define INS_FLR		(INS_ARITH | 0x0E)
#define INS_CEIL	(INS_ARITH | 0x0F)

#define INS_AND		(INS_LOGIC | 0x01)
#define INS_OR		(INS_LOGIC | 0x02)
#define INS_XOR		(INS_LOGIC | 0x03)
#define INS_NOT		(INS_LOGIC | 0x04)
#define INS_NEG		(INS_LOGIC | 0x05)
#define INS_NAND	(INS_LOGIC | 0x06)

#define INS_PUSH	(INS_STACK | 0x01)
#define INS_POP		(INS_STACK | 0x02)
#define INS_PUSHREGS	(INS_STACK | 0x03)
#define INS_POPREGS	(INS_STACK | 0x04)
#define INS_PUSHFLAGS	(INS_STACK | 0x05)
#define INS_POPFLAGS	(INS_STACK | 0x06)
#define INS_ENTER	(INS_STACK | 0x07)
#define INS_LEAVE	(INS_STACK | 0x08)

#define INS_TEST	(INS_COND | 0x01)
#define INS_CMP		(INS_COND | 0x02)

#define INS_MOV		(INS_LOAD | 0x01)
#define INS_MOVCC	(INS_LOAD | 0x02)
#define INS_XCHG	(INS_LOAD | 0x03)
#define INS_XCHGCC	(INS_LOAD | 0x04)
#define INS_CONV	(INS_LOAD | 0x05)

#define INS_STRCMP	(INS_ARRAY | 0x01)
#define INS_STRLOAD	(INS_ARRAY | 0x02)
#define INS_STRMOV	(INS_ARRAY | 0x03)
#define INS_STRSTOR	(INS_ARRAY | 0x04)
#define INS_XLAT	(INS_ARRAY | 0x05)

#define INS_BITTEST	(INS_BIT | 0x01)
#define INS_BITSET	(INS_BIT | 0x02)
#define INS_BITCLR	(INS_BIT | 0x03)

#define INS_CLEARCF	(INS_FLAG | 0x01)
#define INS_CLEARZF	(INS_FLAG | 0x02)
#define INS_CLEAROF	(INS_FLAG | 0x03)
#define INS_CLEARDF	(INS_FLAG | 0x04)
#define INS_CLEARSF	(INS_FLAG | 0x05)
#define INS_CLEARPF	(INS_FLAG | 0x06)
#define INS_SETCF	(INS_FLAG | 0x07)
#define INS_SETZF	(INS_FLAG | 0x08)
#define INS_SETOF	(INS_FLAG | 0x09)
#define INS_SETDF	(INS_FLAG | 0x0A)
#define INS_SETSF	(INS_FLAG | 0x0B)
#define INS_SETPF	(INS_FLAG | 0x0C)
#define INS_TOGCF	(INS_FLAG | 0x10)
#define INS_TOGZF	(INS_FLAG | 0x20)
#define INS_TOGOF	(INS_FLAG | 0x30)
#define INS_TOGDF	(INS_FLAG | 0x40)
#define INS_TOGSF	(INS_FLAG | 0x50)
#define INS_TOGPF	(INS_FLAG | 0x60)

#define INS_FMOV       (INS_FPU | 0x1)
#define INS_FMOVCC     (INS_FPU | 0x2)
#define INS_FNEG       (INS_FPU | 0x3)
#define INS_FABS       (INS_FPU | 0x4)
#define INS_FADD       (INS_FPU | 0x5)
#define INS_FSUB       (INS_FPU | 0x6)
#define INS_FMUL       (INS_FPU | 0x7)
#define INS_FDIV       (INS_FPU | 0x8)
#define INS_FSQRT      (INS_FPU | 0x9)
#define INS_FCMP       (INS_FPU | 0xA)
#define INS_FCOS       (INS_FPU | 0xC)
#define INS_FLDPI      (INS_FPU | 0xD)
#define INS_FLDZ       (INS_FPU | 0xE)
#define INS_FTAN       (INS_FPU | 0xF)
#define INS_FSINE      (INS_FPU | 0x10)
#define INS_FSYS       (INS_FPU | 0x20)

#define INS_TRAP	(INS_TRAPS | 0x01)
#define INS_TRAPCC	(INS_TRAPS | 0x02)
#define INS_TRET	(INS_TRAPS | 0x03)
#define INS_BOUNDS	(INS_TRAPS | 0x04)
#define INS_DEBUG	(INS_TRAPS | 0x05)
#define INS_TRACE	(INS_TRAPS | 0x06)
#define INS_INVALIDOP	(INS_TRAPS | 0x07)
#define INS_OFLOW	(INS_TRAPS | 0x08)
#define INS_ICEBP	(INS_TRAPS | 0x09)

#define INS_HALT	(INS_SYSTEM | 0x01)
#define INS_IN		(INS_SYSTEM | 0x02)
#define INS_OUT		(INS_SYSTEM | 0x03)
#define INS_CPUID	(INS_SYSTEM | 0x04)

#define INS_NOP		(INS_OTHER | 0x01)
#define INS_BCDCONV	(INS_OTHER | 0x02)
#define INS_SZCONV	(INS_OTHER | 0x03)
#define INS_SALC	(INS_OTHER | 0x04)
#define INS_UNKNOWN	(INS_OTHER | 0x05)

#define INS_TYPE_MASK	0xFFFF
#define INS_TYPE( type )      ( type & INS_TYPE_MASK )

#define INS_TEST_CARRY        0x01
#define INS_TEST_ZERO         0x02
#define INS_TEST_OFLOW        0x04
#define INS_TEST_DIR          0x08
#define INS_TEST_SIGN         0x10
#define INS_TEST_PARITY       0x20
#define INS_TEST_OR           0x40
#define INS_TEST_NCARRY       0x100
#define INS_TEST_NZERO        0x200
#define INS_TEST_NOFLOW       0x400
#define INS_TEST_NDIR         0x800
#define INS_TEST_NSIGN        0x100
#define INS_TEST_NPARITY      0x2000

#define INS_TEST_SFEQOF       0x4000

#define INS_TEST_SFNEOF       0x8000

#define INS_TEST_ALL		INS_TEST_CARRY | INS_TEST_ZERO | \
				INS_TEST_OFLOW | INS_TEST_SIGN | \
				INS_TEST_PARITY

#define INS_SET_CARRY        0x010000
#define INS_SET_ZERO         0x020000
#define INS_SET_OFLOW        0x040000
#define INS_SET_DIR          0x080000
#define INS_SET_SIGN         0x100000
#define INS_SET_PARITY       0x200000
#define INS_SET_NCARRY       0x1000000
#define INS_SET_NZERO        0x2000000
#define INS_SET_NOFLOW       0x4000000
#define INS_SET_NDIR         0x8000000
#define INS_SET_NSIGN        0x10000000
#define INS_SET_NPARITY      0x20000000
#define INS_SET_SFEQOF       0x40000000
#define INS_SET_SFNEOF       0x80000000

#define INS_SET_ALL		INS_SET_CARRY | INS_SET_ZERO | \
				INS_SET_OFLOW | INS_SET_SIGN | \
				INS_SET_PARITY

#define INS_TEST_MASK          0x0000FFFF
#define INS_FLAGS_TEST(x)      (x & INS_TEST_MASK)
#define INS_SET_MASK           0xFFFF0000
#define INS_FLAGS_SET(x)       (x & INS_SET_MASK)

#if 0

#define X86_PAIR_NP	1
#define X86_PAIR_PU	2
#define X86_PAIR_PV	3
#define X86_PAIR_UV	4
#define X86_PAIR_FX	5

#define X86_EXEC_PORT_0	1
#define X86_EXEC_PORT_1	2
#define X86_EXEC_PORT_2	4
#define X86_EXEC_PORT_3	8
#define X86_EXEC_PORT_4	16

#define X86_EXEC_UNITS

typedef struct {
	uint32_t flags;

	uint32_t prefix;
	char prefix_str[16];
	uint32_t branch_hint;
	unsigned int cpu_ver;
	unsigned int clocks;
	unsigned char last_prefix;

	unsigned char mode;
	unsigned char gen_regs;
	unsigned char sz_operand;
	unsigned char sz_address;
	unsigned char uops;
	unsigned char pairing;
	unsigned char exec_unit;
	unsigned char exec_port;
	unsigned char latency;
} ia32_info_t;
#define MODE_32 0
#define MODE_16 1
#define MODE_64 2
#endif

#endif
