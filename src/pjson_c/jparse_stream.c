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

#include <jparse_stream.h>
#include <jobject.h>
#include <yajl/yajl_parse.h>
#include "liblog.h"
#include "jparse_stream_internal.h"
#include "jobject_internal.h"
#include "jschema_internal.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

typedef struct DomInfo {
	JDOMOptimization m_optInformation;
	/**
	 * This cannot be null unless we are in a top-level object or array.
	 * m_prev->m_value is the object or array that is our parent.
	 */
	struct DomInfo *m_prev;

	/**
	 * If we are setting the value for an object key, this is the key (string-type).
	 * If we are parsing the key for an object, this is NULL
	 * If we are parsing an element within an array this is NULL.
	 */
	jvalue_ref m_value;
} DomInfo;

static bool jsax_parse_internal(PJSAXCallbacks *parser, raw_buffer input, JSchemaInfoRef schemaInfo, void **ctxt, bool logError, bool comments);

static bool file_size(int fd, off_t *s)
{
	struct stat finfo;
	if (0 != fstat(fd, &finfo)) {
		return false;
	}
	*s = finfo.st_size;
	return true;
}

static inline jvalue_ref createOptimalString(JDOMOptimization opt, const char *str, size_t strLen)
{
	if (opt == DOMOPT_INPUT_OUTLIVES_WITH_NOCHANGE)
		return jstring_create_nocopy(j_str_to_buffer(str, strLen));
	return jstring_create_copy(j_str_to_buffer(str, strLen));
}

static inline jvalue_ref createOptimalNumber(JDOMOptimization opt, const char *str, size_t strLen)
{
	if (opt == DOMOPT_INPUT_OUTLIVES_WITH_NOCHANGE)
		return jnumber_create_unsafe(j_str_to_buffer(str, strLen), NULL);
	return jnumber_create(j_str_to_buffer(str, strLen));
}

static inline DomInfo* getDOMContext(JSAXContextRef ctxt)
{
	return (DomInfo*)jsax_getContext(ctxt);
}

static inline void changeDOMContext(JSAXContextRef ctxt, DomInfo *domCtxt)
{
	jsax_changeContext(ctxt, domCtxt);
}

static int dom_null(JSAXContextRef ctxt)
{
	DomInfo *data = getDOMContext(ctxt);
	// no handle to the context
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "null encountered without any context");
	// no parent node
	CHECK_CONDITION_RETURN_VALUE(data->m_prev == NULL, 0, "unexpected state - how is this possible?");

	SANITY_CHECK_POINTER(ctxt);
	SANITY_CHECK_POINTER(data->m_prev);

	if (data->m_value == NULL) {
		CHECK_CONDITION_RETURN_VALUE(!jis_array(data->m_prev->m_value), 0, "Improper place for null");
		jarray_append(data->m_prev->m_value, jnull());
	} else if (jis_string(data->m_value)) {
		CHECK_CONDITION_RETURN_VALUE(!jis_object(data->m_prev->m_value), 0, "Improper place for null");
		jobject_put(data->m_prev->m_value, data->m_value, jnull());
		data->m_value = NULL;
	} else {
		PJ_LOG_ERR("value portion of key-value pair but not a key");
		return 0;
	}

	return 1;
}

static int dom_boolean(JSAXContextRef ctxt, bool value)
{
	DomInfo *data = getDOMContext(ctxt);
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "boolean encountered without any context");
	CHECK_CONDITION_RETURN_VALUE(data->m_prev == NULL, 0, "unexpected state - how is this possible?");

	if (data->m_value == NULL) {
		CHECK_CONDITION_RETURN_VALUE(!jis_array(data->m_prev->m_value), 0, "Improper place for boolean");
		jarray_append(data->m_prev->m_value, jboolean_create(value));
	} else if (jis_string(data->m_value)) {
		CHECK_CONDITION_RETURN_VALUE(!jis_object(data->m_prev->m_value), 0, "Improper place for boolean");
		jobject_put(data->m_prev->m_value, data->m_value, jboolean_create(value));
		data->m_value = NULL;
	} else {
		PJ_LOG_ERR("value portion of key-value pair but not a key");
		return 0;
	}

	return 1;
}

static int dom_number(JSAXContextRef ctxt, const char *number, size_t numberLen)
{
	DomInfo *data = getDOMContext(ctxt);
	jvalue_ref jnum;

	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "number encountered without any context");
	CHECK_CONDITION_RETURN_VALUE(data->m_prev == NULL, 0, "unexpected state - how is this possible?");
	CHECK_POINTER_RETURN_VALUE(number, 0);
	CHECK_CONDITION_RETURN_VALUE(numberLen <= 0, 0, "unexpected - numeric string doesn't actually contain a number");

	jnum = createOptimalNumber(data->m_optInformation, number, numberLen);

	if (data->m_value == NULL) {
		if (UNLIKELY(!jis_array(data->m_prev->m_value))) {
			PJ_LOG_ERR("Improper place for number");
			j_release(&jnum);
			return 0;
		}
		jarray_append(data->m_prev->m_value, jnum);
	} else if (jis_string(data->m_value)) {
		if (UNLIKELY(!jis_object(data->m_prev->m_value))) {
			PJ_LOG_ERR("Improper place for number");
			j_release(&jnum);
			return 0;
		}
		jobject_put(data->m_prev->m_value, data->m_value, jnum);
		data->m_value = NULL;
	} else {
		PJ_LOG_ERR("value portion of key-value pair but not a key");
		return 0;
	}

	return 1;
}

static int dom_string(JSAXContextRef ctxt, const char *string, size_t stringLen)
{
	DomInfo *data = getDOMContext(ctxt);
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "string encountered without any context");
	CHECK_CONDITION_RETURN_VALUE(data->m_prev == NULL, 0, "unexpected state - how is this possible?");

	jvalue_ref jstr = createOptimalString(data->m_optInformation, string, stringLen);

	if (data->m_value == NULL) {
		if (UNLIKELY(!jis_array(data->m_prev->m_value))) {
			PJ_LOG_ERR("Improper place for string");
			j_release(&jstr);
			return 0;
		}
		jarray_append(data->m_prev->m_value, jstr);
	} else if (jis_string(data->m_value)) {
		if (UNLIKELY(!jis_object(data->m_prev->m_value))) {
			PJ_LOG_ERR("Improper place for string");
			j_release(&jstr);
			return 0;
		}
		jobject_put(data->m_prev->m_value, data->m_value, jstr);
		data->m_value = NULL;
	} else {
		PJ_LOG_ERR("value portion of key-value pair but not a key");
		return 0;
	}

	return 1;
}

static int dom_object_start(JSAXContextRef ctxt)
{
	DomInfo *data = getDOMContext(ctxt);
	jvalue_ref newParent;
	DomInfo *newChild;
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "object encountered without any context");

	newParent = jobject_create();
	newChild = calloc(1, sizeof(DomInfo));
	if (UNLIKELY(newChild == NULL || jis_null(newParent))) {
		PJ_LOG_ERR("Failed to allocate space for new object");
		j_release(&newParent);
		free(newChild);
		return 0;
	}
	newChild->m_prev = data;
	newChild->m_optInformation = data->m_optInformation;
	changeDOMContext(ctxt, newChild);

	if (data->m_prev != NULL) {
		if (jis_array(data->m_prev->m_value)) {
			assert(data->m_value == NULL);
			jarray_append(data->m_prev->m_value, newParent);
		} else {
			assert(jis_object(data->m_prev->m_value));
			CHECK_CONDITION_RETURN_VALUE(!jis_string(data->m_value), 0, "improper place for a child object");
			jobject_put(data->m_prev->m_value, data->m_value, newParent);
		}
	}

	// not using reference counting here on purpose
	data->m_value = newParent;

	return 1;
}

static int dom_object_key(JSAXContextRef ctxt, const char *key, size_t keyLen)
{
	DomInfo *data = getDOMContext(ctxt);
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "object key encountered without any context");
	CHECK_CONDITION_RETURN_VALUE(data->m_value != NULL, 0, "Improper place for an object key");
	CHECK_CONDITION_RETURN_VALUE(data->m_prev == NULL, 0, "object key encountered without any parent object");
	CHECK_CONDITION_RETURN_VALUE(!jis_object(data->m_prev->m_value), 0, "object key encountered without any parent object");

	// Need to be careful here - typically, m_value isn't reference counted
	// thus if parsing fails and m_value hasn't been inserted into a bigger object that is
	// tracked, we will leak.
	// The alternate behaviour is to insert into the parent value with a null value.
	// Then when inserting the value of the key/value pair into an object, we first remove the key & re-insert
	// a key/value pair (we don't currently have a replace mechanism).
	data->m_value = createOptimalString(data->m_optInformation, key, keyLen);

	return 1;
}

static int dom_object_end(JSAXContextRef ctxt)
{
	DomInfo *data = getDOMContext(ctxt);
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "object end encountered without any context");
	CHECK_CONDITION_RETURN_VALUE(data->m_value != NULL, 0, "mismatch between key/value count");
	CHECK_CONDITION_RETURN_VALUE(!jis_object(data->m_prev->m_value), 0, "object end encountered, but not in an object");

	assert(data->m_prev != NULL);
	changeDOMContext(ctxt, data->m_prev);
	if (data->m_prev->m_prev != NULL)
		data->m_prev->m_value = NULL;
	free(data);

	return 1;
}

static int dom_array_start(JSAXContextRef ctxt)
{
	DomInfo *data = getDOMContext(ctxt);
	jvalue_ref newParent;
	DomInfo *newChild;
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "object encountered without any context");

	newParent = jarray_create(NULL);
	newChild = calloc(1, sizeof(DomInfo));
	if (UNLIKELY(newChild == NULL || jis_null(newParent))) {
		PJ_LOG_ERR("Failed to allocate space for new array node");
		j_release(&newParent);
		free(newChild);
		return 0;
	}
	newChild->m_prev = data;
	newChild->m_optInformation = data->m_optInformation;
	changeDOMContext(ctxt, newChild);

	if (data->m_prev != NULL) {
		if (jis_array(data->m_prev->m_value)) {
			assert(data->m_value == NULL);
			jarray_append(data->m_prev->m_value, newParent);
		} else {
			assert(jis_object(data->m_prev->m_value));
			if (UNLIKELY(!jis_string(data->m_value))) {
				PJ_LOG_ERR("improper place for a child object");
				j_release(&newParent);
				return 0;
			}
			jobject_put(data->m_prev->m_value, data->m_value, newParent);
		}
	}

	// not using reference counting here on purpose
	data->m_value = newParent;

	return 1;
}

static int dom_array_end(JSAXContextRef ctxt)
{
	DomInfo *data = getDOMContext(ctxt);
	CHECK_CONDITION_RETURN_VALUE(data == NULL, 0, "array end encountered without any context");
	CHECK_CONDITION_RETURN_VALUE(data->m_value != NULL, 0, "key/value for array");
	CHECK_CONDITION_RETURN_VALUE(!jis_array(data->m_prev->m_value), 0, "array end encountered, but not in an array");

	assert(data->m_prev != NULL);
	changeDOMContext(ctxt, data->m_prev);
	if (data->m_prev->m_prev != NULL)
		data->m_prev->m_value = NULL;
	free(data);

	return 1;
}

jvalue_ref jdom_parse_ex(raw_buffer input, JDOMOptimizationFlags optimizationMode, JSchemaInfoRef schemaInfo, bool allowComments)
{
	jvalue_ref result;
	PJSAXCallbacks callbacks = {
		dom_object_start, // m_objStart
		dom_object_key, // m_objKey
		dom_object_end, // m_objEnd

		dom_array_start, // m_arrStart
		dom_array_end, // m_arrEnd

		dom_string, // m_string
		dom_number, // m_number
		dom_boolean, // m_boolean
		dom_null, // m_null
	};
	DomInfo *topLevelContext = calloc(1, sizeof(struct DomInfo));
	CHECK_POINTER_RETURN_NULL(topLevelContext);
	void *domCtxt = topLevelContext;

	bool parsedOK = jsax_parse_internal(&callbacks, input, schemaInfo, &domCtxt, false /* don't log errors*/, allowComments);

	result = topLevelContext->m_value;

	if (domCtxt != topLevelContext) {
		// unbalanced state machine (probably a result of parser failure)
		// cleanup so there's no memory leak
		PJ_LOG_ERR("state machine indicates invalid input");
		parsedOK = false;
		DomInfo *ctxt = domCtxt;
		DomInfo *parentCtxt;
		while (ctxt) {
#ifdef _DEBUG
			if (ctxt == topLevelContext) {
				assert(ctxt->m_prev == NULL);
			} else {
				assert(ctxt->m_prev != NULL);
			}
#endif

			parentCtxt = ctxt->m_prev;

			// top-level json value can only be an object or array,
			// thus we do not need to check that we aren't releasing topLevelContext->m_value.
			// the only other object type that m_value will contain is string (representing the key of an object).
			//if (ctxt->m_value && !jis_array(ctxt->m_value) && !jis_object(ctxt->m_value)) {
			if (ctxt->m_value && jis_string(ctxt->m_value)) {
				j_release(&ctxt->m_value);
			}
			free(ctxt);

			ctxt = parentCtxt;
		}
		topLevelContext = NULL;
	}

	free(topLevelContext);

	if (!parsedOK) {
		PJ_LOG_ERR("Parser failure");
		j_release(&result);
		return jnull();
	}

	if (result == NULL)
		PJ_LOG_ERR("result was NULL - unexpected. input was '%.*s'", (int)input.m_len, input.m_str);
	else if (result == jnull())
		PJ_LOG_WARN("result was NULL JSON - unexpected.  input was '%.*s'", (int)input.m_len, input.m_str);
	else {
		if ((optimizationMode & (DOMOPT_INPUT_NOCHANGE | DOMOPT_INPUT_OUTLIVES_DOM | DOMOPT_INPUT_NULL_TERMINATED)) && input.m_str[input.m_len] == '\0') {
			result->m_toString = (char *)input.m_str;
			result->m_toStringDealloc = NULL;
		}
	}

	return result;
}

jvalue_ref jdom_parse(raw_buffer input, JDOMOptimizationFlags optimizationMode, JSchemaInfoRef schemaInfo)
{
	return jdom_parse_ex(input, optimizationMode, schemaInfo, false);
}

jvalue_ref jdom_parse_file(const char *file, JSchemaInfoRef schemaInfo, JFileOptimizationFlags flags)
{
	CHECK_POINTER_RETURN_NULL(file);
	CHECK_POINTER_RETURN_NULL(schemaInfo);

	int fd;
	off_t fileSize;
	raw_buffer input = { 0 };
	jvalue_ref result;
	char *err_msg;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		goto errno_parse_failure;
	}

	if (!file_size(fd, &fileSize)) {
		goto errno_parse_failure;
	}

	input.m_len = fileSize;
	if (input.m_len != fileSize) {
		PJ_LOG_ERR("File too big - currently unsupported by this API");
		close(fd);
	}

	if (flags & JFileOptMMap) {
		input.m_str = (char *)mmap(NULL, input.m_len, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);

		if (input.m_str == NULL || input.m_str == MAP_FAILED) {
			goto errno_parse_failure;
		}
	} else {
		input.m_str = (char *)malloc(input.m_len + 1);
		if (input.m_len != read(fd, (char *)input.m_str, input.m_len)) {
			goto errno_parse_failure;
		}
		((char *)input.m_str)[input.m_len] = 0;
	}

	result = jdom_parse(input, DOMOPT_INPUT_OUTLIVES_WITH_NOCHANGE, schemaInfo);

return_result:
	close(fd);

	if (UNLIKELY(jis_null(result))) {
		if (input.m_str) {
			if (flags & JFileOptMMap) {
				munmap((void *)input.m_str, input.m_len);
			} else {
				free((void *)input.m_str);
			}
		}
	} else {
		result->m_backingBuffer = input;
		result->m_backingBufferMMap = flags & JFileOptMMap;
	}

	return result;

errno_parse_failure:
	err_msg = strdup(strerror(errno));
	PJ_LOG_WARN("Attempt to parse json document '%s' failed (%d) : %s", file, errno, err_msg);
	free(err_msg);

	result = jnull();
	goto return_result;
}

struct __JSAXContext {
	void *ctxt;
	yajl_callbacks *m_handlers;
	ValidationStateRef m_validation;
	JErrorCallbacksRef m_errors;
};

void jsax_changeContext(JSAXContextRef saxCtxt, void *userCtxt)
{
	saxCtxt->ctxt = userCtxt;
}

void* jsax_getContext(JSAXContextRef saxCtxt)
{
	return saxCtxt->ctxt;
}

typedef struct __JSAXContext PJSAXContext;

typedef int(* 	pj_yajl_null )(void *ctx);
typedef int(* 	pj_yajl_boolean )(void *ctx, int boolVal);
typedef int(* 	pj_yajl_integer )(void *ctx, long integerVal);
typedef int(* 	pj_yajl_double )(void *ctx, double doubleVal);
typedef int(* 	pj_yajl_number )(void *ctx, const char *numberVal, unsigned int numberLen);
typedef int(* 	pj_yajl_string )(void *ctx, const unsigned char *stringVal, unsigned int stringLen);
typedef int(* 	pj_yajl_start_map )(void *ctx);
typedef int(* 	pj_yajl_map_key )(void *ctx, const unsigned char *key, unsigned int stringLen);
typedef int(* 	pj_yajl_end_map )(void *ctx);
typedef int(* 	pj_yajl_start_array )(void *ctx);
typedef int(* 	pj_yajl_end_array )(void *ctx);

static PJSAXCallbacks no_callbacks = { 0 };

static void bounce_breakpoint()
{
}

#define DEREF_CALLBACK(callback, ...) \
	do { \
		if (callback != NULL) return callback(__VA_ARGS__); \
		return 1; \
	} while (0)

#define ERR_HANDLER_FAILED(err_handler, cb, ...) \
	(err_handler->cb) == NULL || !((err_handler->cb)(err_handler->m_ctxt, ##__VA_ARGS__))

#define SCHEMA_HANDLER_FAILED(ctxt) ERR_HANDLER_FAILED((ctxt)->m_errors, m_schema, (ctxt))

static int my_bounce_start_map(void *ctxt)
{
	bounce_breakpoint();
	PJ_LOG_TRACE("{");

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_obj(spring, spring->m_validation)) {
			if (ERR_HANDLER_FAILED(spring->m_errors, m_schema, spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_start_map, ctxt);
}

static int my_bounce_map_key(void *ctxt, const unsigned char *str, unsigned int strLen)
{
	bounce_breakpoint();
	PJ_LOG_TRACE("%.*s", strLen, str);

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_key(spring, spring->m_validation, j_str_to_buffer((char *)str, strLen))) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_map_key, ctxt, str, strLen);
}

static int my_bounce_end_map(void *ctxt)
{
	bounce_breakpoint();
	PJ_LOG_TRACE("}");

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_obj_end(spring, spring->m_validation)) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_end_map, ctxt);
}

static int my_bounce_start_array(void *ctxt)
{
	bounce_breakpoint();
	PJ_LOG_TRACE("[");

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_arr(spring, spring->m_validation)) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_start_array, ctxt);
}

static int my_bounce_end_array(void *ctxt)
{
	bounce_breakpoint();
	PJ_LOG_TRACE("]");

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_arr_end(spring, spring->m_validation)) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_end_array, ctxt);
}

static int my_bounce_string(void *ctxt, const unsigned char *str, unsigned int strLen)
{
	bounce_breakpoint();
	PJ_LOG_TRACE("%.*s", strLen, str);

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_str(spring, spring->m_validation, j_str_to_buffer((char *)str, strLen))) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_string, ctxt, str, strLen);
}

static int my_bounce_number(void *ctxt, const char *numberVal, unsigned int numberLen)
{
	bounce_breakpoint(numberVal);
	PJ_LOG_TRACE("%.*s", numberLen, numberVal);

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_num(spring, spring->m_validation, j_str_to_buffer((char *)numberVal, numberLen))) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_number, ctxt, numberVal, numberLen);
}

static int my_bounce_boolean(void *ctxt, int boolVal)
{
	bounce_breakpoint();
	PJ_LOG_TRACE("%s", (boolVal ? "true" : "false"));

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_bool(spring, spring->m_validation, boolVal)) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_boolean, ctxt, boolVal);
}

static int my_bounce_null(void *ctxt)
{
	bounce_breakpoint("null");
	PJ_LOG_TRACE("null");

	JSAXContextRef spring = (JSAXContextRef)ctxt;
#if !BYPASS_SCHEMA
#if SHORTCUT_SCHEMA_ALL
	if (spring->m_validation->m_state->m_schema != jschema_all())
#endif
	{
		if (!jschema_null(spring, spring->m_validation)) {
			if (SCHEMA_HANDLER_FAILED(spring))
				return 0;
		}
	}
#endif
	DEREF_CALLBACK(spring->m_handlers->yajl_null, ctxt);
}

static yajl_callbacks my_bounce = {
	my_bounce_null,
	my_bounce_boolean,
	NULL, // yajl_integer,
	NULL, // yajl_double
	my_bounce_number,
	my_bounce_string,
	my_bounce_start_map,
	my_bounce_map_key,
	my_bounce_end_map,
	my_bounce_start_array,
	my_bounce_end_array,
};

static struct JErrorCallbacks null_err_handler = { 0 };

static bool jsax_parse_internal(PJSAXCallbacks *parser, raw_buffer input, JSchemaInfoRef schemaInfo, void **ctxt, bool logError, bool comments)
{
	yajl_status parseResult;

	PJ_LOG_TRACE("Parsing '%.*s'", RB_PRINTF(input));

	if (parser == NULL)
		parser = &no_callbacks;

	if (jis_null_schema(schemaInfo->m_schema)) {
		PJ_LOG_WARN("Cannot match against schema that matches nothing: Schema pointer = %p", schemaInfo->m_schema);
		return false;
	}

	if (schemaInfo->m_schema == jschema_all()) {
		PJ_LOG_DBG("Using default empty schema for matching");
	} else {
		if (schemaInfo->m_resolver == NULL) {
			PJ_LOG_DBG("No resolver specified for the schema.  Make sure %p doesn't contain any external references", schemaInfo->m_schema);
		}
	}

	if (schemaInfo->m_errHandler == NULL)
		schemaInfo->m_errHandler = &null_err_handler;

#ifdef _DEBUG
	logError = true;
#endif

	yajl_callbacks yajl_cb = {
		(pj_yajl_null)parser->m_null, // yajl_null
		(pj_yajl_boolean)parser->m_boolean, // yajl_boolean
		NULL, // yajl_integer
		NULL, // yajl_double
		(pj_yajl_number)parser->m_number, // yajl_number
		(pj_yajl_string)parser->m_string, // yajl_stirng
		(pj_yajl_start_map)parser->m_objStart, // yajl_start_map
		(pj_yajl_map_key)parser->m_objKey, // yajl_map_key
		(pj_yajl_end_map)parser->m_objEnd, // yajl_end_map
		(pj_yajl_start_array)parser->m_arrStart, // yajl_start_array
		(pj_yajl_end_array)parser->m_arrEnd, // yajl_end_array
	};

	yajl_parser_config yajl_opts = {
		comments, // comments are not allowed
		0, // currently only UTF-8 will be supported for input.
	};

	PJSAXContext internalCtxt = {
		.ctxt = (ctxt != NULL ? *ctxt : NULL),
		.m_handlers = &yajl_cb,
		.m_errors = schemaInfo->m_errHandler,
	};

#if !BYPASS_SCHEMA
	internalCtxt.m_validation = jschema_init(schemaInfo);
	if (internalCtxt.m_validation == NULL) {
		PJ_LOG_WARN("Failed to initialize validation state machine");
		return false;
	}
#endif

	yajl_handle handle = yajl_alloc(&my_bounce, &yajl_opts, NULL, &internalCtxt);

	parseResult = yajl_parse(handle, (unsigned char *)input.m_str, input.m_len);
	if (ctxt != NULL) *ctxt = jsax_getContext(&internalCtxt);

	switch (parseResult) {
		case yajl_status_ok:
			break;
		case yajl_status_client_canceled:
			if (ERR_HANDLER_FAILED(schemaInfo->m_errHandler, m_unknown, &internalCtxt))
				goto parse_failure;
			PJ_LOG_WARN("Client claims they handled an unknown error in '%.*s'", (int)input.m_len, input.m_str);
			break;
		case yajl_status_insufficient_data:
			if (ERR_HANDLER_FAILED(schemaInfo->m_errHandler, m_parser, &internalCtxt))
				goto parse_failure;
			PJ_LOG_WARN("Client claims they handled incomplete JSON input provided '%.*s'", (int)input.m_len, input.m_str);
			break;
		case yajl_status_error:
		default:
			if (ERR_HANDLER_FAILED(schemaInfo->m_errHandler, m_unknown, &internalCtxt))
				goto parse_failure;

			PJ_LOG_WARN("Client claims they handled an unknown error in '%.*s'", (int)input.m_len, input.m_str);
			break;
	}

#if !BYPASS_SCHEMA
	jschema_state_release(&internalCtxt.m_validation);
#endif

#ifndef NDEBUG
	assert(yajl_get_error(handle, 0, NULL, 0) == NULL);
#endif

	yajl_free(handle);
	return true;

parse_failure:
	if (UNLIKELY(logError)) {
		unsigned char *errMsg = yajl_get_error_ex(handle, 1, (unsigned char *)input.m_str, input.m_len, "        ");
		PJ_LOG_WARN("Parser reason for failure:\n'%s'", errMsg);
		yajl_free_error(handle, errMsg);
	}

#if !BYPASS_SCHEMA
	jschema_state_release(&internalCtxt.m_validation);
#endif
	yajl_free(handle);
	return false;
}

bool jsax_parse_ex(PJSAXCallbacks *parser, raw_buffer input, JSchemaInfoRef schemaInfo, void **ctxt, bool logError)
{
	return jsax_parse_internal(parser, input, schemaInfo, ctxt, logError, false);
}

bool jsax_parse(PJSAXCallbacks *parser, raw_buffer input, JSchemaInfoRef schema)
{
	assert(schema != NULL);
	return jsax_parse_ex(parser, input, schema, NULL, false);
}

static inline bool jsax_parse_inject_internal(JSAXContextRef ctxt, jvalue_ref key, jvalue_ref value)
{
	assert (ctxt != NULL);
	assert (value != NULL);

	yajl_callbacks *cbs = ctxt->m_handlers;
	raw_buffer str;

	if (key) {
		str = jstring_get_fast(key);
		if (UNLIKELY(!cbs->yajl_map_key(ctxt, (const unsigned char *)str.m_str, str.m_len)))
			return false;
	}

	switch (value->m_type) {
		case JV_OBJECT:
		{
			jobject_key_value keyval;
			if (UNLIKELY(!cbs->yajl_start_map(ctxt)))
				return false;

			for (jobject_iter i = jobj_iter_init(value); jobj_iter_is_valid(i); i = jobj_iter_next(i)) {
				jobj_iter_deref(i, &keyval);
				if (UNLIKELY(!jsax_parse_inject(ctxt, keyval.key, keyval.value)))
					return false;
			}

			if (UNLIKELY(!cbs->yajl_end_map(ctxt)))
				return false;
			break;
		}
		case JV_ARRAY:
		{
			jvalue_ref item;

			if (UNLIKELY(!cbs->yajl_start_array(ctxt)))
				return false;

			for (ssize_t i = jarray_size(value) - 1; i >= 0; i--) {
				item = jarray_get(value, i);
				if (UNLIKELY(!jsax_parse_inject(ctxt, NULL, item)))
					return false;
			}

			if (UNLIKELY(!cbs->yajl_end_array(ctxt)))
				return false;
			break;
		}
		case JV_STR:
		{
			str = jstring_get_fast(value);
			if (UNLIKELY(!cbs->yajl_string(ctxt, (const unsigned char *)str.m_str, str.m_len)))
				return false;

			break;
		}
		case JV_NUM:
		{
			assert (value->value.val_num.m_type == NUM_RAW);
			// this numeric string should have come directly from a schema - we don't do any conversion internally.
			// how did this state occur?
			CHECK_CONDITION_RETURN_VALUE(value->value.val_num.m_type != NUM_RAW, false, "Some internal problem parsing schema");

			str = jnumber_deref_raw(value);
			if (UNLIKELY(!cbs->yajl_number(ctxt, str.m_str, str.m_len)))
				return false;

			break;
		}
		case JV_BOOL:
		{
			if (UNLIKELY(!cbs->yajl_boolean(ctxt, jboolean_deref(value))))
				return false;
			break;
		}
		case JV_NULL:
		{
			if (UNLIKELY(!cbs->yajl_null(ctxt)))
				return false;
			break;
		}
		default:
		{
			// how can this occur? - memory corruption?
			PJ_SCHEMA_ERR("Internal error - schema is corrupt");
			return false;
		}
	}
	return true;
}

bool jsax_parse_inject(JSAXContextRef ctxt, jvalue_ref key, jvalue_ref value)
{
	assert (jis_string(key));
	assert (jis_object(value));

	return jsax_parse_inject_internal(ctxt, key, value);
}
