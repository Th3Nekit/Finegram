#ifndef LIBDISASM_QWORD_H
#define LIBDISASM_QWORD_H

#include <stdint.h>

#ifdef _MSC_VER
	typedef __int64         qword_t;
#else
	typedef int64_t         qword_t;
#endif

#endif
