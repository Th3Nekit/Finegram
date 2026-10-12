#include "ia32_modrm.h"
#include "ia32_reg.h"
#include "x86_imm.h"

#define MODRM_RM_SIB            0x04
#define MODRM_RM_NOREG          0x05

#define MODRM_MOD_NODISP        0x00
#define MODRM_MOD_DISP8         0x01
#define MODRM_MOD_DISP32        0x02
#define MODRM_MOD_NOEA          0x03

#define MOD16_MOD_NODISP      0
#define MOD16_MOD_DISP8       1
#define MOD16_MOD_DISP16      2
#define MOD16_MOD_REG         3

#define MOD16_RM_BXSI         0
#define MOD16_RM_BXDI         1
#define MOD16_RM_BPSI         2
#define MOD16_RM_BPDI         3
#define MOD16_RM_SI           4
#define MOD16_RM_DI           5
#define MOD16_RM_BP           6
#define MOD16_RM_BX           7

#define SIB_INDEX_NONE       0x04
#define SIB_BASE_EBP       0x05
#define SIB_SCALE_NOBASE    0x00

struct modRM_byte {
   unsigned int mod : 2;
   unsigned int reg : 3;
   unsigned int rm  : 3;
};

struct SIB_byte {
   unsigned int scale : 2;
   unsigned int index : 3;
   unsigned int base  : 3;
};

#if 0
int modrm_rm[] = {0,1,2,3,MODRM_RM_SIB,MODRM_MOD_DISP32,6,7};
int modrm_reg[] = {0, 1, 2, 3, 4, 5, 6, 7};
int modrm_mod[]  = {0, MODRM_MOD_DISP8, MODRM_MOD_DISP32, MODRM_MOD_NOEA};
int sib_scl[] = {0, 2, 4, 8};
int sib_idx[] = {0, 1, 2, 3, SIB_INDEX_NONE, 5, 6, 7 };
int sib_bas[] = {0, 1, 2, 3, 4, SIB_SCALE_NOBASE, 6, 7 };
#endif

static unsigned int imm32_signsized( unsigned char *buf, size_t buf_len,
				     int32_t *dest, unsigned int size ) {
	if ( size > buf_len ) {
		return 0;
	}

	switch (size) {
		case 1:
			*dest = *((signed char *) buf);
			break;
		case 2:
			*dest = *((signed short *) buf);
			break;
		case 4:
		default:
			*dest = *((signed int *) buf);
			break;
	}

	return size;
}

static void byte_decode(unsigned char b, struct modRM_byte *modrm) {

	modrm->mod = b >> 6;
	modrm->reg = (b & 56) >> 3;
	modrm->rm = b & 7;
}

static size_t sib_decode( unsigned char *buf, size_t buf_len, x86_ea_t *ea,
			  unsigned int mod ) {

	size_t size = 1;
	struct SIB_byte sib;

	if ( buf_len < 1 ) {
		return 0;
	}

	byte_decode( *buf, (struct modRM_byte *)(void*)&sib );

	if ( sib.base == SIB_BASE_EBP && ! mod ) {

		imm32_signsized( &buf[1], buf_len, &ea->disp, sizeof(int32_t));
		ea->disp_size = sizeof(int32_t);
		ea->disp_sign = (ea->disp < 0) ? 1 : 0;
		size += 4;

	} else {

		ia32_handle_register( &ea->base, sib.base + 1 );
	}

	ea->scale = 1 << sib.scale;

	if (sib.index != SIB_INDEX_NONE) {

		ia32_handle_register( &ea->index, sib.index + 1 );
	}

	return (size);
}

static size_t modrm_decode16( unsigned char *buf, unsigned int buf_len,
			    x86_op_t *op, struct modRM_byte *modrm ) {

	size_t size = 1;
	x86_ea_t * ea = &op->data.expression;

	switch( modrm->rm ) {
		case MOD16_RM_BXSI:
			ia32_handle_register(&ea->base, REG_WORD_OFFSET + 3);
			ia32_handle_register(&ea->index, REG_WORD_OFFSET + 6);
			break;
		case MOD16_RM_BXDI:
			ia32_handle_register(&ea->base, REG_WORD_OFFSET + 3);
			ia32_handle_register(&ea->index, REG_WORD_OFFSET + 7);
		case MOD16_RM_BPSI:
			op->flags |= op_ss_seg;
			ia32_handle_register(&ea->base, REG_WORD_OFFSET + 5);
			ia32_handle_register(&ea->index, REG_WORD_OFFSET + 6);
			break;
		case MOD16_RM_BPDI:
			op->flags |= op_ss_seg;
			ia32_handle_register(&ea->base, REG_WORD_OFFSET + 5);
			ia32_handle_register(&ea->index, REG_WORD_OFFSET + 7);
			break;
		case MOD16_RM_SI:
			ia32_handle_register(&ea->base, REG_WORD_OFFSET + 6);
			break;
		case MOD16_RM_DI:
			ia32_handle_register(&ea->base, REG_WORD_OFFSET + 7);
			break;
		case MOD16_RM_BP:
			if ( modrm->mod != MOD16_MOD_NODISP ) {
				op->flags |= op_ss_seg;
				ia32_handle_register(&ea->base,
						     REG_WORD_OFFSET + 5);
			}
			break;
		case MOD16_RM_BX:
			ia32_handle_register(&ea->base, REG_WORD_OFFSET + 3);
			break;
	}

	++buf;
	--buf_len;

	if ( modrm->mod == MOD16_MOD_DISP8 ) {
		imm32_signsized( buf, buf_len, &ea->disp, sizeof(char) );
		ea->disp_sign = (ea->disp < 0) ? 1 : 0;
		ea->disp_size = sizeof(char);
		size += sizeof(char);
	} else if ( modrm->mod == MOD16_MOD_DISP16 ) {
		imm32_signsized( buf, buf_len, &ea->disp, sizeof(short) );
		ea->disp_sign = (ea->disp < 0) ? 1 : 0;
		ea->disp_size = sizeof(short);
		size += sizeof(short);
	}

	return size;
}

size_t ia32_modrm_decode( unsigned char *buf, unsigned int buf_len,
			    x86_op_t *op, x86_insn_t *insn, size_t gen_regs ) {

	struct modRM_byte modrm;
	size_t size = 1;
	x86_ea_t * ea;

	byte_decode(*buf, &modrm);

	if ( modrm.mod == MODRM_MOD_NOEA ) {
		op->type = op_register;
		ia32_handle_register(&op->data.reg, modrm.rm + gen_regs);

 		return 1;
 	}

	ea = &op->data.expression;
	op->type = op_expression;
	op->flags |= op_pointer;

	if ( insn->addr_size == 2 ) {

		return modrm_decode16( buf, buf_len, op, &modrm);
	}

	++buf;
	--buf_len;

	if (modrm.mod == MODRM_MOD_NODISP) {

		if (modrm.rm == MODRM_RM_NOREG) {

			imm32_signsized( buf, buf_len, &ea->disp,
					sizeof(int32_t) );
			ea->disp_size = sizeof(int32_t);
			ea->disp_sign = (ea->disp < 0) ? 1 : 0;
			size += 4;

		} else if (modrm.rm == MODRM_RM_SIB) {

			size += sib_decode( buf, buf_len, ea, modrm.mod);

			++buf;
			--buf_len;
		} else {

			ia32_handle_register( &ea->base, modrm.rm + 1 );
		}
	} else {
		if (modrm.rm == MODRM_RM_SIB) {

			size += sib_decode( buf, buf_len, ea, modrm.mod);

			++buf;
			--buf_len;
		} else {

			ia32_handle_register( &ea->base, modrm.rm + 1 );
		}

		if (modrm.mod == MODRM_MOD_DISP8) {

			imm32_signsized( buf, buf_len, &ea->disp,
					sizeof(char));
			ea->disp_size = sizeof(char);
			ea->disp_sign = (ea->disp < 0) ? 1 : 0;
			size += 1;

		} else {

			imm32_signsized( buf, buf_len, &ea->disp,
					insn->addr_size);
			ea->disp_size = insn->addr_size;
			ea->disp_sign = (ea->disp < 0) ? 1 : 0;
			size += 4;
		}
	}

	return size;
}

void ia32_reg_decode( unsigned char byte, x86_op_t *op, size_t gen_regs ) {
	struct modRM_byte modrm;
	byte_decode( byte, &modrm );

	op->type = op_register;
	ia32_handle_register(&op->data.reg, modrm.reg + gen_regs);

	return;
}
