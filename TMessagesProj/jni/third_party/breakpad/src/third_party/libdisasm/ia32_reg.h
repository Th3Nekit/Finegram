#ifndef IA32_REG_H
#define IA32_REG_H

#include <sys/types.h>
#include "libdis.h"

#define REG_DWORD_OFFSET 	 1
#define REG_ECX_INDEX		 2
#define REG_ESP_INDEX		 5
#define REG_EBP_INDEX		 6
#define REG_ESI_INDEX		 7
#define REG_EDI_INDEX		 8
#define REG_WORD_OFFSET 	 9
#define REG_BYTE_OFFSET 	17
#define REG_MMX_OFFSET 		25
#define REG_SIMD_OFFSET 	33
#define REG_DEBUG_OFFSET 	41
#define REG_CTRL_OFFSET 	49
#define REG_TEST_OFFSET 	57
#define REG_SEG_OFFSET 		65
#define REG_LDTR_INDEX		71
#define REG_GDTR_INDEX		72
#define REG_FPU_OFFSET 		73
#define REG_FLAGS_INDEX 	81
#define REG_FPCTRL_INDEX 	82
#define REG_FPSTATUS_INDEX 	83
#define REG_FPTAG_INDEX 	84
#define REG_EIP_INDEX 		85
#define REG_IP_INDEX 		86
#define REG_IDTR_INDEX		87
#define REG_MXCSG_INDEX		88
#define REG_TR_INDEX		89
#define REG_CSMSR_INDEX		90
#define REG_ESPMSR_INDEX	91
#define REG_EIPMSR_INDEX	92

void ia32_handle_register( x86_reg_t *reg, size_t id );
size_t ia32_true_register_id( size_t id );

#endif
