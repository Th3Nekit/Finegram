#ifndef IA32_SETTINGS_H
#define IA32_SETTINGS_H

#include "libdis.h"

typedef struct {

	unsigned char endian,
		      wc_byte,
		      max_insn,
		      sz_addr,
		      sz_oper,
		      sz_byte,
		      sz_word,
		      sz_dword;
	unsigned int id_sp_reg,
		     id_fp_reg,
		     id_ip_reg,
		     id_flag_reg,
		     offset_gen_regs,
		     offset_seg_regs,
		     offset_fpu_regs;

	enum x86_options options;
} ia32_settings_t;

#endif
