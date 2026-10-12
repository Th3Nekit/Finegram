#include <stdlib.h>

#include "ia32_implicit.h"
#include "ia32_insn.h"
#include "ia32_reg.h"
#include "x86_operand_list.h"

typedef struct {
	uint32_t type;
	uint32_t operand;
} op_implicit_list_t;

static op_implicit_list_t list_aaa[] =

	{{ OP_R | OP_W, REG_BYTE_OFFSET }, {0}};

static op_implicit_list_t list_aad[] =

	{{ OP_R | OP_W, REG_WORD_OFFSET }, {0}};

static op_implicit_list_t list_call[] =

	{{ OP_R | OP_W, REG_EIP_INDEX },
	 { OP_R | OP_W, REG_ESP_INDEX }, {0}};

static op_implicit_list_t list_cbw[] =

	{{ OP_R | OP_W, REG_WORD_OFFSET },
	 { OP_R, REG_BYTE_OFFSET}, {0}};

static op_implicit_list_t list_cwde[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET },
	 { OP_R, REG_WORD_OFFSET }, {0}};

static op_implicit_list_t list_clts[] =

	{{ OP_R | OP_W, REG_CTRL_OFFSET}, {0}};

static op_implicit_list_t list_cmpxchg[] =

	{{ OP_R | OP_W, REG_BYTE_OFFSET }, {0}};

static op_implicit_list_t list_cmpxchgb[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_cmpxchg8b[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET },
	 { OP_R | OP_W, REG_DWORD_OFFSET + 2 },
	 { OP_R, REG_DWORD_OFFSET + 1 },
	 { OP_R, REG_DWORD_OFFSET + 3 }, {0}};

static op_implicit_list_t list_cpuid[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET },
	 { OP_W, REG_DWORD_OFFSET + 1 },
	 { OP_W, REG_DWORD_OFFSET + 2 },
	 { OP_W, REG_DWORD_OFFSET + 3 }, {0}};

static op_implicit_list_t list_cwd[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET },
	 { OP_W, REG_DWORD_OFFSET + 2 }, {0}};

static op_implicit_list_t list_daa[] =

	{{ OP_R | OP_W, REG_BYTE_OFFSET }, {0}};

static op_implicit_list_t list_idiv[] =

	{{ OP_R, REG_WORD_OFFSET },
	  { OP_W, REG_BYTE_OFFSET },
	  { OP_W, REG_BYTE_OFFSET + 4 }, {0}};

static op_implicit_list_t list_div[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET + 2 },
	  { OP_R | OP_W, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_enter[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET + 4 },
	 { OP_R, REG_DWORD_OFFSET + 5 }, {0}};

static op_implicit_list_t list_f2xm1[] =

	{{ OP_R | OP_W, REG_FPU_OFFSET }, {0}};

static op_implicit_list_t list_fcom[] =

	{{ OP_R, REG_FPU_OFFSET }, {0}};

static op_implicit_list_t list_fpatan[] =

	{{ OP_R, REG_FPU_OFFSET }, {0}};

static op_implicit_list_t list_fprem[] =

	{{ OP_R | OP_W, REG_FPU_OFFSET },
	 { OP_R, REG_FPU_OFFSET + 1 }, {0}};

static op_implicit_list_t list_faddp[] =

	{{ OP_R, REG_FPU_OFFSET },
	 { OP_R | OP_W, REG_FPU_OFFSET + 1 }, {0}};

static op_implicit_list_t list_fucompp[] =

	{{ OP_R, REG_FPU_OFFSET },
	 { OP_R, REG_FPU_OFFSET + 1 }, {0}};

static op_implicit_list_t list_imul[] =

	{{ OP_R, REG_BYTE_OFFSET },
	 { OP_W, REG_WORD_OFFSET }, {0}};

static op_implicit_list_t list_mul[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET },
	 { OP_W, REG_DWORD_OFFSET + 2 }, {0}};

static op_implicit_list_t list_lahf[] =

	{{ OP_R, REG_FLAGS_INDEX },
	 { OP_W, REG_BYTE_OFFSET + 4 }, {0}};

static op_implicit_list_t list_ldmxcsr[] =

	{{ OP_W, REG_MXCSG_INDEX }, {0}};

static op_implicit_list_t list_leave[] =

	{{ OP_R | OP_W, REG_ESP_INDEX },
	 { OP_W, REG_DWORD_OFFSET + 5 }, {0}};

static op_implicit_list_t list_lgdt[] =

	{{ OP_W, REG_GDTR_INDEX }, {0}};

static op_implicit_list_t list_lidt[] =

	{{ OP_W, REG_IDTR_INDEX }, {0}};

static op_implicit_list_t list_lldt[] =

	{{ OP_W, REG_LDTR_INDEX }, {0}};

static op_implicit_list_t list_lmsw[] =

	{{ OP_W, REG_CTRL_OFFSET }, {0}};

static op_implicit_list_t list_loop[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET + 1 }, {0}};

static op_implicit_list_t list_ltr[] =

	{{ OP_W, REG_TR_INDEX }, {0}};

static op_implicit_list_t list_pop[] =

	{{ OP_R | OP_W, REG_ESP_INDEX }, {0}};

static op_implicit_list_t list_popad[] =

	{{ OP_R | OP_W, REG_ESP_INDEX },
	 { OP_W, REG_DWORD_OFFSET + 7 },
	 { OP_W, REG_DWORD_OFFSET + 6 },
	 { OP_W, REG_DWORD_OFFSET + 5 },
	 { OP_W, REG_DWORD_OFFSET + 3 },
	 { OP_W, REG_DWORD_OFFSET + 2 },
	 { OP_W, REG_DWORD_OFFSET + 1 },
	 { OP_W, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_popfd[] =

	{{ OP_R | OP_W, REG_ESP_INDEX },
	 { OP_W, REG_FLAGS_INDEX }, {0}};

static op_implicit_list_t list_pushad[] =

	{{ OP_R | OP_W, REG_ESP_INDEX },
	 { OP_R, REG_DWORD_OFFSET },
	 { OP_R, REG_DWORD_OFFSET + 1 },
	 { OP_R, REG_DWORD_OFFSET + 2 },
	 { OP_R, REG_DWORD_OFFSET + 3 },
	 { OP_R, REG_DWORD_OFFSET + 5 },
	 { OP_R, REG_DWORD_OFFSET + 6 },
	 { OP_R, REG_DWORD_OFFSET + 7 }, {0}};

static op_implicit_list_t list_pushfd[] =

	{{ OP_R | OP_W, REG_ESP_INDEX },
	 { OP_R, REG_FLAGS_INDEX }, {0}};

static op_implicit_list_t list_rdmsr[] =

	{{ OP_R, REG_DWORD_OFFSET + 1 },
	 { OP_W, REG_DWORD_OFFSET + 2 },
	 { OP_W, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_rdpmc[] =

	{{ OP_R, REG_DWORD_OFFSET + 1 },
	 { OP_W, REG_DWORD_OFFSET + 2 },
	 { OP_W, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_rdtsc[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET + 2 },
	 { OP_R | OP_W, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_rep[] =

	{{ OP_R | OP_W, REG_DWORD_OFFSET + 1 }, {0}};

static op_implicit_list_t list_rsm[] =

	{{ OP_R, REG_CTRL_OFFSET + 4 },
	 { OP_R, REG_CTRL_OFFSET }, {0}};

static op_implicit_list_t list_sahf[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_sgdt[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_sidt[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_sldt[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_smsw[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_stmxcsr[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_str[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_sysenter[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_sysexit[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_wrmsr[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_xlat[] =

	{{ OP_R, REG_DWORD_OFFSET }, {0}};

static op_implicit_list_t list_monitor[] =
	{{ OP_R, REG_DWORD_OFFSET }, {0}};
static op_implicit_list_t list_mwait[] =
	{{ OP_R, REG_DWORD_OFFSET }, {0}};

op_implicit_list_t *op_implicit_list[] = {

	NULL,
	list_aaa, list_aad, list_call, list_cbw,
	list_cwde, list_clts, list_cmpxchg, list_cmpxchgb,
	list_cmpxchg8b, list_cpuid, list_cwd, list_daa,
	list_idiv, list_div, list_enter, list_f2xm1,
	list_fcom, list_fpatan, list_fprem, list_faddp,
	list_fucompp, list_imul, list_mul, list_lahf,
	list_ldmxcsr, list_leave, list_lgdt, list_lidt,
	list_lldt, list_lmsw, list_loop, list_ltr,
	list_pop, list_popad, list_popfd, list_pushad,
	list_pushfd, list_rdmsr, list_rdpmc, list_rdtsc,

	list_rep, list_rsm, list_sahf, list_sgdt,
	list_sidt, list_sldt, list_smsw, list_stmxcsr,
	list_str, list_sysenter, list_sysexit, list_wrmsr,
	list_xlat, list_monitor, list_mwait,
	NULL
 };

#define LAST_IMPL_IDX 55

static void handle_impl_reg( x86_op_t *op, uint32_t val ) {
	x86_reg_t *reg = &op->data.reg;
	op->type = op_register;
	ia32_handle_register( reg, (unsigned int) val );
	switch (reg->size) {
		case 1:
			op->datatype = op_byte; break;
		case 2:
			op->datatype = op_word; break;
		case 4:
			op->datatype = op_dword; break;
		case 8:
			op->datatype = op_qword; break;
		case 10:
			op->datatype = op_extreal; break;
		case 16:
			op->datatype = op_dqword; break;
	}
	return;
}

unsigned int ia32_insn_implicit_ops( x86_insn_t *insn, unsigned int impl_idx ) {
	op_implicit_list_t *list;
	x86_op_t *op;
	unsigned int num = 0;

	if (! impl_idx || impl_idx > LAST_IMPL_IDX ) {
		return 0;
	}

	for ( list = op_implicit_list[impl_idx]; list->type; list++, num++ ) {
		enum x86_op_access access = (enum x86_op_access) OP_PERM(list->type);
		enum x86_op_flags  flags  = (enum x86_op_flags) (OP_FLAGS(list->type) >> 12);

		op = NULL;

		x86_oplist_t * existing;
		if (ia32_true_register_id(list->operand) == REG_DWORD_OFFSET) {
			for ( existing = insn->operands; existing; existing = existing->next ) {
				if (existing->op.type == op_register &&
	                            existing->op.data.reg.id == list->operand) {
					op = &existing->op;
					break;
				}
			}
		}
		if (!op) {
			op = x86_operand_new( insn );

			handle_impl_reg( op, list->operand );

			insn->explicit_count = insn->explicit_count -1;
		}
		if (!op) {
			return num;
		}
		op->access |= access;
		op->flags |= flags;
		op->flags |= op_implied;
	}

	return num;
}
