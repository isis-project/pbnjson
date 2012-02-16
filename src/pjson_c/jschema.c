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

/***
 * Simplifying restrictions:
 *
 * 1. A schema must be an object.
 *    Reasoning: The array notation appears to simply be a convenience and makes the validator overly complicated.
 *               End users already have a notation to express tuple-typed schemas through the items property in a schema.
 * 2. Union types are 1 or more simple types
 *    Reasoning: The "array of this schema" notation makes the validator overly complicated.
 *               End users already have a notation to express tuple-types schemas through the items property in a schema.
 * 3. Simple types cannot currently be schemas
 *    Reasoning: This makes parsing extremely complicated because you have to validate against multiple schemas simultaneously.
 *               This is also potentially a security hole as a specially crafted schema can easily result in exponential/infinite validation time.
 *               Simplifying assumptions need to be made about the format of union schemas to get around this if this mode ever needs to
 *               be supported.
 * 4. Specifying null for a property is equivalent to not specifying the property
 *    Reasoning: Google does this and the spec provides no information on how to treat
 *               NULL.  This is the most logical.
 * 5. Specifying integer as the disallowed type is equivalent to specifying number
 *    Reasoning: Disallowing integer means essentially that number would have to contain
 *               a floating point portion.  This is almost certainly the wrong thing since
 *               someone might omit the floating point portion if it's 0 (e.g 6.0).
 *               It's also highly unlikely that there is a use case that makes use of this.
 * 6. maxDecimal is not checked.
 *    Reasoning: It is unclear what the behaviour should be.  What happens if you have
 *               a slightly more complex number: e.g. 5.03e10.  Even worse,
 *               5.03e-10.  What does maxDecimal represent?
 * 7. tuple typing on arrays is lenient - unless maxItems/minItems is specified, no check will be made to ensure
 *    which subset or superset of the array was matched (additionalProperties is where the superset capability
 *    can come in).
 */
#include <jschema.h>
#include <jobject.h>
#include <jobject_internal.h>
#include <jparse_stream_internal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <inttypes.h>

#include "schema_keys.h"
#include "liblog.h"
#include "jschema_internal.h"
#include "jvalue/num_conversion.h"
#include "jparse_stream_internal.h"

#define TRACE_SCHEMA_REF(format, pointer, ...) PJ_SCHEMA_TRACE("TRACE jschema_ref: %p " format, pointer, ##__VA_ARGS__)
#define TRACE_SCHEMA_STATE(format, pointer, ...) PJ_SCHEMA_TRACE("TRACE SchemaStateRef: %p " format, pointer, ##__VA_ARGS__)
#define TRACE_VALIDATION_STATE(format, pointer, ...) PJ_SCHEMA_TRACE("TRACE ValidationStateRef: %p " format, pointer, ##__VA_ARGS__)
#define TRACE_STATE_ARRAY(format, pointer, ...) PJ_SCHEMA_TRACE("TRACE m_validation: %p " format, pointer, ##__VA_ARGS__)
#define TRACE_SCHEMA_RESOLUTION(format, pointer, ...) PJ_SCHEMA_TRACE("TRACE schema $ref %p: " format, pointer, ##__VA_ARGS__)

#if !BYPASS_SCHEMA
static const bool DEFAULT_ADDITIONAL_PROPERTIES_VALUE = true;

static bool jschema_release_internal2(SchemaWrapperRef schema) NON_NULL(1);
static void jschema_release_internal(SchemaWrapperRef *schema) NON_NULL(1);
static void j_release_wrapper(jvalue_ref *value) NON_NULL(1);

static JSchemaResolutionResult noop_bad_resolver(JSchemaResolverRef resolver,
		jschema_ref *resolvedSchema);

static struct JSchemaResolver NOOP_BAD_RESOLVER = {
	.m_resolve = noop_bad_resolver,
	.m_ctxt = NULL,
};

static bool jis_all_schema_hierarchy(jvalue_ref value) NON_NULL(1) UNUSED_FUNC;
static bool jis_empty_schema_dom(jvalue_ref value) NON_NULL(1) UNUSED_FUNC;

JSchemaResolverRef jget_garbage_resolver()
{
	return &NOOP_BAD_RESOLVER;
}

static void ensureArrayInitialized(jvalue_ref *arrRef) NON_NULL(1) UNUSED_FUNC;
static void ensureArrayInitialized(jvalue_ref *arrRef)
{
	if (*arrRef == NULL)
		*arrRef = jarray_create(NULL);
	else
		assert(jis_array(*arrRef));
}
#endif

#if ALLOW_LOCAL_REFS
static void nextToken(raw_buffer *token, const char *lastPosition) NON_NULL(1, 2);
static void nextToken(raw_buffer *token, const char *lastPosition)
{
	assert(token != NULL);
	assert(lastPosition != NULL);
	assert(token->m_str + token->m_len < lastPosition);

	for (; token->m_str + token->m_len < lastPosition; token->m_len ++) {
		switch (token->m_str[token->m_len]) {
		case '[':
		case '.':
		case ']':
			return;
		}
	}
}

static bool resolveLocalSchema(SchemaWrapperRef schema, raw_buffer path) NON_NULL(1);
static bool resolveLocalSchema(SchemaWrapperRef schema, raw_buffer path)
{
	jvalue_ref toLookup = schema->m_top;
	raw_buffer nextObject = {
			.m_len = 0,
			.m_str = path.m_str + 1,
	};
	const char *lastPosition = path.m_str + path.m_len;

	assert(toLookup != NULL);
	if (UNLIKELY(toLookup == NULL))
		return false;

	assert(path.m_len >= 1);
	assert(path.m_str[0] == '$');
	assert(nextObject.m_str[0] == '.');

	while (nextObject.m_str + nextObject.m_len < lastPosition) {
		switch (nextObject.m_str[0]) {
			case '[':
			{
				int32_t index;
				jvalue_ref indexStr;

				CHECK_CONDITION_RETURN_VALUE(!jis_array(toLookup), false, "Attempt to access indexed element of instance that isn't an array");

				nextObject.m_str++;
				nextObject.m_len = 0;
				nextToken(&nextObject, lastPosition);

				CHECK_CONDITION_RETURN_VALUE(nextObject.m_len == 0, false,
						"Invalid array index provided in reference: '%.*s'", RB_PRINTF(path));
				CHECK_CONDITION_RETURN_VALUE(nextObject.m_str[nextObject.m_len] != ']', false, "Invalid array index provided in reference");

				indexStr = jnumber_create_unsafe(nextObject, NULL);
				CHECK_CONDITION_RETURN_VALUE(CONV_OK != jnumber_get_i32(indexStr, &index), false,
						"Invalid array index '%.*s' in reference: '%.*s'", RB_PRINTF(nextObject), RB_PRINTF(path));

				nextObject.m_str++;	// skip ']'
				break;
			}
			case '.':
				CHECK_CONDITION_RETURN_VALUE(!jis_object(toLookup), false, "Attempt to access key of instance that isn't an object");

				nextObject.m_str++;
				nextObject.m_len = 0;
				nextToken(&nextObject, lastPosition);

				CHECK_CONDITION_RETURN_VALUE(!jobject_get_exists(toLookup, nextObject, &toLookup), false, "Key '%.*s' in reference: '%.*s' is invalid", RB_PRINTF(nextObject), RB_PRINTF(path));
				assert(!jis_null(toLookup));
				break;
			case ']':
				PJ_SCHEMA_ERR("Unexpected ']' character in reference: '%.*s'", RB_PRINTF(path));
				return false;
		}
	}
	assert(jis_object(toLookup));
	TRACE_STATE_ARRAY("adding %p", schema->m_validation, toLookup);
	jarray_put(schema->m_validation, 0, toLookup);
	return true;
}
#endif

#if ALLOW_LOCAL_REFS
#define STACK_SCHEMA_WRAPPER(self, top) \
	{\
		.m_refCnt = 1,\
		.m_validation = jarray_create_var(NULL, self, NULL),\
		.m_top = top,\
	}
#else
#define STACK_SCHEMA_WRAPPER(self, top) \
	{\
		.m_refCnt = 1,\
		.m_validation = jarray_create_var(NULL, jvalue_copy(self), NULL)\
	}
#endif

#if 0
static jvalue_ref top_schema(SchemaWrapperRef schema)
{
	SANITY_CHECK_POINTER(schema);
	assert(schema != NULL);
	assert(jis_array(schema->m_validation));
	assert(jarray_size(schema->m_validation) >= 1);
	assert(jis_object(jarray_get(schema->m_validation, 0)));
	return jarray_get(schema->m_validation, 0);
}
#endif

#if !BYPASS_SCHEMA
static jvalue_ref last_schema(SchemaWrapperRef schema)
{
	SANITY_CHECK_POINTER(schema);
	assert(schema != NULL);
	assert(jis_array(schema->m_validation));
	assert(jarray_size(schema->m_validation) >= 1);
	assert(jis_object(jarray_get(schema->m_validation, jarray_size(schema->m_validation) - 1)));
	return jarray_get(schema->m_validation, jarray_size(schema->m_validation) - 1);
}

static jvalue_ref top_schema(SchemaWrapperRef schema)
{
	SANITY_CHECK_POINTER(schema);
	assert(schema != NULL);
	assert(jis_array(schema->m_validation));
	assert(jarray_size(schema->m_validation) >= 1);
	assert(jis_object(jarray_get(schema->m_validation, 0)));
	return jarray_get(schema->m_validation, 0);
}

#if TRACK_SCHEMA_PARSING
#define RESOLVE_SCHEMA(parseState, schema, resolver) resolveSchema(parseState, schema, resolver)
#else
#define RESOLVE_SCHEMA(parseState, schema, resolver) resolveSchema(schema, resolver)
#endif

#if TRACK_SCHEMA_PARSING
static bool resolveSchema(ValidationStateRef parseState, SchemaWrapperRef schema, SchemaResolutionRef resolver) NON_NULL(1, 2);
static bool resolveSchema(ValidationStateRef parseState, SchemaWrapperRef schema, SchemaResolutionRef resolver)
#else
static bool resolveSchema(SchemaWrapperRef schema, SchemaResolutionRef resolver) NON_NULL(1, 2);
static bool resolveSchema(SchemaWrapperRef schema, SchemaResolutionRef resolver)
#endif
{
	jvalue_ref ref;
	assert(schema != NULL);
	SANITY_CHECK_POINTER(schema);
	SANITY_CHECK_POINTER(schema->m_validation);
	assert(jis_array(schema->m_validation));

	jvalue_ref toResolve = last_schema(schema);
	assert(!jis_null(toResolve));

	ref = jobject_get(toResolve, J_CSTR_TO_BUF(SK_REF));
	if (!jis_null(ref)) {
		TRACE_SCHEMA_RESOLUTION("resolving w/ refcnt %d", toResolve, toResolve->m_refCnt);

		raw_buffer refStr;
		SchemaWrapperRef resolvedRemote;

		// need to resolve a reference
		if (UNLIKELY(jobject_size(toResolve) != 1)) {
			CHECK_CONDITION_RETURN_VALUE(
					jobject_size(toResolve) != 2 || !jobject_containskey(toResolve, J_CSTR_TO_BUF(SK_DESCRIPTION)),
					false,
					"Schema is invalid - contains an external reference in addition to other key/values that aren't a description field");
		}
		CHECK_CONDITION_RETURN_VALUE(!jis_string(ref), false, "External reference is invalid - must be a string");

		refStr = jstring_get_fast(ref);
		assert(refStr.m_str != NULL);
		if (UNLIKELY(refStr.m_len <= 0)) {
			const char * invalidSchemaAsStr = jvalue_tostring(toResolve, jschema_all());
			PJ_SCHEMA_ERR("No valid schema reference: %s", invalidSchemaAsStr);
			return false;
		}

		if (UNLIKELY(refStr.m_str[0] == '$')) {
#if ALLOW_LOCAL_REFS
			return resolveLocalSchema(schema, refStr);
#else
			PJ_SCHEMA_WARN("$ is reserved for referencing within the schema.  This isn't currently supported");
#endif
		}

		resolver->m_resolver->m_ctxt = schema;
		resolver->m_resolver->m_resourceToResolve = refStr;
		START_TRACKING_SCHEMA(parseState);
		if (SCHEMA_RESOLVED != resolver->m_resolver->m_resolve(resolver->m_resolver, &resolvedRemote)) {
			END_TRACKING_SCHEMA(parseState);
			PJ_SCHEMA_ERR("Resolver failed to resolve %.*s", RB_PRINTF(refStr));
			return false;
		}
		END_TRACKING_SCHEMA(parseState);
		PJ_SCHEMA_DBG("Resolved remote reference for %.*s", RB_PRINTF(refStr));

		assert(resolvedRemote != NULL);
		assert(jis_array(resolvedRemote->m_validation));

		int lastElement = jarray_size(schema->m_validation) - 1;
		TRACE_STATE_ARRAY("removing %p w/ refCnt %d", schema->m_validation, jarray_get(schema->m_validation, lastElement), jarray_get(schema->m_validation, lastElement)->m_refCnt);
		jarray_remove(schema->m_validation, lastElement);
		if (!jarray_splice_append(schema->m_validation, resolvedRemote->m_validation, SPLICE_TRANSFER)) {
			PJ_SCHEMA_ERR("Failed to append resolved schema for %.*s", RB_PRINTF(refStr));
			jschema_release_internal(&resolvedRemote);
			return false;
		}
//		schema->m_validation = resolvedRemote->m_validation;
//		schema->m_top = resolvedRemote->m_top;

		jschema_release_internal(&resolvedRemote);

		assert(jis_array(schema->m_validation));

		return true;
	} else if (!jis_null(ref = jobject_get(top_schema(schema), J_CSTR_TO_BUF(SK_EXTENDS)))) {
		SchemaWrapper extensionWrapper = STACK_SCHEMA_WRAPPER(ref, schema->m_top);
		CHECK_CONDITION_RETURN_VALUE(!RESOLVE_SCHEMA(parseState, &extensionWrapper, resolver), false, "Failed to resolve inheritance");
#ifndef NDEBUG
		for (ssize_t i = jarray_size(extensionWrapper.m_validation) - 1; i >= 0; i--)
			assert(jis_object(jarray_get(extensionWrapper.m_validation, i)));
#endif

		TRACE_STATE_ARRAY("splicing in %p", schema->m_validation, extensionWrapper.m_validation);
		CHECK_CONDITION_RETURN_VALUE(!jarray_splice_append(schema->m_validation, extensionWrapper.m_validation, SPLICE_TRANSFER), false, "Failed to splice extension");
		assert(jis_array(extensionWrapper.m_validation));

		TRACE_STATE_ARRAY("intermediary destroyed", extensionWrapper.m_validation);
		j_release_wrapper(&extensionWrapper.m_validation);
	}
	// else nothing to do - no reference to resolve

	return true;
}
#endif

jschema_ref jschema_copy(jschema_ref schema)
{
#if !BYPASS_SCHEMA
	SchemaWrapperRef schemaImpl = (SchemaWrapperRef)schema;
	assert(schemaImpl != jschema_all());
	assert(schemaImpl->m_refCnt > 0);
	schemaImpl->m_refCnt++;

	TRACE_SCHEMA_REF("inc refcnt to %d", schemaImpl, schemaImpl->m_refCnt);
//	PJ_SCHEMA_DBG("Referencing schema %p: %d", schema, schemaImpl->m_refCnt);
#endif
	return schema;
}

#if !BYPASS_SCHEMA
static bool jschema_release_internal2(SchemaWrapperRef schema)
{
	assert (!jis_null_schema(schema));
	assert (schema->m_refCnt > 0);

	if (schema == jschema_all())
		return false;

//	PJ_SCHEMA_DBG("Unreferencing schema %p: %d", schema, schema->m_refCnt - 1);
	if (--schema->m_refCnt == 0) {
		TRACE_SCHEMA_REF("releasing validation array %p", schema, schema->m_validation);
//		PJ_SCHEMA_DBG("Releasing schema %p validation array", schema);
		j_release_wrapper(&schema->m_validation);
#if ALLOW_LOCAL_REFS
		j_release_wrapper(&schema->m_top);
#endif

		if (schema->m_backingMMap) {
			munmap((char *)schema->m_backingMMap, schema->m_backingMMapSize);
			SANITY_CLEAR_VAR(schema->m_backingMMap, MAP_FAILED);
			SANITY_CLEAR_VAR(schema->m_backingMMapSize, -1);
		}
		return true;
	} else if (UNLIKELY(schema->m_refCnt < 0)) {
		PJ_LOG_ERR("reference counter messed up - memory corruption and/or random crashes are possible");
		assert(false);
	} else {
		TRACE_SCHEMA_REF("dec refcnt to %d", schema, schema->m_refCnt);
	}

	return false;
}

static void jschema_release_internal(SchemaWrapperRef *schemaImpl)
{
	assert(schemaImpl != NULL);
	SANITY_CHECK_POINTER(*schemaImpl);

	if (jis_null_schema(*schemaImpl))
		goto released_schema;

	if (jschema_release_internal2(*schemaImpl)) {
//		PJ_SCHEMA_DBG("Destroying schema %p", *schemaImpl);
		TRACE_SCHEMA_REF("freed", *schemaImpl);

		assert((*schemaImpl)->m_refCnt == 0);
		free(*schemaImpl);
	}

released_schema:
	SANITY_KILL_POINTER(*schemaImpl);
}
#endif

void jschema_release(jschema_ref *schema)
{
#if !BYPASS_SCHEMA
	return jschema_release_internal((SchemaWrapperRef*)schema);
#endif
}

#if !BYPASS_SCHEMA
static SchemaWrapperRef jschema_wrap_prepare(jvalue_ref parent)
{
	SchemaWrapperRef schema = (SchemaWrapperRef)malloc(sizeof(SchemaWrapper));
	schema->m_refCnt = 1;
#if ALLOW_LOCAL_REFS
	schema->m_top = jvalue_copy(parent);
#endif
	schema->m_validation = jarray_create(NULL);

	schema->m_backingMMap = NULL;
	schema->m_backingMMapSize = 0;

	TRACE_SCHEMA_REF("created w/ refcnt %d", schema, schema->m_refCnt);

//	PJ_SCHEMA_DBG("Allocating schema %p with parent %p", schema, parent);

	return schema;
}

static SchemaWrapperRef jschema_wrap(jvalue_ref toWrap, jvalue_ref parent)
{
	SchemaWrapperRef wrapped = jschema_wrap_prepare(parent);

	assert(!jis_all_schema_hierarchy(toWrap));
	assert(!jis_empty_schema_dom(toWrap));
	TRACE_STATE_ARRAY("adding %p", wrapped->m_validation, toWrap);
	jarray_append(wrapped->m_validation, toWrap);
	return wrapped;
}
#endif

#if !BYPASS_SCHEMA
#if TRACK_SCHEMA_PARSING
static bool parsing_schema_internal = false;
#define START_TRACKING_SCHEMA_INTERNAL parsing_schema_internal = true
#define END_TRACKING_SCHEMA_INTERNAL parsing_schema_internal = false
#else
#define START_TRACKING_SCHEMA_INTERNAL do {} while (0)
#define END_TRACKING_SCHEMA_INTERNAL do {} while (0)
#endif

static jvalue_ref jschema_parse_internal(raw_buffer input, JSchemaOptimizationFlags inputOpt, JSchemaInfoRef validationInfo)
{
	START_TRACKING_SCHEMA_INTERNAL;
	jvalue_ref rawSchema = jdom_parse_ex(input, inputOpt, validationInfo, true);
	END_TRACKING_SCHEMA_INTERNAL;
	if (jis_null(rawSchema) || !jis_object(rawSchema)) {
		PJ_SCHEMA_ERR("Not a valid schema - accepting no inputs: %.*s", (int)input.m_len, input.m_str);
		j_release_wrapper(&rawSchema);
		rawSchema = NULL;
	}

	return rawSchema;
}
#endif

void jschema_info_init(JSchemaInfoRef schemaInfo, jschema_ref schema, JSchemaResolverRef resolver, JErrorCallbacksRef errHandler)
{
	// if the structure ever changes, fill the remaining with 0
	schemaInfo->m_schema = schema;
	schemaInfo->m_errHandler = errHandler;
	schemaInfo->m_resolver = resolver;
#ifndef NDEBUG
	for (int i = sizeof(schemaInfo->m_padding) / sizeof(schemaInfo->m_padding[0]) - 1; i >= 0; i--)
		SANITY_KILL_POINTER(schemaInfo->m_padding[i]);
#endif
}

jschema_ref jschema_parse(raw_buffer input, JSchemaOptimizationFlags inputOpt, JErrorCallbacksRef errorHandler)
{
#if !BYPASS_SCHEMA
	struct JSchemaInfo info = {
		.m_schema = jschema_all(),
		.m_resolver = jget_garbage_resolver(),
		.m_errHandler = errorHandler
	};
	return jschema_parse_ex(input, inputOpt, &info);
#else
	return NULL;
#endif
}

jschema_ref jschema_parse_file(const char *file, JErrorCallbacksRef errorHandler)
{
#if !BYPASS_SCHEMA
	jschema_ref parsedSchema;

	// mmap the file
	const char *mapContents = NULL;
	size_t mapSize = 0;
	int fd = -1;
	struct stat fileInfo;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		PJ_LOG_WARN("Unable to open schema file %s", file);
		return NULL;
	}

	if (-1 == fstat(fd, &fileInfo)) {
		PJ_LOG_WARN("Unable to get information for schema file %s", file);
		goto map_failure;
	}
	mapSize = fileInfo.st_size;

	mapContents = mmap(NULL, mapSize, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
	if (mapContents == MAP_FAILED || mapContents == NULL) {
		PJ_LOG_WARN("Failed to create memory map for schema file %s", file);
		mapContents = NULL;
		goto map_failure;
	}

	close(fd);
	fd = -1;

	parsedSchema = jschema_parse(j_str_to_buffer(mapContents, mapSize),
		DOMOPT_INPUT_OUTLIVES_WITH_NOCHANGE, errorHandler);
	if (parsedSchema == NULL) {
		PJ_LOG_WARN("Failed to parse schema file %s", file);
		goto map_failure;
	}

	parsedSchema->m_backingMMap = mapContents;
	parsedSchema->m_backingMMapSize = mapSize;

	return parsedSchema;

map_failure:
	if (mapContents)
		munmap((char *)mapContents, mapSize);

	if (fd != -1)
		close(fd);

	return NULL;
#else
	return NULL;
#endif
}

jschema_ref jschema_parse_ex(raw_buffer input, JSchemaOptimizationFlags inputOpt, JSchemaInfoRef validationInfo)
{
#if !BYPASS_SCHEMA
	jvalue_ref parsed = jschema_parse_internal(input, inputOpt, validationInfo);
	if (parsed == NULL)
		return NULL;

	return jschema_wrap(parsed, parsed);
#else
	return NULL;
#endif
}

#ifndef OPT_TYPE_PARSING
// in the absence of how much faster this makes this, we do things correctly
#define OPT_TYPE_PARSING 0
#endif /* OPT_TYPE_PARSING */

#if OPTIMIZE_TYPE_PARSING
#define PICK_TYPE(buffer, strliteral, type) \
	do { \
		assert(buffer.m_len == sizeof(strliteral) - 1); \
		if (buffer.m_str[0] == strliteral[0]) { \
			return type; \
		} \
	} while (0)
#else
#define PICK_TYPE(buffer, strliteral, type) \
	do {\
		assert(buffer.m_len == sizeof(strliteral) - 1); \
		if (memcmp(buffer.m_str, strliteral, sizeof(strliteral) - 1) == 0) {\
			return type; \
		}\
	} while(0)
#endif /* OPTIMIZE_TYPE_PARSING */

#if !BYPASS_SCHEMA
static SchemaType parseType(raw_buffer input)
{
	// TODO: STATIC VALIDATION POSSIBILITY OF SCHEMA
	if (input.m_len == 0)
		return ST_ERR;

	switch (input.m_len) {
	case 3:
		PICK_TYPE(input, "any", ST_ANY);
		break;
	case 4:
		PICK_TYPE(input, "null", ST_NULL);
		break;
	case 5:
		PICK_TYPE(input, "array", ST_ARR);
		break;
	case 6:
		switch (input.m_str[0]) {
		case 'o':
			PICK_TYPE(input, "object", ST_OBJ);
			break;
		case 's':
			PICK_TYPE(input, "string", ST_STR);
			break;
		case 'n':
			PICK_TYPE(input, "number", ST_NUM);
			break;
		}
		break;
	case 7:
		switch (input.m_str[0]) {
		case 'i':
			PICK_TYPE(input, "integer", ST_INT);
			break;
		case 'b':
			PICK_TYPE(input, "boolean", ST_BOOL);
			break;
		}
		break;
	}
	PJ_LOG_WARN("Ignoring unsupported type %.*s", (int)input.m_len, input.m_str);
	return ST_ANY;
}

static SchemaTypeBitField determinePossibilities_internal(SchemaTypeBitField field, jvalue_ref schemaType)
{
	if ((field & ST_ANY) == ST_ANY)
		return field;

	if (jis_null(schemaType)) {
		return ST_ANY;
	}
	if (jis_string(schemaType)) {
		return field | parseType(jstring_get_fast(schemaType));
	}
	if (jis_array(schemaType)) {
		for (ssize_t i = jarray_size(schemaType) - 1; i >= 0 && (field & ST_ANY) != ST_ANY; i--)
			field |= determinePossibilities_internal(field, jarray_get(schemaType, i));
		return field;
	}
	PJ_LOG_WARN("Schema type/disallowed field contains an unsupported JSON type: %u", schemaType->m_type);
	return ST_ANY;
}

static void determinePossibilities(SchemaStateRef state)
{
	SchemaTypeBitField parsed = 0;
	jvalue_ref typeParameter;

	// TODO: STATIC VALIDATION POSSIBILITY OF SCHEMA
	// here we have to be careful - m_validation is the inheritance tree.  So the types are actually restricted
	// as we move further down the inheritance tree.  if the user breaks the tree, we catch this
	// by disallowing all schemas
	ssize_t numInheritanceSchemas = jarray_size(state->m_schema->m_validation);
	jvalue_ref parent;

	// initialize the types with the most lax types possible
	state->m_allowedTypes = ST_ANY;
	state->m_disallowedTypes = 0;

	// keep restricting moving up the inheritance tree
	for (ssize_t i = 0; i < numInheritanceSchemas; i++) {
		parent = jarray_get(state->m_schema->m_validation, i);
		// we are restricting further - must have at most the types that were allowed previously
		// otherwise, we assume the parent provides acceptable parameters
		if (!jis_null(typeParameter = jobject_get(parent, J_CSTR_TO_BUF(SK_TYPE)))) {
			parsed = determinePossibilities_internal(0, typeParameter);
			if ((parsed & state->m_allowedTypes) != parsed) {
				PJ_LOG_WARN("Schema type specified in %zd from top conflicts with one of its parent types - violates more restricting schema rule for inheritance", i);
				assert(false);	// this is a problem with the schema - force handling of this in debug mode

				// this is an error condition - this schema will fail any input
				state->m_allowedTypes = 0;
				state->m_disallowedTypes = ST_ANY;
				return;
			}
			// update with the current value (which may have restricted the types allowed further)
			state->m_allowedTypes = parsed;
		}

		if (!jis_null(typeParameter = jobject_get(parent, J_CSTR_TO_BUF(SK_DISALLLOWED)))) {
			if ((parsed & state->m_disallowedTypes) != state->m_disallowedTypes) {
				// if the child doesn't specify at least all of its parent disallowed types, then it can allow more
				PJ_LOG_WARN("Schema type specified in %zd from top conflicts with one of its parent disallowed types - violates more restricting schema rule for inheritance", i);
				assert(false);	// this is a problem with the schema - force handling of this in debug mode

				// this is an error condition - this schema will fail any input
				state->m_allowedTypes = 0;
				state->m_disallowedTypes = ST_ANY;
				return;
			}
			state->m_disallowedTypes = parsed;
		}
	}
}

static void validation_destroy(ValidationStateRef *statePtr)
{
	ValidationStateRef state;

	assert(statePtr != NULL);
	state = *statePtr;

	SANITY_CHECK_POINTER(state);
	SANITY_CHECK_POINTER(state->m_state);

	if (state) {
		TRACE_VALIDATION_STATE("destroyed", state);
		TRACE_SCHEMA_STATE("destroyed", state->m_state);

		free(state->m_state);
		SANITY_KILL_POINTER(state->m_state);
		free(state);
	}

	SANITY_KILL_POINTER(*statePtr);
}

static JSchemaResolutionResult noop_bad_resolver(JSchemaResolverRef resolver,
		jschema_ref *resolvedSchema)
{
	PJ_LOG_ERR("Attempted to use external ref in schema without providing a resolver");
	return SCHEMA_GENERIC_ERROR;
}

static void assert_valid_schema(SchemaWrapperRef schema)
{
	assert(schema->m_refCnt > 0);
	assert(!jis_null(schema->m_validation));
	assert(schema->m_validation->m_refCnt > 0);
#if ALLOW_LOCAL_REFS
	assert(!jis_null(schema->m_top));
	assert(schema->m_top > 0);
#endif
}
#endif

ValidationStateRef jschema_init(JSchemaInfoRef schemaInfo)
{
#if !BYPASS_SCHEMA
	ValidationStateRef validation = (ValidationStateRef) malloc(sizeof(struct ValidationState));
	CHECK_POINTER_RETURN_NULL(validation);

	validation->m_state = calloc(1, sizeof(struct SchemaState));
	if (validation->m_state == NULL) {
		validation_destroy(&validation);
		return NULL;
	}

	TRACE_VALIDATION_STATE("created", validation);

	assert(schemaInfo->m_schema != NULL);
	assert_valid_schema(schemaInfo->m_schema);

	if (schemaInfo->m_resolver == NULL)
		schemaInfo->m_resolver = jget_garbage_resolver();
	else if (schemaInfo->m_resolver->m_resolve == NULL) {
		schemaInfo->m_resolver->m_resolve = noop_bad_resolver;
	}
	validation->m_resolutionHandlers.m_resolver = schemaInfo->m_resolver;
	validation->m_resolutionHandlers.m_errorHandler = schemaInfo->m_errHandler;

	// only one top-level state possible - when we allow more complex unions, this will have to change
	if (!RESOLVE_SCHEMA(validation, schemaInfo->m_schema, &validation->m_resolutionHandlers)) {
		PJ_LOG_ERR("Failed to initialize validation state because requested schema failed to resolve");
		validation_destroy(&validation);
		return NULL;
	}

	validation->m_state->m_schema =
			((schemaInfo->m_schema != jschema_all()) ?
					jschema_copy(schemaInfo->m_schema) :
					jschema_all());
	determinePossibilities(validation->m_state);

	return validation;
#else
	return NULL;
#endif
}

#if !BYPASS_SCHEMA
/**
 * @return The parent of state
 */
static SchemaStateRef destroy_state(SchemaStateRef *state)
{
	SchemaStateRef parent;

	assert(state != NULL);
	SANITY_CHECK_POINTER(*state);

	parent = (*state)->m_parent;
	TRACE_SCHEMA_STATE("destroyed - parent is %p", *state, parent);

	if ((*state)->m_seenKeys != NULL)
		j_release_wrapper(&((*state)->m_seenKeys));
	jschema_release_internal(&((*state)->m_schema));
	free(*state);

	SANITY_KILL_POINTER(*state);

	return parent;
}

/**
 * Destroy branch between state & all parents up to terminating (non-inclusive).
 *
 * @param state Pointer to the SchemaState reference to destroy
 * @param terminating The SchemaState reference to stop termination at (doesn't get destroyed itself)
 *        NULL implicitly means to top-level.
 */
static SchemaStateRef destroy_branch(SchemaStateRef *state, SchemaStateRef terminating) NON_NULL(1);
static SchemaStateRef destroy_branch(SchemaStateRef *state, SchemaStateRef terminating)
{
	TRACE_SCHEMA_STATE("destroying between %p & %p", NULL, *state, terminating);
	SchemaStateRef parent = NULL;

	assert(state != NULL);
	SANITY_CHECK_POINTER(*state);
	SANITY_CHECK_POINTER(terminating);

	while (*state != terminating) {
		SANITY_CHECK_POINTER(*state);
		assert(*state != NULL);

		parent = destroy_state(state);
		state = &parent;

		assert(parent != NULL || terminating == NULL);
	}

	SANITY_KILL_POINTER(*state);

	return parent;
}
#endif

void jschema_state_release(ValidationStateRef *refPtr)
{
#if !BYPASS_SCHEMA
	assert (refPtr != NULL);

	ValidationStateRef state = *refPtr;
	SANITY_CHECK_POINTER(state);

	TRACE_VALIDATION_STATE("destroyed", state);

	// if we get a parser failure at certain stages, this can
	// cause the state machine to be left in an indeterminate state.
	// make sure to clean up after ourselves to prevent memory leaks
	SchemaStateRef toCheck = state->m_state;
	while (toCheck != NULL) {
		toCheck = destroy_state(&toCheck);
	}
	free(state);

	SANITY_KILL_POINTER(*refPtr);
#endif
}

#if !BYPASS_SCHEMA
static bool jschema_isvalid_internal(ValidationStateRef parseState)
{
	SANITY_CHECK_POINTER(parseState);
	SANITY_CHECK_POINTER(parseState->m_state);

	return parseState->m_state != NULL;
}
#endif

bool jschema_isvalid(ValidationStateRef parseState)
{
#if !BYPASS_SCHEMA
	return parseState != NULL && jschema_isvalid_internal(parseState);
#else
	return true;
#endif
}

#if !BYPASS_SCHEMA
static inline void increment_num_items_unsafe(SchemaStateRef parent)
{
	parent->m_numItems++;
}

#if 0
static void increment_num_items(SchemaStateRef parent)
{
	SANITY_CHECK_POINTER(parent);

	if (parent != NULL && parent->m_allowedTypes == ST_ARR) {
		increment_num_items_unsafe(parent);
		assert(parent->m_numItems > 0);
	}
}
#endif

#if TRACK_SCHEMA_PARSING
#define ADD_INHERITED_SCHEMA(parseState, wrapper, inherited, resolver) addInheritedSchema(parseState, wrapper, inherited, resolver)
#else
#define ADD_INHERITED_SCHEMA(parseState, wrapper, inherited, resolver) addInheritedSchema(wrapper, inherited, resolver)
#endif

#if TRACK_SCHEMA_PARSING
static bool addInheritedSchema(ValidationStateRef parseState, SchemaWrapperRef wrapper, jvalue_ref inherited, SchemaResolutionRef resolver) NON_NULL(1, 2, 3, 4);
static bool addInheritedSchema(ValidationStateRef parseState, SchemaWrapperRef wrapper, jvalue_ref inherited, SchemaResolutionRef resolver)
#else
static bool addInheritedSchema(SchemaWrapperRef wrapper, jvalue_ref inherited, SchemaResolutionRef resolver) NON_NULL(1, 2, 3);
static bool addInheritedSchema(SchemaWrapperRef wrapper, jvalue_ref inherited, SchemaResolutionRef resolver)
#endif
{
	assert(jis_array(wrapper->m_validation));
	assert(!jis_all_schema_hierarchy(inherited));
	assert(!jis_empty_schema_dom(inherited));

	SchemaWrapper inheritedWrapper = STACK_SCHEMA_WRAPPER(inherited, wrapper->m_top);
	if (UNLIKELY(!RESOLVE_SCHEMA(parseState, &inheritedWrapper, resolver)))
		return false;

	TRACE_STATE_ARRAY("splicing %p", wrapper->m_validation, inheritedWrapper.m_validation);
	if (UNLIKELY(!jarray_splice_append(wrapper->m_validation, inheritedWrapper.m_validation, SPLICE_COPY))) {
		PJ_LOG_ERR("Failed to append to JSON array");
		assert(false);
		return false;
	}

	TRACE_STATE_ARRAY("intermediary freed", inheritedWrapper.m_validation);
	j_release_wrapper(&inheritedWrapper.m_validation);

	return true;
}

static bool push_new_state(ValidationStateRef parseState, SchemaWrapperRef schema)
{
	SchemaStateRef nextState = (SchemaStateRef) calloc(1, sizeof(struct SchemaState));
	CHECK_POINTER_RETURN_VALUE(nextState, false); // check for out-of-memory
	nextState->m_parent = parseState->m_state;
	nextState->m_schema = jschema_copy(schema);
	determinePossibilities(nextState);

	TRACE_SCHEMA_STATE("created", nextState);

	if (nextState->m_disallowedTypes == ST_ANY || nextState->m_allowedTypes == 0) {
		PJ_SCHEMA_WARN("Schema disallows all input");
		destroy_state(&nextState);
		return false;
	}

	parseState->m_state = nextState;

#if defined _DEBUG && 0
#if PJSON_SCHEMA_TRACE
	PJ_SCHEMA_DBG("<------------------Schema stack");
	SchemaStateRef toMatch = parseState->m_state;
	while (toMatch) {
		PJ_SCHEMA_DBG("Schemas to match against are: %s\n", jvalue_tostring(toMatch->m_schema->m_validation, jschema_all()));
		toMatch = toMatch->m_parent;
	}
	PJ_SCHEMA_DBG("Schema stack--------------------->");
#endif
#endif
	return true;
}

static bool push_next_array_state(ValidationStateRef parseState)
{
	// the schema proposal makes this annoying.  these are the following conditions
	//     items:  an array - tuple typed schemas
	//     items:  an object - schema applies to all elements
	//     additionalProperties: any items falling outside of items must match this
	//                           true or missing - default to everything schema
	//                           false - default to null schema
	//                           if items is missing, this is equivalent to it having this value
	//                           if items is an object, then this property will never be used

	jvalue_ref itemsProperty;
	jvalue_ref elementSchema;
	jvalue_ref additionalProperties;
	jvalue_ref arraySchema;

	assert(parseState->m_state->m_allowedTypes == ST_ARR);

#if ALLOW_LOCAL_REFS
	SchemaWrapperRef nextArraySchema = jschema_wrap_prepare(parseState->m_state->m_schema->m_top);
#else
	SchemaWrapperRef nextArraySchema = jschema_wrap_prepare(NULL);
#endif

	jvalue_ref currentSchemaStack = parseState->m_state->m_schema->m_validation;
	jvalue_ref validationHierarchy = nextArraySchema->m_validation;

	// we need to go over the entire inheritance tree at the current state
	// this is because the schema in the next state will have a pseudo-inheritance
	// based on the current inheritance

	// resolve schema takes the last schema on the stack and expands it fully
	// (in terms of inheritance)
	ssize_t numSchemas = jarray_size(currentSchemaStack);
	for (ssize_t i = 0; i < numSchemas; i++) {
		arraySchema = jarray_get(currentSchemaStack, i);

		assert(jis_object(arraySchema));

		itemsProperty = jobject_get(arraySchema, J_CSTR_TO_BUF(SK_ITEMS));
		if (jis_object(itemsProperty)) {
			if (!ADD_INHERITED_SCHEMA(parseState, nextArraySchema, itemsProperty, &parseState->m_resolutionHandlers)) {
				PJ_SCHEMA_ERR("Failed to resolve all items schema");
				goto schema_gen_failure;
			}
		} else if (jis_array(itemsProperty) && parseState->m_state->m_numItems <= jarray_size(itemsProperty)) {
			// we are in a valid position to grab a schema from "items"
			assert(parseState->m_state->m_numItems > 0);
			elementSchema = jarray_get(itemsProperty, parseState->m_state->m_numItems - 1);
			assert(!jis_all_schema_hierarchy(validationHierarchy));
			jarray_append(validationHierarchy, jvalue_copy(elementSchema));

			if (!RESOLVE_SCHEMA(parseState, nextArraySchema, &parseState->m_resolutionHandlers)) {
				PJ_SCHEMA_ERR("Failed to resolve items schema %zu", parseState->m_state->m_numItems);
				goto schema_gen_failure;
			}
		} else {
			assert (jis_null(itemsProperty));
			// "items" failed to be valid for supplying a schema
			// now we look at "additionalProperties"

			additionalProperties = jobject_get(arraySchema, J_CSTR_TO_BUF(SK_MORE_PROPS));
			if (jis_null(additionalProperties) || jis_boolean(additionalProperties)) {
				bool additionalAllowed = jis_null(additionalProperties) ?
						DEFAULT_ADDITIONAL_PROPERTIES_VALUE : jboolean_deref(additionalProperties);
				if (!additionalAllowed) {
					PJ_SCHEMA_ERR("No more properties in schema allowed");
					goto schema_gen_failure;
				}

				assert(!jis_all_schema_hierarchy(validationHierarchy));
//				jarray_append(validationHierarchy, jobject_create());
			} else {
				assert(jis_object(additionalProperties));

				assert(!jis_all_schema_hierarchy(validationHierarchy));
				jarray_append(validationHierarchy, jvalue_copy(additionalProperties));
				if (!RESOLVE_SCHEMA(parseState, nextArraySchema, &parseState->m_resolutionHandlers)) {
					PJ_SCHEMA_ERR("Failed to resolve additionalProperties schema");
					goto schema_gen_failure;
				}
			}
		}
	}

	if (!push_new_state(parseState, nextArraySchema))
		goto schema_gen_failure;

	jschema_release_internal(&nextArraySchema);

	// wow - we found the schema to validate against for the next array item
	// pat on the back yet?
	return true;

schema_gen_failure:
	jschema_release_internal(&nextArraySchema);
	return false;
}

static SchemaStateRef getNextStateSimple(ValidationStateRef parseState) NON_NULL(1);
static SchemaStateRef getNextStateSimple(ValidationStateRef parseState)
{
	SANITY_CHECK_POINTER(parseState);
	assert(parseState != NULL);

	SchemaStateRef toMatch = parseState->m_state;
	if ((toMatch->m_allowedTypes & ST_ARR) == ST_ARR) {
		if (toMatch->m_arrayOpened) {
			increment_num_items_unsafe(toMatch);

			if (!push_next_array_state(parseState)) {
				return NULL;
			}

			assert(toMatch != parseState->m_state);
			assert(toMatch == parseState->m_state->m_parent);
			toMatch = parseState->m_state;
		}
	}

	return toMatch;
}

static SchemaStateRef getNextState(ValidationStateRef parseState, SchemaType type) NON_NULL(1);
static SchemaStateRef getNextState(ValidationStateRef parseState, SchemaType type)
{
	SchemaStateRef toMatch = getNextStateSimple(parseState);
	if (toMatch == NULL)
		return NULL;

	if ((toMatch->m_disallowedTypes & type) == type) {
		PJ_SCHEMA_INFO("Pruning schema - disallowed type %d matched disallowed types %d", type, toMatch->m_disallowedTypes);
		return NULL;
	}

	if ((toMatch->m_allowedTypes & type) != type) {
		PJ_SCHEMA_INFO("Pruning schema - type %d didn't match allowed types with %d", type, toMatch->m_allowedTypes);
		return NULL;
	}

	toMatch->m_allowedTypes = type;

	return toMatch;
}
#endif

static UNUSED_FUNC void schema_breakpoint(const char *type)
{
	PJ_SCHEMA_TRACIEST("%s: Validating %s", __func__, type);
}

/**
 * On object start here are the things we check:
 *    does the current union allow an object
 *    does the current union disallowed an object
 */
bool jschema_obj(JSAXContextRef sax, ValidationStateRef parseState)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("obj_start");

	// we can't get 2 consecutive object opens because the parser will have failed for us
	// if that is not the case, we should push a "string" state indicating we expect a key

	// only support 1 state at a time currently
	// more will require careful thought about how to properly manage them
	SchemaStateRef toMatch = getNextState(parseState, ST_OBJ);
	if (toMatch == NULL)
		goto schema_failure;

	assert(jis_null(toMatch->m_seenKeys));
	// use object instead of array for hash-based lookup which will
	// help us on object end
	toMatch->m_seenKeys = jobject_create();

	// we managed to validate something against the schema - really???
	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to complete object validation against schema");
	// TODO : only support 1 state at a time currently
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

#if !BYPASS_SCHEMA
static bool is_required_key_present(jvalue_ref seenKeys, jvalue_ref requiredKey, jvalue_ref actualKey) NON_NULL(1, 2, 3);
static bool is_required_key_present(jvalue_ref seenKeys, jvalue_ref requiredKey, jvalue_ref actualKey)
{
	assert(jis_object(seenKeys));
	assert(jis_string(requiredKey));
	assert(jis_string(actualKey));

	if (!jobject_containskey2(seenKeys, requiredKey)) {
#if !PJSON_NO_LOGGING
		raw_buffer propKey = jstring_get_fast(actualKey);
		raw_buffer expectedKey = jstring_get_fast(requiredKey);
		PJ_SCHEMA_WARN("Key %.*s is required by %.*s but was not encountered",
				(int)propKey.m_len, propKey.m_str,
				(int)expectedKey.m_len, expectedKey.m_str);
#endif /* PJSON_NO_LOGGING */
		return false;
	}

	return true;
}
#endif

/**
 * On object end here are the things we check:
 *    have we seen all the keys that are non-optional
 *    have we seen all the keys that other keys might require
 */
bool jschema_obj_end(JSAXContextRef sax, ValidationStateRef parseState)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("obj_end");

	SANITY_CHECK_POINTER(parseState);

	// only support 1 state at a time currently
	// more will require careful thought about how to properly manage them
	SchemaStateRef toMatch = parseState->m_state;

	PJ_SCHEMA_TRACE("1. Object end: Going from schema state\n%s\nto\n%s",
		jvalue_tostring(parseState->m_state->m_schema->m_validation, jschema_all()),
		parseState->m_state->m_parent ? jvalue_tostring(parseState->m_state->m_parent->m_schema->m_validation, jschema_all()) : "");

	assert(toMatch->m_allowedTypes == ST_OBJ);
	assert(jis_object(toMatch->m_seenKeys));

	// an alternative implementation might iterate in reverse (starting with the parent schema
	// moving down the inheritance tree).  however, what this gives us is a way of detecting
	// if there's an inconsistency in the schema (something accepted by a child that is rejected by a parent)
	// TODO : is there any performance reason to choose one over the other?  need real-world examples to begin answering.
	ssize_t numSchemas = jarray_size(toMatch->m_schema->m_validation);
	for (ssize_t i = 0; i < numSchemas; i++) {
		jvalue_ref schemaToValidate = jarray_get(toMatch->m_schema->m_validation, i);
		assert(jis_object(schemaToValidate));

		// TODO: Determine if there is a bug here if `properties' is ommitted in the schema,
		//  but `additionalProperties' isn't.
		jvalue_ref propertiesList = jobject_get(schemaToValidate, J_CSTR_TO_BUF(SK_PROPS));
		if (!jis_null(propertiesList)) {
			jobject_key_value property;
			jvalue_ref value;

			for (jobject_iter i = jobj_iter_init(propertiesList); jobj_iter_is_valid(i); i = jobj_iter_next(i)) {
				if (!jobj_iter_deref(i, &property)) {
					PJ_SCHEMA_ERR("Failed to dereference iterator over properties list");
					goto schema_failure;
				}
				if (!jobject_containskey2(toMatch->m_seenKeys, property.key)) {
					// we haven't seen the key from the list of keys
					// we have schemas for.  is it optional?
					bool optional;

					assert(jis_object(property.value));

					jvalue_ref isOptional = jobject_get(property.value, J_CSTR_TO_BUF(SK_OPTIONAL));
					assert(jis_null(isOptional) || jis_boolean(isOptional));
					if (jis_null(isOptional))
						optional = false;
					else
						optional = jboolean_deref(isOptional);


					if (!optional) {
						// we have a required key in the schema but not in the input - does it provide
						// a default value we can use?
						jvalue_ref defaultVal;
						if (!jobject_get_exists(property.value, J_CSTR_TO_BUF(SK_DEFAULT), &defaultVal)) {
							raw_buffer keyStr UNUSED_VAR;
							keyStr = jstring_get_fast(property.key);
							PJ_SCHEMA_INFO("Key %.*s isn't optional but it is missing", RB_PRINTF(keyStr));
							goto schema_failure;
						}
						if (!jsax_parse_inject(sax, property.key, defaultVal)) {
							raw_buffer keyStr UNUSED_VAR;
							keyStr = jstring_get_fast(property.key);
							PJ_SCHEMA_INFO("The default value for key '%.*s' violates the schema", RB_PRINTF(keyStr));
							goto schema_failure;
						}
					}

					// the key is optional and we haven't seen it
					// not a problem
				} else {
					// we encountered a key - does it require any keys to be present?
					if (!jis_null(value = jobject_get(property.value, J_CSTR_TO_BUF(SK_REQUIRED)))) {
						// EXTENSION: required can be a value or a list of values
						if (jis_string(value)) {
							if (!is_required_key_present(toMatch->m_seenKeys, value, property.key))
								goto schema_failure;
						} else {
							assert(jis_array(value));
							jvalue_ref requiredKey;

							for (ssize_t i = jarray_size(property.value) - 1; i >= 0; i--) {
								requiredKey = jarray_get(property.value, i);
								if (!is_required_key_present(toMatch->m_seenKeys, requiredKey, property.key))
									goto schema_failure;
							}
						}
					}
				}
			}
		} else {
			// no properties list - any logic here is handled by the key validator
		}
	}

	// pop-up to parent
	// we managed to validate against an entire object - really??? wow

	PJ_SCHEMA_TRACE("2. Object end: Going from schema state\n%s\nto\n%s",
		jvalue_tostring(parseState->m_state->m_schema->m_validation, jschema_all()),
		parseState->m_state->m_parent ? jvalue_tostring(parseState->m_state->m_parent->m_schema->m_validation, jschema_all()) : "");

	parseState->m_state = destroy_state(&(parseState->m_state));

	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to complete object validation against schema");
	// TODO : only support 1 state at a time currently
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

/**
 * On array start here are the things we check:
 *    does the current union allow an array
 *    does the current union disallowed an array
 */
bool jschema_arr(JSAXContextRef sax, ValidationStateRef parseState)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("arr_start");

	SANITY_CHECK_POINTER(parseState);
	SchemaStateRef toMatch = getNextState(parseState, ST_ARR);
	if (toMatch == NULL)
		goto schema_failure;

	toMatch->m_arrayOpened = true;

	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to begin array validation against schema");
	// TODO : only support 1 state at a time currently
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

bool jschema_arr_end(JSAXContextRef sax, ValidationStateRef parseState)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("arr_end");

	SANITY_CHECK_POINTER(parseState);

	SchemaStateRef toMatch = parseState->m_state;
	jvalue_ref numItems, schemaToValidate;

	assert(toMatch->m_seenKeys == NULL);
	assert(toMatch->m_allowedTypes == ST_ARR);

	int64_t minItemsSoFar = INT64_MAX;
	int64_t maxItemsSoFar = -1;

	ssize_t numSchemas = jarray_size(toMatch->m_schema->m_validation);
	for (ssize_t i = 0; i < numSchemas; i++) {
		schemaToValidate = jarray_get(toMatch->m_schema->m_validation, i);
		assert(jis_object(schemaToValidate));

		numItems = jobject_get(schemaToValidate, J_CSTR_TO_BUF(SK_MIN_ITEMS));
		if (!jis_null(numItems)) {
			int64_t minRequired;
			if (CONV_OK != jnumber_get_i64(numItems, &minRequired) || minRequired < 0) {
				PJ_SCHEMA_ERR("Minimum number of items is not a valid integer > 0");
				goto schema_failure;
			}
			if (minRequired > minItemsSoFar) {
				PJ_SCHEMA_ERR("Schema violation - inherited minimum items is bigger than parent (child is more generic than parent)");
				goto schema_failure;
			}
			minItemsSoFar = minRequired;
			if (toMatch->m_numItems < minItemsSoFar) {
				PJ_SCHEMA_WARN("Too few items in array: %zd but schema expects at least %"PRId64, toMatch->m_numItems, minItemsSoFar);
				goto schema_failure;
			}
		}

		numItems = jobject_get(schemaToValidate, J_CSTR_TO_BUF(SK_MAX_ITEMS));
		if (!jis_null(numItems)) {
			int64_t maxRequired;
			if (CONV_OK != jnumber_get_i64(numItems, &maxRequired) || maxRequired < 0) {
				PJ_SCHEMA_ERR("Maximum number of items is not a valid integer > 0");
				goto schema_failure;
			}
			if (maxRequired < maxItemsSoFar) {
				PJ_SCHEMA_ERR("Schema violation - inherited minimum items is bigger than parent (child is more generic than parent)");
				goto schema_failure;
			}
			maxItemsSoFar = maxRequired;
			if (toMatch->m_numItems > maxItemsSoFar) {
				PJ_SCHEMA_WARN("Too many items in array: %zd but schema expects at least %"PRId64, toMatch->m_numItems, minItemsSoFar);
				goto schema_failure;
			}
		}
	}

	PJ_SCHEMA_TRACE("Array End: Going from schema state\n%s\nto\n%s",
		jvalue_tostring(parseState->m_state->m_schema->m_validation, jschema_all()),
		jvalue_tostring(parseState->m_state->m_parent->m_schema->m_validation, jschema_all()));

	SANITY_CLEAR_VAR(parseState->m_state->m_arrayOpened, false);
	parseState->m_state = destroy_state(&(parseState->m_state));
	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to complete array validation against schema");
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

bool jschema_key(JSAXContextRef sax, ValidationStateRef parseState, raw_buffer objKey)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("obj_key");

	SANITY_CHECK_POINTER(parseState);

	SchemaStateRef toMatch = parseState->m_state;
	jvalue_ref schemaToValidate, specificSchema;

	assert(toMatch->m_allowedTypes == ST_OBJ);
	assert(toMatch->m_arrayOpened == false);

#if ALLOW_LOCAL_REFS
	SchemaWrapperRef valueSchema = jschema_wrap_prepare(toMatch->m_schema->m_top);
#else
	SchemaWrapperRef valueSchema = jschema_wrap_prepare(NULL);
#endif

	jobject_put(toMatch->m_seenKeys, jstring_create_copy(objKey), jnull());

	ssize_t numSchemas = jarray_size(toMatch->m_schema->m_validation);
	for (ssize_t i = 0; i < numSchemas; i++) {
		schemaToValidate = jarray_get(toMatch->m_schema->m_validation, i);
		assert(jis_object(schemaToValidate));

		specificSchema = jobject_get(schemaToValidate, J_CSTR_TO_BUF(SK_PROPS));
		if (!jis_null(specificSchema)) {
			assert(jis_object(specificSchema));
			specificSchema = jobject_get(specificSchema, objKey);
			if (!jis_null(specificSchema)) {
				assert(jis_object(specificSchema));
				goto append_schema;
			}
			PJ_SCHEMA_ERR("Schema problem - schema for instance key %.*s is null instead of object",
					(int)objKey.m_len, objKey.m_str);
		}

		// properties field either didn't contain the key or didn't exist
		// let's use additionalProperties if it exists
		specificSchema = jobject_get(schemaToValidate, J_CSTR_TO_BUF(SK_MORE_PROPS));
		if (jis_null(specificSchema) || jis_boolean(specificSchema)) {
			bool additionalAllowed = jis_null(specificSchema) ?
					DEFAULT_ADDITIONAL_PROPERTIES_VALUE : jboolean_deref(specificSchema);
			if (additionalAllowed) {
				// append the empty object (match everything)
				assert(!jis_all_schema_hierarchy(valueSchema->m_validation));
//				jarray_append(valueSchema->m_validation, jobject_create());
			} else {
				PJ_SCHEMA_ERR("Schema violation - key without specific key and no unspecified properties allowed");
				goto schema_failure;
			}
		} else {
			assert(jis_object(specificSchema));
			goto append_schema;
		}
		continue;

append_schema:
		assert(!jis_all_schema_hierarchy(valueSchema->m_validation));
		// if specificSchema is a reference, resolve schema will release it for us
		// if it's not, then it will be untouched (i.e. remain the last element in the array)
		// & we'll release ownership of the copy
		specificSchema = jvalue_copy(specificSchema);
		TRACE_STATE_ARRAY("adding %p", valueSchema->m_validation, specificSchema);
		jarray_append(valueSchema->m_validation, specificSchema);
		if (!RESOLVE_SCHEMA(parseState, valueSchema, &parseState->m_resolutionHandlers)) {
			PJ_SCHEMA_ERR("Failed to resolve schema in properties for key %.*s", RB_PRINTF(objKey));
			goto schema_failure;
		}
		assert(jis_array(valueSchema->m_validation));
//		if (specificSchema == jarray_get(valueSchema->m_validation, jarray_size(valueSchema->m_validation) - 1))
//			j_release(&specificSchema);
	}

	if (!push_new_state(parseState, valueSchema))
		goto schema_failure;

	jschema_release_internal(&valueSchema);

	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to validate key '%.*s' against schema", RB_PRINTF(objKey));
	jschema_release_internal(&valueSchema);
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

bool jschema_str(JSAXContextRef sax, ValidationStateRef parseState, raw_buffer str)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("string");

	SchemaStateRef toMatch = getNextState(parseState, ST_STR);
	if (toMatch == NULL) {
		goto schema_failure;
	}

	// check the min string length
	// check the max string length
	// we do this by building up the range of valid values
	int64_t minLength = -1;
	int64_t maxLength = -1; // count on overflow semantics

	{
		jvalue_ref property;
		jvalue_ref schemaToCheck;
		ssize_t arrayLen = jarray_size(toMatch->m_schema->m_validation);
		int64_t specifiedLen;

		for (ssize_t i = 0; i < arrayLen; i++) {
			schemaToCheck = jarray_get(toMatch->m_schema->m_validation, i);
			property = jobject_get(schemaToCheck, J_CSTR_TO_BUF(SK_MIN_LEN));

			if (!jis_null(property)) {
				if (CONV_OK != jnumber_get_i64(property, &specifiedLen)) {
					PJ_SCHEMA_WARN("String length min limit is out of bounds - clamping");
					specifiedLen = 0;
				}
				if (minLength == -1)
					minLength = specifiedLen;
				else if (specifiedLen > minLength) {
					assert(false);
					PJ_SCHEMA_ERR("Schema is invalid - inherited schema has length bounds outside of its parent");
					goto schema_failure;
				}

				if (str.m_len < specifiedLen) {
					PJ_SCHEMA_ERR("String '%.*s' doesn't meet the minimum length of %"PRId64,
							RB_PRINTF(str), specifiedLen);
				}
			}

			property = jobject_get(schemaToCheck, J_CSTR_TO_BUF(SK_MAX_LEN));
			if (!jis_null(property)) {
				if (CONV_OK != jnumber_get_i64(property, &specifiedLen)) {
					PJ_SCHEMA_WARN("String length max limit is out of bounds - clamping");
					specifiedLen = INT64_MAX;
				}
				if (maxLength == -1)
					maxLength = specifiedLen;
				else if (specifiedLen < maxLength) {
					assert(false);
					PJ_SCHEMA_ERR("Schema is invalid - inherited schema has length bounds outside of its parent");
					goto schema_failure;
				}

				if (str.m_len > specifiedLen) {
					PJ_SCHEMA_ERR("String '%.*s' doesn't meet the minimum length of %"PRId64,
							RB_PRINTF(str), specifiedLen);
				}
			}

			property = jobject_get(schemaToCheck, J_CSTR_TO_BUF(SK_ENUM));
			if (!jis_null(property)) {
				if (jis_array(property)) {
					jvalue_ref enumValue;
					for (ssize_t i = jarray_size(property) - 1; i >= 0; i--) {
						enumValue = jarray_get(property, i);
						if (jis_string(enumValue) && jstring_equal2(enumValue, str))
							goto matched_enum;
					}

					PJ_SCHEMA_WARN("Enums specified but string '%.*s' failed to match against '%s",
							(int) str.m_len, str.m_str, jvalue_tostring(property, jschema_all()));
					goto schema_failure;
				} else {
					PJ_SCHEMA_ERR("Invalid enum type %d", property->m_type);
					goto schema_failure;
				}
			}

matched_enum:
			property = jobject_get(schemaToCheck, J_CSTR_TO_BUF(SK_REGEXP));
			if (!jis_null(property)) {
				if (!jis_string(property)) {
					PJ_SCHEMA_ERR("Invalid regexp type %d", property->m_type);
					goto schema_failure;
				}

				// TODO: test regexp
			}
		}
	}

	parseState->m_state = destroy_state(&(parseState->m_state));

	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to validate string '%.*s' against schema", RB_PRINTF(str));
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

bool jschema_num(JSAXContextRef sax, ValidationStateRef parseState, raw_buffer num)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("number");

	SchemaStateRef toMatch = getNextStateSimple(parseState);
	jvalue_ref numValue = NULL;

	if (toMatch == NULL)
		goto schema_failure;

	PJ_SCHEMA_TRACIER("1. Number: Using schema state\n%s\nwithin\n%s",
		jvalue_tostring(parseState->m_state->m_schema->m_validation, jschema_all()),
		jvalue_tostring(parseState->m_state->m_parent->m_schema->m_validation, jschema_all()));

	assert ((ST_NUM & ST_INT) == ST_INT);
#ifdef __GNUC__
	assert (__builtin_popcount(ST_INT) == 1); // 1 bits set
	assert (__builtin_popcount(ST_NUM) == 2); // 2 bit set
#endif

	if ((toMatch->m_disallowedTypes & ST_NUM) != 0) {
		// if integer or number were specified
		if ((toMatch->m_disallowedTypes & ST_NUM) == ST_INT)
			// see note at top regarding disallowed ints/numbers
			PJ_SCHEMA_WARN("Using an undefined mode within the schema - please specify number as disallowed instead of integer");
		PJ_SCHEMA_INFO("Got a number but it's not allowed according to the schema");
		goto schema_failure;
	}

	if ((toMatch->m_allowedTypes & ST_INT) == 0) {
		PJ_SCHEMA_INFO("A number is not in the allowed types at this position");
		goto schema_failure;
	}

	if ((toMatch->m_allowedTypes & ST_NUM) == ST_INT) {
		int64_t integerPortion, exponentPortion, decimalPortion, leadingZeros;
		ConversionResultFlags conversion = parseJSONNumber(&num, &integerPortion, &exponentPortion, &decimalPortion, &leadingZeros);

		if (conversion != CONV_OK) {
			PJ_SCHEMA_WARN("Number %.*s isn't actually a number or is too big.  Errors: %x", (int)num.m_len, num.m_str, conversion);
			goto schema_failure;
		}

		if (decimalPortion != 0 || exponentPortion != 0 || leadingZeros != 0) {
			// TODO: do we need to support this?
			//       the assumption is that an integer must be a simple integer
			//       no exponents or decimal points
			PJ_SCHEMA_INFO("Expecting an integer but got a number");
			goto schema_failure;
		}
	}

	jvalue_ref arraySchema;
	jvalue_ref limit;
	jvalue_ref minimumSoFar = NULL;
	jvalue_ref maximumSoFar = NULL;
	bool maximumValidated = false;
	bool minimumValidated = false;

	// delay actually parsing until we need it
	//
	ssize_t numSchemas = jarray_size(toMatch->m_schema->m_validation);
	for (ssize_t i = 0; i < numSchemas; i++) {
		arraySchema = jarray_get(toMatch->m_schema->m_validation, i);
		assert (jis_object(arraySchema));

		if (!jis_null(limit = jobject_get(arraySchema, J_CSTR_TO_BUF(SK_MIN_VALUE)))) {
			if (!jis_number(limit)) {
				PJ_SCHEMA_ERR("Minimum value must be a number");
				assert(false);
				goto schema_failure;
			}

			if (minimumSoFar != NULL && jnumber_compare(minimumSoFar, limit) < 0) {
				PJ_SCHEMA_ERR("Schema inheritance violation - child schema accepts a potentially wider range of numbers");
				goto schema_failure;
			}
			minimumSoFar = limit;

			if (!minimumValidated) {
				if (UNLIKELY(numValue == NULL))
					numValue = jnumber_create_converted(num);
				if (jnumber_compare(limit, numValue) < 0) {
					PJ_SCHEMA_INFO("Schema violation - number '%.*s' is too small",
							(int)num.m_len, num.m_str);
					goto schema_failure;
				}
				minimumValidated = true;
			}
		}

		if (!jis_null(limit = jobject_get(arraySchema, J_CSTR_TO_BUF(SK_MAX_VALUE)))) {
			if (!jis_number(limit)) {
				PJ_SCHEMA_ERR("Maximum value must be a number");
				assert(false);
				goto schema_failure;
			}

			if (maximumSoFar != NULL && jnumber_compare(maximumSoFar, limit) > 0) {
				PJ_SCHEMA_ERR("Schema inheritance violation - child schema accepts a potentially wider range of numbers");
				goto schema_failure;
			}
			maximumSoFar = limit;

			if (!maximumValidated) {
				if (UNLIKELY(numValue == NULL))
					numValue = jnumber_create_converted(num);

				if (jnumber_compare(limit, numValue) < 0) {
					PJ_SCHEMA_INFO("Schema violation - number '%.*s' is too small",
							(int)num.m_len, num.m_str);
					goto schema_failure;
				}
				maximumValidated = false;
			}
		}

		if (!jis_null(limit = jobject_get(arraySchema, J_CSTR_TO_BUF(SK_ENUM)))) {
			if (!jis_array(limit)) {
				PJ_SCHEMA_ERR("Enum must be an array");
				goto schema_failure;
			}
			if (UNLIKELY(numValue == NULL))
				numValue = jnumber_create_converted(num);

			jvalue_ref enumValue;
			for (ssize_t j = jarray_size(limit) - 1; j >= 0; j--) {
				enumValue = jarray_get(limit, j);
				if (jis_number(enumValue) && jnumber_compare(enumValue, numValue) == 0)
					goto enum_found;
			}

			PJ_SCHEMA_INFO("Number not found in enums");
			goto schema_failure;

enum_found:
			;
		}
	}

	PJ_SCHEMA_TRACIER("2. Number: Going from schema state\n%s\nto\n%s",
		jvalue_tostring(parseState->m_state->m_schema->m_validation, jschema_all()),
		jvalue_tostring(parseState->m_state->m_parent->m_schema->m_validation, jschema_all()));

	parseState->m_state = destroy_state(&parseState->m_state);
	j_release(&numValue);
	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to validate number '%.*s' against schema", RB_PRINTF(num));
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	j_release(&numValue);
	return false;
#else
	return true;
#endif
}

bool jschema_bool(JSAXContextRef sax, ValidationStateRef parseState, bool truth)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("bool");

	SchemaStateRef toMatch = getNextState(parseState, ST_BOOL);
	if (toMatch == NULL)
		goto schema_failure;

	ssize_t numSchemas = jarray_size(toMatch->m_schema->m_validation);
	jvalue_ref schema;
	jvalue_ref enums;
	for (ssize_t i = 0; i < numSchemas; i++) {
		schema = jarray_get(toMatch->m_schema->m_validation, i);
		assert(jis_object(schema));
		if (!jis_null(enums = jobject_get(schema, J_CSTR_TO_BUF(SK_ENUM)))) {
			if (!jis_array(enums)) {
				PJ_SCHEMA_ERR("Enum must be an array");
				goto schema_failure;
			}

			jvalue_ref enumValue;
			for (ssize_t j = jarray_size(enums) - 1; j >= 0; j--) {
				enumValue = jarray_get(enums, j);
				if (jis_boolean(enumValue) && jboolean_deref(enumValue) == truth)
					goto enum_found;
			}

			PJ_SCHEMA_INFO("Boolean %d not found in enums", (int)truth);
			goto schema_failure;
enum_found:
			;
		}
	}

	PJ_SCHEMA_TRACIER("Boolean: Going from schema state\n%s\nto\n%s",
		jvalue_tostring(parseState->m_state->m_schema->m_validation, jschema_all()),
		jvalue_tostring(parseState->m_state->m_parent->m_schema->m_validation, jschema_all()));

	parseState->m_state = destroy_state(&parseState->m_state);
	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to validate boolean '%s' against schema", (truth ? "true" : "false"));
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

bool jschema_null(JSAXContextRef sax, ValidationStateRef parseState)
{
#if !BYPASS_SCHEMA
	schema_breakpoint("null");

	SchemaStateRef toMatch = getNextState(parseState, ST_NULL);
	if (toMatch == NULL)
		goto schema_failure;

	ssize_t numSchemas = jarray_size(toMatch->m_schema->m_validation);
	jvalue_ref schema;
	jvalue_ref enums;
	for (ssize_t i = 0; i < numSchemas; i++) {
		schema = jarray_get(toMatch->m_schema->m_validation, i);
		assert(jis_object(schema));
		if (!jis_null(enums = jobject_get(schema, J_CSTR_TO_BUF(SK_ENUM)))) {
			if (!jis_array(enums)) {
				PJ_SCHEMA_ERR("Enum must be an array");
				goto schema_failure;
			}

			jvalue_ref enumValue;
			for (ssize_t j = jarray_size(enums) - 1; j >= 0; j--) {
				enumValue = jarray_get(enums, j);
				if (jis_null(enumValue))
					goto enum_found;
			}

			PJ_SCHEMA_INFO("Null not found in enums");
			goto schema_failure;
enum_found:
			;
		}
	}

	PJ_SCHEMA_TRACIER("Null: Going from schema state\n%s\nto\n%s",
		jvalue_tostring(parseState->m_state->m_schema->m_validation, jschema_all()),
		jvalue_tostring(parseState->m_state->m_parent->m_schema->m_validation, jschema_all()));

	parseState->m_state = destroy_state(&parseState->m_state);
	return true;

schema_failure:
	PJ_SCHEMA_INFO("Failed to validate null against schema");
	destroy_branch(&(parseState->m_state), NULL);
	parseState->m_state = NULL;
	return false;
#else
	return true;
#endif
}

#if !BYPASS_SCHEMA
static SchemaWrapper NULL_SCHEMA = {
	.m_refCnt = 1,
	.m_validation = NULL,
#if ALLOW_LOCAL_REFS
	.m_top = NULL,
#endif
};
#endif

bool jis_null_schema(SchemaWrapperRef schema)
{
#if !BYPASS_SCHEMA
	return schema == NULL || schema == &NULL_SCHEMA;
#else
	return false;
#endif
}

#if !BYPASS_SCHEMA
static struct jvalue DEFAULT_SCHEMA = {
		.m_type = JV_OBJECT,
		.m_refCnt = 1,
		.m_toString = "{}",
		.m_toStringDealloc = NULL,
		.value.val_obj = {
				.m_table = {
						.m_bucket = {
								{
									.list = {
											.prev = NULL,
											.next = NULL
									},
									.entry = {
											.key = NULL,
											.value = NULL
									}
								}
						},
						.m_next = NULL
				},
				.m_start = {
						.list = {
								.prev = NULL,
								.next = NULL
						},
						.entry = {
								.key = NULL,
								.value = NULL
						}
				}
		}
	};

static struct jvalue DEFAULT_SCHEMA_STACK = {
	.m_type = JV_ARRAY,
	.m_refCnt = 1,
	.m_toString = "[{}]",
	.m_toStringDealloc = NULL,
	.value.val_array = {
			.m_smallBucket = {
				&DEFAULT_SCHEMA, NULL
			},
			.m_bigBucket = NULL,
			.m_size = 1,
			.m_capacity = 16
	}
};

static void j_release_wrapper(jvalue_ref *value)
{
	assert(value != NULL);
	SANITY_CHECK_POINTER(*value);
	if (LIKELY(*value != &DEFAULT_SCHEMA_STACK && *value != &DEFAULT_SCHEMA))
		j_release(value);
	SANITY_KILL_POINTER(*value);
}

static bool jis_all_schema_hierarchy(jvalue_ref value)
{
	assert(!jis_empty_schema_dom(value));
	return value == &DEFAULT_SCHEMA_STACK;
}

static bool jis_empty_schema_dom(jvalue_ref value)
{
	return value == &DEFAULT_SCHEMA;
}

static SchemaWrapper ALL_SCHEMA = {
	.m_refCnt = 1,
	.m_validation = &DEFAULT_SCHEMA_STACK,
#if ALLOW_LOCAL_REFS
	.m_top = &DEFAULT_SCHEMA
#endif
};
#endif

jschema_ref jschema_all()
{
#if !BYPASS_SCHEMA
	static SchemaWrapperRef allSchemaRef = NULL;
	if (UNLIKELY(allSchemaRef == NULL)) {
		allSchemaRef = &ALL_SCHEMA;
		// need to fix up linked list head pointer in object
		list_head *list = &allSchemaRef->m_validation->
				value.val_array.m_smallBucket[0]->
				value.val_obj.m_start.list;
		list->next = list->prev = list;
		assert(jarray_size(allSchemaRef->m_validation) == 1);
		assert(jis_object(jarray_get(allSchemaRef->m_validation, 0)));
		assert(jobject_size(jarray_get(allSchemaRef->m_validation, 0)) == 0);
#if 0
		allSchemaRef->m_refCnt = 1;
		allSchemaRef->m_top = jschema_all_stack();
		allSchemaRef->m_validation = jschema_all_stack();
#endif
	}
	return &ALL_SCHEMA;
#else
	return NULL;
#endif
}

