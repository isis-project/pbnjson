/* @@@LICENSE
*
*      Copyright (c) 2012 Hewlett-Packard Development Company, L.P.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
LICENSE@@@ */

#ifndef LIB_LOG_H_
#define LIB_LOG_H_

#include <japi.h>
#include <stdlib.h>
#include <pjson_syslog.h>
#include <compiler/format_attribute.h>
#include <compiler/throw_attribute.h>
#include <compiler/noreturn_attribute.h>
#include <compiler/builtins.h>
#include <assert_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PJSON_EMPTY_VAR
#define PJSON_NOOP do {} while (0)

#if HAVE_VSYSLOG || HAVE_VFPRINTF
#define HAVE_LOG_TARGET 1
#endif

#if HAVE_LOG_TARGET
PJSON_LOCAL void log_info(const char *path, int line, const char *message, ...) PRINTF_FORMAT_FUNC(3, 4);
PJSON_LOCAL void log_warn(const char *path, int line, const char *message, ...) PRINTF_FORMAT_FUNC(3, 4);
PJSON_LOCAL void log_fatal(const char *path, int line, const char *message, ...) PRINTF_FORMAT_FUNC(3, 4);
#else
// no way to print anything
static inline void log_info(const char *path, int line, const char *message, ...){}
static inline void log_warn(const char *path, int line, const char *message, ...){}
static inline void log_fatal(const char *path, int line, const char *message, ...){}
#endif /* HAVE_LOG_TARGET */

#ifdef NDEBUG
#undef PJSON_LOG_DBG
#undef PJSON_LOG_TRACE
#undef PJSON_SCHEMA_TRACE
#undef DEBUG_FREED_POINTERS
#endif

#if DEBUG_FREED_POINTERS
#define FREED_POINTER (void *)0xdeadbeef
#else
#undef FREED_POINTER
#endif

#if PJSON_LOG_TRACE
#define PJ_LOG_TRACE(format, ...) log_info(__FILE__, __LINE__, " %s " format, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#define PJ_LOG_TRACE(format, ...) PJSON_NOOP
#endif

#if PJSON_SCHEMA_TRACE >= 1
#define PJ_SCHEMA_TRACE PJ_LOG_TRACE
#else
#define PJ_SCHEMA_TRACE(format, ...) PJSON_NOOP
#endif

#if PJSON_SCHEMA_TRACE >= 2
#define PJ_SCHEMA_TRACIER PJ_LOG_TRACE
#else
#define PJ_SCHEMA_TRACIER(format, ...) PJSON_NOOP
#endif

#if PJSON_SCHEMA_TRACE >= 3
#define PJ_SCHEMA_TRACIEST PJ_LOG_TRACE
#else
#define PJ_SCHEMA_TRACIEST(format, ...) PJSON_NOOP
#endif

#if PJSON_LOG_DBG
#define PJ_LOG_DBG(format, ...) log_info(__FILE__, __LINE__, format, ##__VA_ARGS__ )
#else
#define PJ_LOG_DBG(format, ...) PJSON_NOOP
#endif

#if PJSON_LOG_INFO && !PJSON_NO_LOGGING
#define PJ_LOG_INFO(format, ...) log_info(__FILE__, __LINE__, format, ##__VA_ARGS__ )
#else
#define PJ_LOG_INFO(format, ...) PJSON_NOOP
#endif /* PJSON_NO_LOGGING */

#if !PJSON_NO_LOGGING
#define PJ_LOG_WARN(format, ...) log_warn(__FILE__, __LINE__, format, ##__VA_ARGS__)
#define PJ_LOG_ERR(format, ...) log_fatal(__FILE__, __LINE__, format, ##__VA_ARGS__)
#else
#define PJ_LOG_WARN(format, ...) PJSON_NOOP
#define PJ_LOG_ERR(format, ...) PJSON_NOOP
#endif /* PJSON_NO_LOGGING */

#define PJ_SCHEMA_DBG(...) PJ_LOG_DBG(__VA_ARGS__)
#define PJ_SCHEMA_INFO(...) PJ_LOG_INFO(__VA_ARGS__)
#define PJ_SCHEMA_WARN(...) PJ_LOG_WARN(__VA_ARGS__)
#define PJ_SCHEMA_ERR(...) PJ_LOG_ERR(__VA_ARGS__)

#define CHECK_CONDITION_RETURN_VALUE(errorCondition, returnValue, format, ...)					\
	if (UNLIKELY(errorCondition)) {										\
		PJ_LOG_ERR(format, ##__VA_ARGS__);								\
		return returnValue;										\
	}

#define CHECK_CONDITION_RETURN(errorCondition, format, ...) 							\
	CHECK_CONDITION_RETURN_VALUE(errorCondition, PJSON_EMPTY_VAR, format, ##__VA_ARGS__)

#define CHECK_POINTER_MSG_RETURN_VALUE(pointer, value, format, ...)						\
	CHECK_CONDITION_RETURN_VALUE((pointer) == NULL, value, format, ##__VA_ARGS__)

#define CHECK_POINTER_RETURN_VALUE(pointer, value) 								\
	do {													\
		SANITY_CHECK_POINTER((pointer));								\
		CHECK_CONDITION_RETURN_VALUE((pointer) == NULL, value, "Invalid API use: null pointer");	\
	} while(0)

#define CHECK_POINTER(pointer) CHECK_POINTER_RETURN_VALUE(pointer, PJSON_EMPTY_VAR)
#define CHECK_POINTER_RETURN(pointer) CHECK_POINTER_RETURN_VALUE(pointer, pointer)
#define CHECK_POINTER_RETURN_NULL(pointer) CHECK_POINTER_RETURN_VALUE(pointer, NULL)
#define CHECK_POINTER_MSG(pointer, format, ...) CHECK_POINTER_MSG_RETURN_VALUE(pointer, PJSON_EMPTY_VAR, format, ##__VA_ARGS__)
#define CHECK_POINTER_MSG_RETURN_NULL(pointer, format, ...) CHECK_POINTER_MSG_RETURN_VALUE(pointer, NULL, format, ##__VA_ARGS__)
#define CHECK_ALLOC(pointer) CHECK_POINTER_MSG(pointer, "Out of memory")
#define CHECK_ALLOC_RETURN_VALUE(pointer, value) CHECK_POINTER_MSG_RETURN_VALUE(pointer, value, "Out of memory")
#define CHECK_ALLOC_RETURN_NULL(pointer) CHECK_POINTER_MSG_RETURN_NULL(pointer, "Out of memory")

/**
 * SANITY_FREE & SANITY_FREE_CUST are not not side-effect safe macros
 */
#define SANITY_FREE(dealloc, type, pointer, length, ...)		\
		do {							\
			SANITY_CHECK_POINTER(dealloc);			\
			SANITY_CHECK_POINTER(pointer);			\
			SANITY_CLEAR_MEMORY(pointer, length);		\
			dealloc((type)(pointer), ##__VA_ARGS__);	\
			SANITY_KILL_POINTER(pointer);			\
		} while(0)

#define SANITY_FREE_CUST(dealloc, type, pointer, length, ...)				\
		do {									\
			SANITY_FREE(dealloc, type, pointer, length, ##__VA_ARGS__);	\
			SANITY_KILL_POINTER(dealloc);					\
		} while(0)

#if DEBUG_FREED_POINTERS
#define SANITY_CLEAR_VAR(variable, value) (variable) = (value)
#define SANITY_CHECK_POINTER(pointer)													\
		do {															\
			if (UNLIKELY((pointer) == FREED_POINTER)) {									\
				PJ_LOG_ERR("Attempting to use pointer %p that has already been freed", (pointer));			\
				abort();												\
			}														\
			if (UNLIKELY((void *)(pointer) < (void *)1024) && pointer != NULL) {						\
				PJ_LOG_ERR("Invalid pointer %p assuming that first page is always unmapped", (void *)(pointer)); 	\
				abort();												\
			}														\
		}															\
		while (0)
#define SANITY_KILL_POINTER(pointer) SANITY_CLEAR_VAR(pointer, FREED_POINTER)
#define SANITY_CLEAR_MEMORY(memory, length)					\
	do {									\
		char value[] = {0xde, 0xad, 0xbe, 0xef };			\
		char *mem = (char *)memory;					\
		for (int i = 0; i < length; i++)				\
			SANITY_CLEAR_VAR(mem[i], value[i%sizeof(value)]);	\
	} while (0)
#define SANITY_CHECK_MEMORY(memory, length)					\
	do {									\
		char value[] = {0xde, 0xad, 0xbe, 0xef };			\
		char *mem = (char *)memory;					\
		bool ok = length == 0;						\
		for (int i = 0; i < length; i++) {				\
			if (mem[i] != value[i % 4]) {ok = true; break; }	\
		}								\
		if (!ok) {							\
			PJ_LOG_ERR("Attempting to use memory at %p"		\
				   " that has already been freed", memory);	\
			abort();						\
		}								\
	} while(0)
#else
#define SANITY_CLEAR_VAR(variable, value) PJSON_NOOP
#define SANITY_CHECK_POINTER(pointer) PJSON_NOOP
#define SANITY_KILL_POINTER(pointer) PJSON_NOOP
#define SANITY_CLEAR_MEMORY(memory, length) PJSON_NOOP
#define SANITY_CHECK_MEMORY(memory, length) PJSON_NOOP
#endif /* DEBUG_FREED_POINTERS */

#define RB_PRINTF(raw_buffer) (int) ((raw_buffer).m_len), (raw_buffer).m_str

#ifdef __cplusplus
}
#endif

#endif /* LIB_LOG_H_ */
