#define GOOGLE_NAMESPACE google

#undef HAVE_DLADDR

#undef HAVE_DLFCN_H

#undef HAVE_EXECINFO_H

#undef HAVE_INTTYPES_H

#undef HAVE_LIBUNWIND_H

#undef HAVE_LIB_GFLAGS

#undef HAVE_LIB_UNWIND

#undef HAVE_MEMORY_H

#undef HAVE_NAMESPACES

#undef HAVE_PTHREAD

#undef HAVE_RWLOCK

#undef HAVE_SIGALTSTACK

#undef HAVE_STDINT_H

#undef HAVE_STDLIB_H

#undef HAVE_STRINGS_H

#undef HAVE_STRING_H

#undef HAVE_SYSCALL_H

#undef HAVE_SYS_STAT_H

#undef HAVE_SYS_SYSCALL_H

#undef HAVE_SYS_TYPES_H

#undef HAVE_UCONTEXT_H

#undef HAVE_UNISTD_H

#undef HAVE_USING_OPERATOR

#undef HAVE___ATTRIBUTE__

#undef HAVE___BUILTIN_EXPECT

#undef HAVE___SYNC_VAL_COMPARE_AND_SWAP

#undef PACKAGE

#undef PACKAGE_BUGREPORT

#undef PACKAGE_NAME

#undef PACKAGE_STRING

#undef PACKAGE_TARNAME

#undef PACKAGE_VERSION

#undef PC_FROM_UCONTEXT

#undef PTHREAD_CREATE_JOINABLE

#undef SIZEOF_VOID_P

#undef STDC_HEADERS

#undef STL_NAMESPACE

#undef VERSION

#define _END_GOOGLE_NAMESPACE_ }

#define _START_GOOGLE_NAMESPACE_ namespace google {

#ifndef GOOGLE_GLOG_DLL_DECL
# define GOOGLE_GLOG_IS_A_DLL  1
# define GOOGLE_GLOG_DLL_DECL  __declspec(dllexport)
# define GOOGLE_GLOG_DLL_DECL_FOR_UNITTESTS  __declspec(dllimport)
#endif
