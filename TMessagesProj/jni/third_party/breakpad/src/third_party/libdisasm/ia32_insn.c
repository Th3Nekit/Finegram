#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qword.h"

#include "ia32_insn.h"
#include "ia32_opcode_tables.h"

#include "ia32_reg.h"
#include "ia32_operand.h"
#include "ia32_implicit.h"
#include "ia32_settings.h"

#include "libdis.h"

extern ia32_table_desc_t ia32_tables[];
extern ia32_settings_t ia32_settings;

#define IS_SP( op )  (op->type == op_register && 	\
		(op->data.reg.id == REG_ESP_INDEX || 	\
		 op->data.reg.alias == REG_ESP_INDEX) )
#define IS_IMM( op ) (op->type == op_immediate )

#ifdef WIN32
#  define INLINE
#else
#  define INLINE inline
#endif

static INLINE int32_t long_from_operand( x86_op_t *op ) {

	if (! IS_IMM(op) ) {
		return 0L;
	}

	switch ( op->datatype ) {
		case op_byte:
			return (int32_t) op->data.sbyte;
		case op_word:
			return (int32_t) op->data.sword;
		case op_qword:
			return (int32_t) op->data.sqword;
		case op_dword:
			return op->data.sdword;
		default:

			break;
	}

	return 0L;
}

static void ia32_stack_mod(x86_insn_t *insn) {
	x86_op_t *dest, *src = NULL;

	if (! insn || ! insn->operands ) {
		return;
	}

	dest = &insn->operands->op;
	if ( dest ) {
		src = &insn->operands->next->op;
	}

	insn->stack_mod = 0;
	insn->stack_mod_val = 0;

	switch ( insn->type ) {
		case insn_call:
		case insn_callcc:
			insn->stack_mod = 1;
			insn->stack_mod_val = insn->addr_size * -1;
			break;
		case insn_push:
			insn->stack_mod = 1;
			insn->stack_mod_val = insn->addr_size * -1;
			break;
		case insn_return:
			insn->stack_mod = 1;
			insn->stack_mod_val = insn->addr_size;
		case insn_int: case insn_intcc:
		case insn_iret:
			break;
		case insn_pop:
			insn->stack_mod = 1;
			if (! IS_SP( dest ) ) {
				insn->stack_mod_val = insn->op_size;
			}
			break;
		case insn_enter:
			insn->stack_mod = 1;
			insn->stack_mod_val = 0;
			break;
		case insn_leave:
			insn->stack_mod = 1;
			insn->stack_mod_val = 0;
			break;
		case insn_pushregs:
			insn->stack_mod = 1;
			insn->stack_mod_val = 0;
			break;
		case insn_popregs:
			insn->stack_mod = 1;
			insn->stack_mod_val = 0;
			break;
		case insn_pushflags:
			insn->stack_mod = 1;
			insn->stack_mod_val = 0;
			break;
		case insn_popflags:
			insn->stack_mod = 1;
			insn->stack_mod_val = 0;
			break;
		case insn_add:
			if ( IS_SP( dest ) ) {
				insn->stack_mod = 1;
				insn->stack_mod_val = long_from_operand( src );
			}
			break;
		case insn_sub:
			if ( IS_SP( dest ) ) {
				insn->stack_mod = 1;
				insn->stack_mod_val = long_from_operand( src );
				insn->stack_mod_val *= -1;
			}
			break;
		case insn_inc:
			if ( IS_SP( dest ) ) {
				insn->stack_mod = 1;
				insn->stack_mod_val = 1;
			}
			break;
		case insn_dec:
			if ( IS_SP( dest ) ) {
				insn->stack_mod = 1;
				insn->stack_mod_val = 1;
			}
			break;
		case insn_mov: case insn_movcc:
		case insn_xchg: case insn_xchgcc:
		case insn_mul: case insn_div:
		case insn_shl: case insn_shr:
		case insn_rol: case insn_ror:
		case insn_and: case insn_or:
		case insn_not: case insn_neg:
		case insn_xor:
			if ( IS_SP( dest ) ) {
				insn->stack_mod = 1;
			}
			break;
		default:
			break;
	}
	if (! strcmp("enter", insn->mnemonic) ) {
		insn->stack_mod = 1;
	} else if (! strcmp("leave", insn->mnemonic) ) {
		insn->stack_mod = 1;
	}

	return;
}

static void ia32_handle_cpu( x86_insn_t *insn, unsigned int cpu ) {
	insn->cpu = (enum x86_insn_cpu) CPU_MODEL(cpu);
	insn->isa = (enum x86_insn_isa) (ISA_SUBSET(cpu)) >> 16;
	return;
}

static void ia32_handle_mnemtype(x86_insn_t *insn, unsigned int mnemtype) {
	unsigned int type = mnemtype & ~INS_FLAG_MASK;
        insn->group = (enum x86_insn_group) (INS_GROUP(type)) >> 12;
        insn->type = (enum x86_insn_type) INS_TYPE(type);

	return;
}

static void ia32_handle_notes(x86_insn_t *insn, unsigned int notes) {
	insn->note = (enum x86_insn_note) notes;
	return;
}

static void ia32_handle_eflags( x86_insn_t *insn, unsigned int eflags) {
        unsigned int flags;

        flags = INS_FLAGS_TEST(eflags);

        if (flags & INS_TEST_OR) {
                flags &= ~INS_TEST_OR;
                if ( flags & INS_TEST_ZERO ) {
                        flags &= ~INS_TEST_ZERO;
                        if ( flags & INS_TEST_CARRY ) {
                                flags &= ~INS_TEST_CARRY ;
                                flags |= (int)insn_carry_or_zero_set;
                        } else if ( flags & INS_TEST_SFNEOF ) {
                                flags &= ~INS_TEST_SFNEOF;
                                flags |= (int)insn_zero_set_or_sign_ne_oflow;
                        }
                }
        }
        insn->flags_tested = (enum x86_flag_status) flags;

        insn->flags_set = (enum x86_flag_status) INS_FLAGS_SET(eflags) >> 16;

	return;
}

static void ia32_handle_prefix( x86_insn_t *insn, unsigned int prefixes ) {

        insn->prefix = (enum x86_insn_prefix) prefixes & PREFIX_MASK;
        if (! (insn->prefix & PREFIX_PRINT_MASK) ) {

                insn->prefix = insn_no_prefix;
        }

        if ( (unsigned int)insn->prefix & PREFIX_LOCK ) {
                strncat(insn->prefix_string, "lock ", 32 -
				strlen(insn->prefix_string));
        }

        if ( (unsigned int)insn->prefix & PREFIX_REPNZ ) {
                strncat(insn->prefix_string, "repnz ", 32  -
				strlen(insn->prefix_string));
        } else if ( (unsigned int)insn->prefix & PREFIX_REPZ ) {
                strncat(insn->prefix_string, "repz ", 32 -
				strlen(insn->prefix_string));
        }

        return;
}

static void reg_32_to_16( x86_op_t *op, x86_insn_t *insn, void *arg ) {

	if ( op->type == op_register && op->data.reg.size == 4 &&
	     (op->data.reg.type & reg_gen) ) {

		ia32_handle_register( &(op->data.reg),
				op->data.reg.id + 8 );
	}
}

static void handle_insn_metadata( x86_insn_t *insn, ia32_insn_t *raw_insn ) {
	ia32_handle_mnemtype( insn, raw_insn->mnem_flag );
	ia32_handle_notes( insn, raw_insn->notes );
	ia32_handle_eflags( insn, raw_insn->flags_effected );
	ia32_handle_cpu( insn, raw_insn->cpu );
	ia32_stack_mod( insn );
}

static size_t ia32_decode_insn( unsigned char *buf, size_t buf_len,
			   ia32_insn_t *raw_insn, x86_insn_t *insn,
			   unsigned int prefixes ) {
	size_t size, op_size;
	unsigned char modrm;

	if ( raw_insn->mnem_flag == INS_INVALID ) {
		return 0;
	}

	if (ia32_settings.options & opt_16_bit) {
		insn->op_size = ( prefixes & PREFIX_OP_SIZE ) ? 4 : 2;
		insn->addr_size = ( prefixes & PREFIX_ADDR_SIZE ) ? 4 : 2;
	} else {
		insn->op_size = ( prefixes & PREFIX_OP_SIZE ) ? 2 : 4;
		insn->addr_size = ( prefixes & PREFIX_ADDR_SIZE ) ? 2 : 4;
	}

	if ((ia32_settings.options & opt_att_mnemonics) && raw_insn->mnemonic_att[0]) {
		strncpy( insn->mnemonic, raw_insn->mnemonic_att, 16 );
	}
	else {
		strncpy( insn->mnemonic, raw_insn->mnemonic, 16 );
	}
	ia32_handle_prefix( insn, prefixes );

	handle_insn_metadata( insn, raw_insn );

	modrm = GET_BYTE( buf, buf_len );

	op_size = ia32_decode_operand( buf, buf_len, insn, raw_insn->dest,
					raw_insn->dest_flag, prefixes, modrm );

	buf += op_size;
	buf_len -= op_size;
	size = op_size;

	op_size = ia32_decode_operand( buf, buf_len, insn, raw_insn->src,
					raw_insn->src_flag, prefixes, modrm );
	buf += op_size;
	buf_len -= op_size;
	size += op_size;

	op_size = ia32_decode_operand( buf, buf_len, insn, raw_insn->aux,
					raw_insn->aux_flag, prefixes, modrm );
	size += op_size;

	ia32_insn_implicit_ops( insn, raw_insn->implicit_ops );

	if ( (prefixes & PREFIX_REPZ) || (prefixes & PREFIX_REPNZ) ) {
		ia32_insn_implicit_ops( insn, IDX_IMPLICIT_REP );
	}

	if ( insn->op_size == 2 ) {
		x86_operand_foreach( insn, reg_32_to_16, NULL, op_any );
	}

	return size;
}

#define USES_MOD_RM(flag) \
	(flag == ADDRMETH_E || flag == ADDRMETH_M || flag == ADDRMETH_Q || \
	 flag == ADDRMETH_W || flag == ADDRMETH_R)

static int uses_modrm_flag( unsigned int flag ) {
	unsigned int meth;
	if ( flag == ARG_NONE ) {
		return 0;
	}
	meth = (flag & ADDRMETH_MASK);
	if ( USES_MOD_RM(meth) ) {
		return 1;
	}

	return 0;
}

size_t ia32_table_lookup( unsigned char *buf, size_t buf_len,
				 unsigned int table, ia32_insn_t **raw_insn,
				 unsigned int *prefixes ) {
	unsigned char *next, op = buf[0];
	size_t size = 1, sub_size = 0, next_len;
	ia32_table_desc_t *table_desc;
	unsigned int subtable, prefix = 0, recurse_table = 0;

	table_desc = &ia32_tables[table];

	op = GET_BYTE( buf, buf_len );

	if ( table_desc->type == tbl_fpu && op > table_desc->maxlim) {

		table_desc = &ia32_tables[table +1];
	}

	op >>= table_desc->shift;

	op &= table_desc->mask;

	if ( op > table_desc->maxlim ) {

		return INVALID_INSN;
	}

	if ( table_desc->minlim > op ) {

		return INVALID_INSN;
	}

	op -= table_desc->minlim;

	*raw_insn = &(table_desc->table[op]);

	if ( (*raw_insn)->mnem_flag & INS_FLAG_PREFIX ) {
		prefix = (*raw_insn)->mnem_flag & PREFIX_MASK;
	}

	subtable = (*raw_insn)->table;

	if ( subtable && ia32_tables[subtable].type != tbl_suffix &&
	     (! prefix || ! *prefixes) ) {

                if ( ia32_tables[subtable].type == tbl_ext_ext ||
                     ia32_tables[subtable].type == tbl_fpu_ext ) {

			next = buf;
			next_len = buf_len;
		} else {

			if ( buf_len > 1 ) {
				next = &buf[1];
				next_len = buf_len - 1;
			}
			else {

				return INVALID_INSN;
			}
		}

		sub_size = ia32_table_lookup( next, next_len, subtable,
				raw_insn, prefixes );

		if ( prefix && ( sub_size == INVALID_INSN  ||
		       INS_TYPE((*raw_insn)->mnem_flag) == INS_INVALID ) ) {

			recurse_table = 1;
		} else {

			prefix = 0;

			if (sub_size == INVALID_INSN) return INVALID_INSN;
		}
	} else if ( prefix ) {
		recurse_table = 1;
	}

	if ( recurse_table ) {

		sub_size = ia32_table_lookup( &buf[1], buf_len - 1, table,
				raw_insn, prefixes );

		if (sub_size == INVALID_INSN) return INVALID_INSN;

		if ( prefix & BRANCH_HINT_MASK ) {
			if ( INS_GROUP((*raw_insn)->mnem_flag) == INS_EXEC ) {

				prefix &= ~PREFIX_REG_MASK;
			} else {
				prefix &= ~BRANCH_HINT_MASK;
			}
		}

		(*prefixes) |= prefix;
	}

	if ( table_desc->type == tbl_ext_ext ) {

		--size;
	} else if ( (table_desc->type == tbl_extension ||
                     table_desc->type == tbl_fpu ||
		     table_desc->type == tbl_fpu_ext ) &&

                    (uses_modrm_flag((*raw_insn)->dest_flag) ||
	             uses_modrm_flag((*raw_insn)->src_flag) )  	) {
		--size;
	}

	size += sub_size;

	return size;
}

static size_t handle_insn_suffix( unsigned char *buf, size_t buf_len,
			   ia32_insn_t *raw_insn, x86_insn_t * insn ) {
	ia32_insn_t *sfx_insn;
	size_t size;
	unsigned int prefixes = 0;

	size = ia32_table_lookup( buf, buf_len, raw_insn->table, &sfx_insn,
				 &prefixes );
	if (size == INVALID_INSN || sfx_insn->mnem_flag == INS_INVALID ) {
		return 0;
	}

	strncpy( insn->mnemonic, sfx_insn->mnemonic, 16 );
	handle_insn_metadata( insn, sfx_insn );

	return 1;
}

size_t ia32_disasm_addr( unsigned char * buf, size_t buf_len,
		x86_insn_t *insn ) {
	ia32_insn_t *raw_insn = NULL;
	unsigned int prefixes = 0;
	size_t size, sfx_size;

	if ( (ia32_settings.options & opt_ignore_nulls) && buf_len > 3 &&
	    !buf[0] && !buf[1] && !buf[2] && !buf[3]) {

		MAKE_INVALID( insn, buf );
		return 0;
	}

	size = ia32_table_lookup(buf, buf_len, idx_Main, &raw_insn, &prefixes);
	if ( size == INVALID_INSN || size > buf_len || raw_insn->mnem_flag == INS_INVALID ) {
		MAKE_INVALID( insn, buf );

		return 0;
	}

	size += ia32_decode_insn( &buf[size], buf_len - size, raw_insn, insn,
				  prefixes );
	if ( raw_insn->mnem_flag & INS_FLAG_SUFFIX ) {

		sfx_size = handle_insn_suffix( &buf[size], buf_len - size,
				raw_insn, insn );
		if (! sfx_size ) {

			MAKE_INVALID( insn, buf );
			return 0;
		}

		size += sfx_size;
	}

	if (! size ) {

		MAKE_INVALID( insn, buf );
		return 0;
	}

	insn->size = size;
	return size;
}
