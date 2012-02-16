#cmakedefine HAVE_EXECINFO_H 1

#cmakedefine HAVE_CXXABI_H_ 1

#if HAVE_EXECINFO_H
	#include <execinfo.h>
#endif

#if HAVE_CXXABI_H_
	#define DEMANGLE_CPP 1

	#include <cxxabi.h>
#ifdef __cplusplus
	using namespace abi;
#endif
#else
	#undef DEMANGLE_CPP
#endif
