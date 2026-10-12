#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libdis.h"
#include "ia32_insn.h"
#include "ia32_operand.h"
#include "ia32_modrm.h"
#include "ia32_reg.h"
#include "x86_imm.h"
#include "x86_operand_list.h"

static void apply_seg( x86_op_t *op, unsigned int prefixes ) {
	if (! prefixes ) return;

	switch ( prefixes & PREFIX_REG_MASK ) {
		case PREFIX_CS:
			op->flags |= op_cs_seg; break;
		case PREFIX_SS:
			op->flags |= op_ss_seg; break;
		case PREFIX_DS:
			op->flags |= op_ds_seg; break;
		case PREFIX_ES:
			op->flags |= op_es_seg; break;
		case PREFIX_FS:
			op->flags |= op_fs_seg; break;
		case PREFIX_GS:
			op->flags |= op_gs_seg; break;
	}

	return;
}

static size_t decode_operand_value( unsigned char *buf, size_t buf_len,
			    x86_op_t *op, x86_insn_t *insn,
			    unsigned int addr_meth, size_t op_size,
			    unsigned int op_value, unsigned char modrm,
			    size_t gen_regs ) {
	size_t size = 0;

	switch (addr_meth) {

		case ADDRMETH_E:
			size = ia32_modrm_decode( buf, buf_len, op, insn,
						  gen_regs );
			break;
		case ADDRMETH_M:
			size = ia32_modrm_decode( buf, buf_len, op, insn,
						  gen_regs );
			break;
		case ADDRMETH_Q:
			size = ia32_modrm_decode( buf, buf_len, op, insn,
						  REG_MMX_OFFSET );
			break;
		case ADDRMETH_R:
			size = ia32_modrm_decode( buf, buf_len, op, insn,
						  gen_regs );
			break;
		case ADDRMETH_W:
			size = ia32_modrm_decode( buf, buf_len, op, insn,
						  REG_SIMD_OFFSET );
			break;

		case ADDRMETH_C:
			ia32_reg_decode( modrm, op, REG_CTRL_OFFSET );
			break;
		case ADDRMETH_D:
			ia32_reg_decode( modrm, op, REG_DEBUG_OFFSET );
			break;
		case ADDRMETH_G:
			ia32_reg_decode( modrm, op, gen_regs );
			break;
		case ADDRMETH_P:
			ia32_reg_decode( modrm, op, REG_MMX_OFFSET );
			break;
		case ADDRMETH_S:
			ia32_reg_decode( modrm, op, REG_SEG_OFFSET );
			break;
		case ADDRMETH_T:
			ia32_reg_decode( modrm, op, REG_TEST_OFFSET );
			break;
		case ADDRMETH_V:
			ia32_reg_decode( modrm, op, REG_SIMD_OFFSET );
			break;

		case ADDRMETH_A:
			op->type = op_absolute;

			x86_imm_sized( buf, buf_len,
				       &op->data.absolute.segment, 2 );
			if ( insn->addr_size == 4 ) {
				x86_imm_sized( buf, buf_len,
				    &op->data.absolute.offset.off32, 4 );
				size = 6;
			} else {
				x86_imm_sized( buf, buf_len,
				    &op->data.absolute.offset.off16, 2 );
				size = 4;
			}

			break;
		case ADDRMETH_I:
			op->type = op_immediate;

			if ( op->flags & op_signed ) {
				x86_imm_signsized(buf, buf_len, &op->data.byte,
						op_size);
			} else {
				x86_imm_sized(buf, buf_len, &op->data.byte,
						op_size);
			}
			size = op_size;
			break;
		case ADDRMETH_J:

			op->flags |= op_signed;
			if ( op_size == 1 ) {

				op->type = op_relative_near;
				x86_imm_signsized(buf, buf_len,
						&op->data.relative_near, 1);
			} else {

				op->type = op_relative_far;
				x86_imm_signsized(buf, buf_len,
					&op->data.relative_far, op_size );
			}
			size = op_size;
			break;
		case ADDRMETH_O:

			op->type = op_offset;
			op->flags |= op_pointer;
			x86_imm_sized( buf, buf_len, &op->data.offset,
					insn->addr_size );

			size = insn->addr_size;
			break;

		case ADDRMETH_F:
			op->type = op_register;
			op->flags |= op_hardcode;
			ia32_handle_register( &op->data.reg, REG_FLAGS_INDEX );
			break;
		case ADDRMETH_X:
			op->type = op_expression;
			op->flags |= op_hardcode;
			op->flags |= op_ds_seg | op_pointer | op_string;
			ia32_handle_register( &op->data.expression.base,
					     REG_DWORD_OFFSET + 6 );
			break;
		case ADDRMETH_Y:
			op->type = op_expression;
			op->flags |= op_hardcode;
			op->flags |= op_es_seg | op_pointer | op_string;
			ia32_handle_register( &op->data.expression.base,
					     REG_DWORD_OFFSET + 7 );
			break;
		case ADDRMETH_RR:
			op->type = op_register;
			op->flags |= op_hardcode;
			ia32_handle_register( &op->data.reg,
						op_value + gen_regs );
			break;
		case ADDRMETH_RS:
			op->type = op_register;
			op->flags |= op_hardcode;
			ia32_handle_register( &op->data.reg,
						op_value + REG_SEG_OFFSET );
			break;
		case ADDRMETH_RF:
			op->type = op_register;
			op->flags |= op_hardcode;
			ia32_handle_register( &op->data.reg,
						op_value + REG_FPU_OFFSET );
			break;
		case ADDRMETH_RT:
			op->type = op_register;
			op->flags |= op_hardcode;
			ia32_handle_register( &op->data.reg,
						op_value + REG_TEST_OFFSET );
			break;
		case ADDRMETH_II:
			op->type = op_immediate;
			op->data.dword = op_value;
			op->flags |= op_hardcode;
			break;

		case 0:
		default:

			op->type = op_unused;
			break;
	}

	return size;
}

static size_t decode_operand_size( unsigned int op_type, x86_insn_t *insn,
				   x86_op_t *op ){
	size_t size;

	switch (op_type) {

		case OPTYPE_c:
			size = (insn->op_size == 4) ? 2 : 1;
			op->datatype = (size == 4) ? op_word : op_byte;
			break;
		case OPTYPE_a:

			size = (insn->op_size == 4) ? 8 : 4;
			op->datatype = (size == 4) ? op_bounds32 : op_bounds16;
			break;
		case OPTYPE_v:
			size = (insn->op_size == 4) ? 4 : 2;
			op->datatype = (size == 4) ? op_dword : op_word;
			break;
		case OPTYPE_p:

			size = (insn->addr_size == 4) ? 6 : 4;
			op->datatype = (size == 4) ? op_descr32 : op_descr16;
			break;
		case OPTYPE_b:
			size = 1;
			op->datatype = op_byte;
			break;
		case OPTYPE_w:
			size = 2;
			op->datatype = op_word;
			break;
		case OPTYPE_d:
			size = 4;
			op->datatype = op_dword;
			break;
		case OPTYPE_s:

			size = 6;
			op->datatype = (insn->addr_size == 4) ?
				op_pdescr32 : op_pdescr16;
			break;
		case OPTYPE_q:
			size = 8;
			op->datatype = op_qword;
			break;
		case OPTYPE_dq:
			size = 16;
			op->datatype = op_dqword;
			break;
		case OPTYPE_ps:
			size = 16;

			op->datatype = op_ssimd;
			break;
		case OPTYPE_pd:
			size = 16;

			op->datatype = op_dsimd;
			break;
		case OPTYPE_ss:
			size = 16;

			op->datatype = op_sssimd;
			break;
		case OPTYPE_sd:
			size = 16;

			op->datatype = op_sdsimd;
			break;
		case OPTYPE_pi:
			size = 8;
			op->datatype = op_qword;
			break;
		case OPTYPE_si:
			size = 4;
			op->datatype = op_dword;
			break;
		case OPTYPE_fs:
			size = 4;
			op->datatype = op_sreal;
			break;
		case OPTYPE_fd:
			size = 8;
			op->datatype = op_dreal;
			break;
		case OPTYPE_fe:
			size = 10;
			op->datatype = op_extreal;
			break;
		case OPTYPE_fb:
			size = 10;
			op->datatype = op_bcd;
			break;
		case OPTYPE_fv:
			size = (insn->addr_size == 4)? 28 : 14;
			op->datatype = (size == 28)?  op_fpuenv32: op_fpuenv16;
			break;
		case OPTYPE_ft:
			size = (insn->addr_size == 4)? 108 : 94;
			op->datatype = (size == 108)?
				op_fpustate32: op_fpustate16;
			break;
		case OPTYPE_fx:
			size = 512;
			op->datatype = op_fpregset;
			break;
		case OPTYPE_fp:
			size = 10;
			op->datatype = op_fpreg;
			break;
		case OPTYPE_m:
			size = insn->addr_size;
			op->datatype = (size == 4) ?  op_dword : op_word;
			break;
		case OPTYPE_none:
			size = 0;
			op->datatype = op_none;
			break;
		case 0:
		default:
			size = insn->op_size;
			op->datatype = (size == 4) ? op_dword : op_word;
			break;
		}
	return size;
}

size_t ia32_decode_operand( unsigned char *buf, size_t buf_len,
			      x86_insn_t *insn, unsigned int raw_op,
			      unsigned int raw_flags, unsigned int prefixes,
			      unsigned char modrm ) {
	unsigned int addr_meth, op_type, op_size, gen_regs;
	x86_op_t *op;
	size_t size;

	addr_meth = raw_flags & ADDRMETH_MASK;
	op_type = raw_flags & OPTYPE_MASK;

	if ( raw_flags == ARG_NONE ) {

		return 0;
	}

	op = x86_operand_new( insn );

	op->access = (enum x86_op_access) OP_PERM(raw_flags);
	op->flags = (enum x86_op_flags) (OP_FLAGS(raw_flags) >> 12);

	op_size = decode_operand_size(op_type, insn, op);

	if (op_size == 1) {
		gen_regs = REG_BYTE_OFFSET;
	} else if (op_size == 2) {
		gen_regs = REG_WORD_OFFSET;
	} else {
		gen_regs = REG_DWORD_OFFSET;
	}

	size = decode_operand_value( buf, buf_len, op, insn, addr_meth,
				      op_size, raw_op, modrm, gen_regs );

	if ( op->type == op_expression || op->type == op_offset ) {
		apply_seg(op, prefixes);
	}

	return size;
}
