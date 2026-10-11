#ifndef LIBDISASM_H
#define LIBDISASM_H

#ifdef WIN32
#include <windows.h>
#endif

#include <stdint.h>

#ifndef LIBDISASM_QWORD_H
        #define LIBDISASM_QWORD_H
        #ifdef _MSC_VER
                typedef __int64         qword_t;
        #else
                typedef int64_t         qword_t;
        #endif
#endif

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum x86_report_codes {
        report_disasm_bounds,

        report_insn_bounds,

        report_invalid_insn,

        report_unknown
};

typedef void (*DISASM_REPORTER)( enum x86_report_codes code,
				 void *data, void *arg );

void x86_report_error( enum x86_report_codes code, void *data );

enum x86_options {
        opt_none= 0,
        opt_ignore_nulls=1,
        opt_16_bit=2,
        opt_att_mnemonics=4,
};

int x86_init( enum x86_options options, DISASM_REPORTER reporter, void *arg);
void x86_set_reporter( DISASM_REPORTER reporter, void *arg);
void x86_set_options( enum x86_options options );
enum x86_options x86_get_options( void );
int x86_cleanup(void);

#define MAX_REGNAME 8

#define MAX_PREFIX_STR 32
#define MAX_MNEM_STR 16
#define MAX_INSN_SIZE 20
#define MAX_OP_STRING 32
#define MAX_OP_RAW_STRING 64
#define MAX_OP_XML_STRING 256
#define MAX_NUM_OPERANDS 8

#define MAX_INSN_STRING 512
#define MAX_INSN_RAW_STRING 1024
#define MAX_INSN_XML_STRING 4096

enum x86_reg_type {
        reg_gen         = 0x00001,
        reg_in          = 0x00002,
        reg_out         = 0x00004,
        reg_local       = 0x00008,
        reg_fpu         = 0x00010,
        reg_seg         = 0x00020,
        reg_simd        = 0x00040,
        reg_sys         = 0x00080,
        reg_sp          = 0x00100,
        reg_fp          = 0x00200,
        reg_pc          = 0x00400,
        reg_retaddr     = 0x00800,
        reg_cond        = 0x01000,
        reg_zero        = 0x02000,
        reg_ret         = 0x04000,
        reg_src         = 0x10000,
        reg_dest        = 0x20000,
        reg_count       = 0x40000
};

typedef struct {
        char name[MAX_REGNAME];
        enum x86_reg_type type;
        unsigned int size;
        unsigned int id;
	unsigned int alias;
	unsigned int shift;
} x86_reg_t;

typedef struct {
        unsigned int     scale;
        x86_reg_t        index, base;
        int32_t          disp;
        char             disp_sign;
        char             disp_size;
} x86_ea_t;

typedef struct {
	unsigned short	segment;
	union {
		unsigned short	off16;
		uint32_t		off32;
	} offset;
} x86_absolute_t;

enum x86_op_type {
        op_unused = 0,
        op_register = 1,
        op_immediate = 2,
        op_relative_near = 3,
        op_relative_far = 4,
        op_absolute = 5,
        op_expression = 6,
        op_offset = 7,
        op_unknown
};

#define x86_optype_is_address( optype ) \
	( optype == op_absolute || optype == op_offset )
#define x86_optype_is_relative( optype ) \
	( optype == op_relative_near || optype == op_relative_far )
#define x86_optype_is_memory( optype ) \
	( optype > op_immediate && optype < op_unknown )

enum x86_op_datatype {
        op_byte = 1,
        op_word = 2,
        op_dword = 3,
        op_qword = 4,
        op_dqword = 5,
        op_sreal = 6,
        op_dreal = 7,
        op_extreal = 8,
        op_bcd = 9,
        op_ssimd = 10,
        op_dsimd = 11,
        op_sssimd = 12,
        op_sdsimd = 13,
	op_descr32 = 14,
	op_descr16 = 15,
	op_pdescr32 = 16,
	op_pdescr16 = 17,
	op_bounds16 = 18,
	op_bounds32 = 19,
        op_fpuenv16 = 20,
        op_fpuenv32 = 21,
	op_fpustate16 = 22,
	op_fpustate32 = 23,
	op_fpregset = 24,
	op_fpreg = 25,
    op_none = 0xFF,
};

enum x86_op_access {
        op_read = 1,
        op_write = 2,
        op_execute = 4
};

enum x86_op_flags {
        op_signed = 1,
        op_string = 2,
        op_constant = 4,
        op_pointer = 8,
	op_sysref = 0x010,
	op_implied = 0x020,
	op_hardcode = 0x40,

        op_es_seg = 0x100,
        op_cs_seg = 0x200,
        op_ss_seg = 0x300,
        op_ds_seg = 0x400,
        op_fs_seg = 0x500,
        op_gs_seg = 0x600
};

typedef struct {
        enum x86_op_type        type;
        enum x86_op_datatype    datatype;
        enum x86_op_access      access;
        enum x86_op_flags       flags;
        union {

                char            sbyte;
                short           sword;
                int32_t         sdword;
                qword_t         sqword;
                unsigned char   byte;
                unsigned short  word;
                uint32_t        dword;
                qword_t         qword;
                float           sreal;
                double          dreal;

                unsigned char   extreal[10];
                unsigned char   bcd[10];
                qword_t         dqword[2];
                unsigned char   simd[16];
                unsigned char   fpuenv[28];

                uint32_t        offset;

                x86_reg_t       reg;

                char            relative_near;
                int32_t         relative_far;

		x86_absolute_t	absolute;

                x86_ea_t        expression;
        } data;

	void * insn;
} x86_op_t;

typedef struct x86_operand_list {
	x86_op_t op;
	struct x86_operand_list *next;
} x86_oplist_t;

enum x86_insn_group {
	insn_none = 0,
        insn_controlflow = 1,
        insn_arithmetic = 2,
        insn_logic = 3,
        insn_stack = 4,
        insn_comparison = 5,
        insn_move = 6,
        insn_string = 7,
        insn_bit_manip = 8,
        insn_flag_manip = 9,
        insn_fpu = 10,
        insn_interrupt = 13,
        insn_system = 14,
        insn_other = 15
};

enum x86_insn_type {
	insn_invalid = 0,

        insn_jmp = 0x1001,
        insn_jcc = 0x1002,
        insn_call = 0x1003,
        insn_callcc = 0x1004,
        insn_return = 0x1005,

        insn_add = 0x2001,
        insn_sub = 0x2002,
        insn_mul = 0x2003,
        insn_div = 0x2004,
        insn_inc = 0x2005,
        insn_dec = 0x2006,
        insn_shl = 0x2007,
        insn_shr = 0x2008,
        insn_rol = 0x2009,
        insn_ror = 0x200A,

        insn_and = 0x3001,
        insn_or = 0x3002,
        insn_xor = 0x3003,
        insn_not = 0x3004,
        insn_neg = 0x3005,

        insn_push = 0x4001,
        insn_pop = 0x4002,
        insn_pushregs = 0x4003,
        insn_popregs = 0x4004,
        insn_pushflags = 0x4005,
        insn_popflags = 0x4006,
        insn_enter = 0x4007,
        insn_leave = 0x4008,

        insn_test = 0x5001,
        insn_cmp = 0x5002,

        insn_mov = 0x6001,
        insn_movcc = 0x6002,
        insn_xchg = 0x6003,
        insn_xchgcc = 0x6004,

        insn_strcmp = 0x7001,
        insn_strload = 0x7002,
        insn_strmov = 0x7003,
        insn_strstore = 0x7004,
        insn_translate = 0x7005,

        insn_bittest = 0x8001,
        insn_bitset = 0x8002,
        insn_bitclear = 0x8003,

        insn_clear_carry = 0x9001,
        insn_clear_zero = 0x9002,
        insn_clear_oflow = 0x9003,
        insn_clear_dir = 0x9004,
        insn_clear_sign = 0x9005,
        insn_clear_parity = 0x9006,
        insn_set_carry = 0x9007,
        insn_set_zero = 0x9008,
        insn_set_oflow = 0x9009,
        insn_set_dir = 0x900A,
        insn_set_sign = 0x900B,
        insn_set_parity = 0x900C,
        insn_tog_carry = 0x9010,
        insn_tog_zero = 0x9020,
        insn_tog_oflow = 0x9030,
        insn_tog_dir = 0x9040,
        insn_tog_sign = 0x9050,
        insn_tog_parity = 0x9060,

        insn_fmov = 0xA001,
        insn_fmovcc = 0xA002,
        insn_fneg = 0xA003,
        insn_fabs = 0xA004,
        insn_fadd = 0xA005,
        insn_fsub = 0xA006,
        insn_fmul = 0xA007,
        insn_fdiv = 0xA008,
        insn_fsqrt = 0xA009,
        insn_fcmp = 0xA00A,
        insn_fcos = 0xA00C,
        insn_fldpi = 0xA00D,
        insn_fldz = 0xA00E,
        insn_ftan = 0xA00F,
        insn_fsine = 0xA010,
        insn_fsys = 0xA020,

        insn_int = 0xD001,
        insn_intcc = 0xD002,
        insn_iret = 0xD003,
        insn_bound = 0xD004,
        insn_debug = 0xD005,
        insn_trace = 0xD006,
        insn_invalid_op = 0xD007,
        insn_oflow = 0xD008,

        insn_halt = 0xE001,
        insn_in = 0xE002,
        insn_out = 0xE003,
        insn_cpuid = 0xE004,

        insn_nop = 0xF001,
        insn_bcdconv = 0xF002,
        insn_szconv = 0xF003
};

enum x86_insn_note {
	insn_note_ring0		= 1,
	insn_note_smm		= 2,
	insn_note_serial	= 4,
	insn_note_nonswap	= 8,
	insn_note_nosuffix  = 16,
};

enum x86_flag_status {
        insn_carry_set = 0x1,
        insn_zero_set = 0x2,
        insn_oflow_set = 0x4,
        insn_dir_set = 0x8,
        insn_sign_set = 0x10,
        insn_parity_set = 0x20,
        insn_carry_or_zero_set = 0x40,
        insn_zero_set_or_sign_ne_oflow = 0x80,
        insn_carry_clear = 0x100,
        insn_zero_clear = 0x200,
        insn_oflow_clear = 0x400,
        insn_dir_clear = 0x800,
        insn_sign_clear = 0x1000,
        insn_parity_clear = 0x2000,
        insn_sign_eq_oflow = 0x4000,
        insn_sign_ne_oflow = 0x8000
};

enum x86_insn_cpu {
	cpu_8086 	= 1,
	cpu_80286	= 2,
	cpu_80386	= 3,
	cpu_80387	= 4,
	cpu_80486	= 5,
	cpu_pentium	= 6,
	cpu_pentiumpro	= 7,
	cpu_pentium2	= 8,
	cpu_pentium3	= 9,
	cpu_pentium4	= 10,
	cpu_k6		= 16,
	cpu_k7		= 32,
	cpu_athlon	= 48
};

enum x86_insn_isa {
	isa_gp		= 1,
	isa_fp		= 2,
	isa_fpumgt	= 3,
	isa_mmx		= 4,
	isa_sse1	= 5,
	isa_sse2	= 6,
	isa_sse3	= 7,
	isa_3dnow	= 8,
	isa_sys		= 9
};

enum x86_insn_prefix {
        insn_no_prefix = 0,
        insn_rep_zero = 1,
        insn_rep_notzero = 2,
        insn_lock = 4
};

typedef struct {

        uint32_t addr;
        uint32_t offset;
        enum x86_insn_group group;
        enum x86_insn_type type;
	enum x86_insn_note note;
        unsigned char bytes[MAX_INSN_SIZE];
        unsigned char size;

	unsigned char addr_size;
	unsigned char op_size;

	enum x86_insn_cpu cpu;
	enum x86_insn_isa isa;

        enum x86_flag_status flags_set;
        enum x86_flag_status flags_tested;

	unsigned char stack_mod;
	int32_t stack_mod_val;

        enum x86_insn_prefix prefix;
        char prefix_string[MAX_PREFIX_STR];
        char mnemonic[MAX_MNEM_STR];
        x86_oplist_t *operands;
	size_t operand_count;
	size_t explicit_count;

        void *block;
        void *function;
        int tag;
} x86_insn_t;

int x86_insn_is_valid( x86_insn_t *insn );

typedef void (*DISASM_CALLBACK)( x86_insn_t *insn, void * arg );

typedef int32_t (*DISASM_RESOLVER)( x86_op_t *op, x86_insn_t * current_insn,
				 void *arg );

unsigned int x86_disasm( unsigned char *buf, unsigned int buf_len,
                	 uint32_t buf_rva, unsigned int offset,
                	 x86_insn_t * insn );

unsigned int x86_disasm_range( unsigned char *buf, uint32_t buf_rva,
	                       unsigned int offset, unsigned int len,
	                       DISASM_CALLBACK func, void *arg );

unsigned int x86_disasm_forward( unsigned char *buf, unsigned int buf_len,
	                         uint32_t buf_rva, unsigned int offset,
	                         DISASM_CALLBACK func, void *arg,
	                         DISASM_RESOLVER resolver, void *r_arg );

typedef void (*x86_operand_fn)(x86_op_t *op, x86_insn_t *insn, void *arg);

enum x86_op_foreach_type {
	op_any 	= 0,
	op_dest = 1,
	op_src 	= 2,
	op_ro 	= 3,
	op_wo 	= 4,
	op_xo 	= 5,
	op_rw 	= 6,
	op_implicit = 0x10,
	op_explicit = 0x20
};

void x86_oplist_free( x86_insn_t *insn );

int x86_operand_foreach( x86_insn_t *insn, x86_operand_fn func, void *arg,
	       	  	 enum x86_op_foreach_type type);

size_t x86_operand_count( x86_insn_t *insn, enum x86_op_foreach_type type );

x86_op_t * x86_operand_1st( x86_insn_t *insn );
x86_op_t * x86_operand_2nd( x86_insn_t *insn );
x86_op_t * x86_operand_3rd( x86_insn_t *insn );

#define x86_get_dest_operand( insn ) x86_operand_1st( insn )
#define x86_get_src_operand( insn ) x86_operand_2nd( insn )
#define x86_get_imm_operand( insn ) x86_operand_3rd( insn )

unsigned int x86_operand_size( x86_op_t *op );

uint32_t x86_get_address( x86_insn_t *insn );

int32_t x86_get_rel_offset( x86_insn_t *insn );

x86_op_t * x86_get_branch_target( x86_insn_t *insn );

x86_op_t * x86_get_imm( x86_insn_t *insn );

unsigned char * x86_get_raw_imm( x86_insn_t *insn );

void x86_set_insn_addr( x86_insn_t *insn, uint32_t addr );

void x86_set_insn_offset( x86_insn_t *insn, unsigned int offset );

void x86_set_insn_function( x86_insn_t *insn, void * func );

void x86_set_insn_block( x86_insn_t *insn, void * block );

void x86_tag_insn( x86_insn_t *insn );

void x86_untag_insn( x86_insn_t *insn );

int x86_insn_is_tagged( x86_insn_t *insn );

enum x86_asm_format {
	unknown_syntax = 0,
	native_syntax,
	intel_syntax,
	att_syntax,
	xml_syntax,
	raw_syntax
};

int x86_format_operand(x86_op_t *op, char *buf, int len,
                  enum x86_asm_format format);

int x86_format_mnemonic(x86_insn_t *insn, char *buf, int len,
                        enum x86_asm_format format);

int x86_format_insn(x86_insn_t *insn, char *buf, int len, enum x86_asm_format);

int x86_format_header( char *buf, int len, enum x86_asm_format format);

unsigned int x86_endian(void);

unsigned int x86_addr_size(void);
unsigned int x86_op_size(void);

unsigned int x86_word_size(void);

#define x86_max_inst_size(x) x86_max_insn_size(x)
unsigned int x86_max_insn_size(void);

unsigned int x86_sp_reg(void);
unsigned int x86_fp_reg(void);
unsigned int x86_ip_reg(void);
unsigned int x86_flag_reg(void);

void x86_reg_from_id( unsigned int id, x86_reg_t * reg );

#define x86_get_aliased_reg( alias_reg, output_reg )			\
	x86_reg_from_id( alias_reg->alias, output_reg )

#define X86_WILDCARD_BYTE 0xF4

typedef struct {
        enum x86_op_type        type;
        enum x86_op_datatype    datatype;
        enum x86_op_access      access;
        enum x86_op_flags       flags;
} x86_invariant_op_t;

typedef struct {
	unsigned char bytes[64];
	unsigned int  size;
        enum x86_insn_group group;
        enum x86_insn_type type;
	x86_invariant_op_t operands[3];
} x86_invariant_t;

size_t x86_invariant_disasm( unsigned char *buf, int buf_len,
			  x86_invariant_t *inv );

size_t x86_size_disasm( unsigned char *buf, unsigned int buf_len );

#ifdef __cplusplus
}
#endif

#endif
