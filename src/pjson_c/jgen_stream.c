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

#include "gen_stream.h"

#include <yajl/yajl_gen.h>
#include <stdio.h>
#include <string.h>
#include <sys_malloc.h>
#include <assert.h>
#include <inttypes.h>

#include <compiler/malloc_attribute.h>
#include <compiler/unused_attribute.h>

#include "liblog.h"

typedef struct PJSON_LOCAL {
	struct __JStream stream;
	TopLevelType opened;
	yajl_gen handle;
	StreamStatus error;
} ActualStream;

#define CHECK_HANDLE(stream) 							\
	do {									\
		if (stream->error != GEN_OK || stream->handle == NULL) {	\
			if (stream->error == GEN_OK) {				\
				stream->error = GEN_GENERIC_ERROR;		\
			}							\
			return stream;						\
		}								\
	} while(0)

// Why is this emitting a warning - is this due to the compiler version on OSX?
#if 0
#define INTERNAL_MALLOC_SIZE RETURN_SIZE2(2, 3)
#define INTERNAL_MALLOC_SIZE2 RETURN_SIZE(3)
#else
#define INTERNAL_MALLOC_SIZE
#define INTERNAL_MALLOC_SIZE2
#endif

// for some reason the return size is ignored
static void * pjson_internal_calloc(UNUSED_VAR void *ctx, size_t nmemb, size_t sz) MALLOC_FUNC INTERNAL_MALLOC_SIZE UNUSED_FUNC;
static void * pjson_internal_malloc(UNUSED_VAR void *ctx, size_t nmemb, size_t sz) MALLOC_FUNC INTERNAL_MALLOC_SIZE UNUSED_FUNC;
static void * pjson_internal_realloc(UNUSED_VAR void *ctx, void *ptr, unsigned int sz) UNUSED_FUNC INTERNAL_MALLOC_SIZE2;

static void pjson_internal_free(UNUSED_VAR void *ctx, void * ptr) UNUSED_FUNC;

static void * pjson_internal_calloc(void *ctx, size_t nmemb, size_t sz)
{
	return calloc(nmemb, sz);
}

static void * pjson_internal_malloc(void *ctx, size_t nmemb, size_t sz)
{
	return malloc(sz * nmemb);
}

static void * pjson_internal_realloc(void *ctx, void *ptr, unsigned int sz)
{
	return realloc(ptr, sz);
}

static void pjson_internal_free(void *ctx, void * ptr)
{
	return free(ptr);
}

static ActualStream* begin_object(ActualStream* __stream)
{
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	yajl_gen_map_open(__stream->handle);
	return __stream;
}

static ActualStream* end_object(ActualStream* __stream)
{
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	yajl_gen_map_close(__stream->handle);
	return __stream;
}

static ActualStream* begin_array(ActualStream* __stream)
{
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	yajl_gen_array_open(__stream->handle);
	return __stream;
}

static ActualStream* end_array(ActualStream* __stream)
{
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	yajl_gen_array_close(__stream->handle);
	return __stream;
}

static ActualStream* val_num(ActualStream* __stream, raw_buffer numstr)
{
	SANITY_CHECK_POINTER(__stream);
	SANITY_CHECK_POINTER(numstr.m_str);
	assert (numstr.m_str != NULL);
	CHECK_HANDLE(__stream);
	yajl_gen_number(__stream->handle, numstr.m_str, numstr.m_len);
	return __stream;
}

static ActualStream* val_int(ActualStream* __stream, int64_t number)
{
	char buf[24];
	int printed;
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	printed = snprintf(buf, sizeof(buf), "%" PRId64, number);
	yajl_gen_number(__stream->handle, buf, printed);
	return __stream;
}

static ActualStream* val_dbl(ActualStream* __stream, double number)
{
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	// yajl doesn't print properly (%g doesn't seem to do what it claims to
	// do or something - fails for 42323.0234234)
	// let's work around it with the  raw interface by 
	char f[32];
	int len = snprintf(f, sizeof(f) - 1, "%.14lg", number); 
	yajl_gen_number(__stream->handle, f, len);
#ifdef _DEBUG
	const unsigned char *buffer;
	unsigned int bufLen;
	yajl_gen_get_buf(__stream->handle, &buffer, &bufLen);
#endif
	return __stream;
}

static ActualStream* val_str(ActualStream* __stream, raw_buffer str)
{
	SANITY_CHECK_POINTER(__stream);
	SANITY_CHECK_POINTER(str.m_str);
	assert(str.m_str != NULL);
	CHECK_HANDLE(__stream);
	yajl_gen_string(__stream->handle, (const unsigned char *)str.m_str, str.m_len);
	
	return __stream;
}

static ActualStream* val_bool(ActualStream* __stream, bool boolean)
{
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	yajl_gen_bool(__stream->handle, boolean);
	return __stream;
}

static ActualStream* val_null(ActualStream* __stream)
{
	SANITY_CHECK_POINTER(__stream);
	CHECK_HANDLE(__stream);
	yajl_gen_null(__stream->handle);
	return __stream;
}

static StreamStatus convert_error_code(yajl_gen_status raw_code)
{
	switch (raw_code) {
		case yajl_gen_generation_complete:
		case yajl_gen_status_ok:
			return GEN_OK;
		case yajl_gen_keys_must_be_strings:
			return GEN_KEYS_MUST_BE_STRINGS;
		case yajl_max_depth_exceeded:
		case yajl_gen_in_error_state:
		default:
			return GEN_GENERIC_ERROR;
	}
}

static char* finish_stream(ActualStream* __stream, StreamStatus *error_code)
{
	char *buf = NULL;
	unsigned int len;
	yajl_gen_status result;

	SANITY_CHECK_POINTER(__stream);
	SANITY_CHECK_POINTER(error_code);

	switch (__stream->opened) {
		case TOP_None:
			break;
		case TOP_Object:
			end_object(__stream);
			break;
		case TOP_Array:
			end_array(__stream);
			break;
		default:
			PJ_LOG_ERR("Invalid object type: %d", __stream->opened);
			if (error_code) *error_code = GEN_GENERIC_ERROR;
			goto stream_error;
	}

	if (!__stream->handle) {
		if (error_code) *error_code = GEN_GENERIC_ERROR;
		goto stream_error;
	}

	if (__stream->error == GEN_OK) {
		const unsigned char *yajlBuf;
		result = yajl_gen_get_buf(__stream->handle, &yajlBuf, &len);
		if (error_code) {
			*error_code = convert_error_code(result);
		}
		if (result != yajl_gen_status_ok && result != yajl_gen_generation_complete) {
			buf = NULL;
		} else {
			buf = calloc(len + 1, sizeof(char));
			if (LIKELY(buf != NULL)) {
				memcpy(buf, yajlBuf, len);
			}
		}
	} else if (error_code) {
		*error_code = __stream->error;
	}
	yajl_gen_free(__stream->handle);
	SANITY_KILL_POINTER(__stream->handle);
	free(__stream);
	SANITY_KILL_POINTER(__stream);
	return buf;

stream_error:
	if (__stream->handle)
		yajl_gen_free(__stream->handle);
	free(__stream);
	return NULL;
}

static struct __JStream yajl_stream_generator =
{
	(jObjectBegin)begin_object,
	(jObjectEnd)end_object,
	(jArrayBegin)begin_array,
	(jArrayEnd)end_array,
	(jNumber)val_num,
	(jNumberI)val_int,
	(jNumberF)val_dbl,
	(jString)val_str,
	(jBoolean)val_bool,
	(jNull)val_null,
	(jFinish)finish_stream
};

JStreamRef jstreamInternal(jschema_ref schema, TopLevelType type)
{
	ActualStream* stream = (ActualStream*)calloc(1, sizeof(ActualStream));
	if (UNLIKELY(stream == NULL)) {
		return NULL;
	}
	memcpy(&stream->stream, &yajl_stream_generator, sizeof(struct __JStream));

#if 0
	// try to use custom allocators to bypass freeing the buffer & instead passing off
	// ownership to the caller.  for now, this is too difficult - we'll duplicate the string instead
	yajl_alloc_funcs allocators = {
		pjson_internal_malloc,
		pjso_internal_malloc,
		pjso_internal_free,
		NULL,
	};
	stream->handle = yajl_gen_alloc(NULL, &allocators);
#else
	stream->handle = yajl_gen_alloc(NULL, NULL);
#endif
	stream->opened = type;

	return (JStreamRef)stream;
}

JStreamRef jstream(jschema_ref schema)
{
	return jstreamInternal(schema, TOP_None);
}

JStreamRef jstreamObj(jschema_ref schema)
{
	JStreamRef opened = jstreamInternal(schema, TOP_Object);
	opened->o_begin(opened);
	return opened;
}

JStreamRef jstreamArr(jschema_ref schema)
{
	JStreamRef opened = jstreamInternal(schema, TOP_Array);
	opened->a_begin(opened);
	return opened;
}

