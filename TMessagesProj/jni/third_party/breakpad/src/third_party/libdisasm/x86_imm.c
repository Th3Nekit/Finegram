#include "qword.h"
#include "x86_imm.h"

#include <stdio.h>

unsigned int x86_imm_signsized( unsigned char * buf, size_t buf_len,
				void *dest, unsigned int size ) {
	signed char *cp = (signed char *) dest;
	signed short *sp = (signed short *) dest;
	int32_t *lp = (int32_t *) dest;
	qword_t *qp = (qword_t *) dest;

	if ( size > buf_len ) {
		return 0;
	}

	switch (size) {
		case 1:
			*cp =  *((signed char *) buf);
			break;
		case 2:
			*sp =  *((signed short *) buf);
			break;
		case 6:
		case 8:
			*qp = *((qword_t *) buf);
			break;
		case 4:
		default:
			*lp = *((int32_t *) buf);
			break;
	}
	return (size);
}

unsigned int x86_imm_sized( unsigned char * buf, size_t buf_len, void *dest,
			    unsigned int size ) {
	unsigned char *cp = (unsigned char *) dest;
	unsigned short *sp = (unsigned short *) dest;
	uint32_t *lp = (uint32_t *) dest;
	qword_t *qp = (qword_t *) dest;

	if ( size > buf_len ) {
		return 0;
	}

	switch (size) {
		case 1:
			*cp =  *((unsigned char *) buf);
			break;
		case 2:
			*sp =  *((unsigned short *) buf);
			break;
		case 6:
		case 8:
			*qp = *((qword_t *) buf);
			break;
		case 4:
		default:
			*lp = *((uint32_t *) buf);
			break;
	}

	return (size);
}
