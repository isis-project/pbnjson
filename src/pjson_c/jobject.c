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

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <compiler/inline_attribute.h>
#include <compiler/nonnull_attribute.h>
#include <compiler/builtins.h>
#include <math.h>
#include <inttypes.h>

#include <jobject.h>
#include <jschema_internal.h>

#include <sys_malloc.h>
#include <sys/mman.h>
#include "jobject_internal.h"
#include "liblog.h"
#include "jvalue/num_conversion.h"
#include "linked_list.h"

#ifdef DBG_C_MEM
#define PJ_LOG_MEM(...) PJ_LOG_INFO(__VA_ARGS__)
#else
#define PJ_LOG_MEM(...) do { } while (0)
#endif

#ifdef DBG_C_REFCNT
#define PJ_LOG_REFCNT(...) PJ_LOG_INFO(__VA_ARGS__)
#else
#define PJ_LOG_REFCNT(...) do { } while (0)
#endif

#ifndef PJSON_EXPORT
#error "Compiling with the wrong options"
#endif

#include <jgen_stream.h>
#include "gen_stream.h"

#include "liblog.h"

#define CONST_C_STRING(string) (string), sizeof(string)
#define VAR_C_STRING(string) (string), strlen(string)

#ifndef NDEBUG
static int s_inGdb = 0;
#define NUM_TERM_NULL 1
#else
#define NUM_TERM_NULL 0
#endif

#define TRACE_REF(format, pointer, ...) PJ_LOG_TRACE("TRACE JVALUE_REF: %p " format, pointer, ##__VA_ARGS__)

// 7 NULL bytes is enough to ensure that any Unicode string will be NULL-terminated
// even if it is malformed Unicode
#define SAFE_TERM_NULL_LEN 7

jvalue JNULL = {
	.m_type = JV_NULL,
	.m_refCnt = 1,
	.m_toString = "null",
	.m_toStringDealloc = NULL
};

static jvalue JEMPTY_STR = {
	.value.val_str = {
		.m_dealloc = NULL,
		.m_data = {
			.m_str = "",
			.m_len = 0,
		}
	},
	.m_type = JV_STR,
	.m_refCnt = 1,
	.m_toString = "",
	.m_toStringDealloc = NULL
};

static const char *jvalue_tostring_internal (jvalue_ref val, jschema_ref schema, bool schemaNecessary);
static void jvalue_to_string_append (jvalue_ref jref, JStreamRef generating);
static void jobject_to_string_append (jvalue_ref jref, JStreamRef generating);
static void jarray_to_string_append (jvalue_ref jref, JStreamRef generating);
static void jnumber_to_string_append (jvalue_ref jref, JStreamRef generating);
static jvalue_ref jnumber_duplicate (jvalue_ref num) NON_NULL(1);
static inline void jstring_to_string_append (jvalue_ref jref, JStreamRef generating);
static inline void jboolean_to_string_append (jvalue_ref jref, JStreamRef generating);

static inline jobject_iter JO_ITER (struct list_head *p)
{
	return (jobject_iter) {p};
}

bool jbuffer_equal(raw_buffer buffer1, raw_buffer buffer2)
{
	return buffer1.m_len == buffer2.m_len &&
			memcmp(buffer1.m_str, buffer2.m_str, buffer1.m_len) == 0;
}

static void jvalue_to_string_append (jvalue_ref jref, JStreamRef generating)
{
	SANITY_CHECK_POINTER(jref);
	if (jref == NULL) {
		PJ_LOG_ERR("Internal error.  Using NULL pointer instead of reference to NULL JSON object");
		jref = &JNULL;
	}
	CHECK_POINTER_MSG(generating, "Internal problem due to buffer to append to being null");
	switch (jref->m_type) {
		case JV_NULL:
			generating->null_value (generating);
			break;
		case JV_OBJECT:
			jobject_to_string_append (jref, generating);
			break;
		case JV_ARRAY:
			jarray_to_string_append (jref, generating);
			break;
		case JV_NUM:
			jnumber_to_string_append (jref, generating);
			break;
		case JV_STR:
			jstring_to_string_append (jref, generating);
			break;
		case JV_BOOL:
			jboolean_to_string_append (jref, generating);
			break;
	}
}

/**
 * NOTE: The structure returned (if not null) is always initialized to 0 except for the
 * reference count (1) and the type (set to the first parameter)
 * @param type The type of JSON value to create
 * @return NULL or a reference to a valid, dynamically allocated, structure that isn't a JSON null reference.
 */
static jvalue_ref jvalue_create (JValueType type)
{
	jvalue_ref new_value = (jvalue_ref) calloc (1, sizeof(jvalue));
	CHECK_ALLOC_RETURN_NULL(new_value);
	new_value->m_refCnt = 1;
	new_value->m_type = type;
	TRACE_REF("created", new_value);
	return new_value;
}

#if PJSON_LOG_INFO && !PJSON_NO_LOGGING && DBG_C_REFCNT
#define COUNT_EMPTY_NULL 1
#endif

#if COUNT_EMPTY_NULL
static int jnull_cnt = 0;
static int jempty_cnt = 0;
#endif

jvalue_ref jvalue_copy (jvalue_ref val)
{
	SANITY_CHECK_POINTER(val);
	CHECK_POINTER_RETURN(val);
	assert(s_inGdb || val->m_refCnt > 0);

	if (val == &JNULL) {
#if COUNT_EMPTY_NULL
		PJ_LOG_REFCNT("attempt to grab ownership of JSON Null object: %d", ATOMIC_INC(&jnull_cnt));
#endif
		return val;
	} else if (val == &JEMPTY_STR) {
#if COUNT_EMPTY_NULL
		PJ_LOG_REFCNT("attempt to grab ownership of empty string constant: %d", ATOMIC_INC(&jempty_cnt));
#endif
		return val;
	}

	val->m_refCnt++;
	TRACE_REF("inc refcnt to %d", val, val->m_refCnt);
	return val;
}

jvalue_ref jvalue_duplicate (jvalue_ref val)
{
	jvalue_ref result = val;
	SANITY_CHECK_POINTER(val);

	if (jis_null (val) || val == &JEMPTY_STR) return result;

	if (jis_object (val)) {
		result = jobject_create_hint (jobject_size (val));
		jobject_iter i;
		jobject_key_value pair;
		jvalue_ref valueCopy;

		for (i = jobj_iter_init (val); jobj_iter_is_valid (i); i = jobj_iter_next (i)) {
			if (!jobj_iter_deref (i, &pair)) {
				j_release (&result);
				result = NULL;
				break;
			}
			valueCopy = jvalue_duplicate (pair.value);
			if (!jobject_put (result, jvalue_copy (pair.key), valueCopy)) {
				j_release (&valueCopy);
				j_release (&pair.key);
				j_release (&result);
				result = NULL;
				break;
			}
		}
	} else if (jis_array (val)) {
		ssize_t arrSize = jarray_size (val);
		result = jarray_create_hint (NULL, arrSize);
		for (ssize_t i = arrSize - 1; i >= 0; i--) {
			if (!jarray_append (result, jvalue_duplicate (jarray_get (val, i)))) {
				j_release (&result);
				result = NULL;
				break;
			}
		}
		return result;
	} else {
		// string, number, & boolean are immutable, so no need to do an actual duplication
#if 0
		return jvalue_copy(val);
#else
		if (jis_string(val)) {
			result = jstring_create_copy(jstring_get_fast(val));
		} else if (jis_number(val)) {
			result = jnumber_duplicate(val);
		} else
			result = jboolean_create(jboolean_deref(val));
#endif
	}

	TRACE_REF("w/ refcnt of %d, deep copy to %p w/ refcnt of %d",
	          val, val->m_refCnt, result, result->m_refCnt);

	return result;
}

static void j_destroy_object (jvalue_ref obj) NON_NULL(1);
static void j_destroy_array (jvalue_ref arr) NON_NULL(1);
static void j_destroy_string (jvalue_ref str) NON_NULL(1);
static void j_destroy_number (jvalue_ref num) NON_NULL(1);
static inline void j_destroy_boolean (jvalue_ref boolean) NON_NULL(1);

void j_release (jvalue_ref *val)
{
	SANITY_CHECK_POINTER(val);
	CHECK_POINTER(val);
	if (UNLIKELY(*val == NULL)) {
		SANITY_KILL_POINTER(*val);
		return;
	}
	if (UNLIKELY(*val == &JNULL)) {
#if COUNT_EMPTY_NULL
		int newVal = ATOMIC_DEC(&jnull_cnt);
		PJ_LOG_REFCNT("attempt to release ownership of global JSON null object: %d", newVal);
		assert(newVal >= 0);
#endif
		SANITY_KILL_POINTER(*val);
		return;
	} else if (UNLIKELY(*val == &JEMPTY_STR)) {
#if COUNT_EMPTY_NULL
		int newVal = ATOMIC_DEC(&jempty_cnt);
		PJ_LOG_REFCNT("attempt to release ownership of global empty JSON string: %d", newVal);
		assert(newVal >= 0);
#endif
		SANITY_KILL_POINTER(*val);
		return;
	}

	assert((*val)->m_refCnt > 0);

	if ((*val)->m_refCnt == 1) {
		TRACE_REF("freeing because refcnt is 0: %s", *val, jvalue_tostring(*val, jschema_all()));
		if ((*val)->m_toStringDealloc) {
			PJ_LOG_MEM("Freeing string representation of jvalue %p", (*val)->m_toString);
			(*val)->m_toStringDealloc ((*val)->m_toString);
		}
		SANITY_KILL_POINTER((*val)->m_toString);

		switch ( (*val)->m_type) {
			case JV_OBJECT:
				j_destroy_object (*val);
				break;
			case JV_ARRAY:
				j_destroy_array (*val);
				break;
			case JV_STR:
				j_destroy_string (*val);
				break;
			case JV_NUM:
				j_destroy_number (*val);
				break;
			case JV_BOOL:
				j_destroy_boolean (*val);
				break;
			case JV_NULL:
				PJ_LOG_ERR("Invalid program state - should've already returned from j_release before this point");
				assert(false);
				break;
		}

		if ((*val)->m_backingBuffer.m_str) {
			if ((*val)->m_backingBufferMMap) {
				munmap((void *)(*val)->m_backingBuffer.m_str, (*val)->m_backingBuffer.m_len);
			} else {
				free((void *)(*val)->m_backingBuffer.m_str);
			}
		}

		SANITY_CLEAR_VAR((*val)->m_refCnt, 0);
		PJ_LOG_MEM("Freeing %p", *val);
		free (*val);
	} else if (UNLIKELY((*val)->m_refCnt < 0)) {
		PJ_LOG_ERR("reference counter messed up - memory corruption and/or random crashes are possible");
		assert(false);
	} else {
		(*val)->m_refCnt--;
		TRACE_REF("decrement ref cnt to %d: %s", *val, (*val)->m_refCnt, jvalue_tostring(*val, jschema_all()));
	}
	SANITY_KILL_POINTER(*val);
}

bool jis_null (jvalue_ref val)
{
	SANITY_CHECK_POINTER(val);
	if (val == &JNULL || val == NULL) {
		assert(val == NULL || val->m_type == JV_NULL);
	}
	else {
		assert(val->m_type != JV_NULL);
	}
	return val == &JNULL || val == NULL;
}

jvalue_ref jnull ()
{
	return &JNULL;
}

static const char *jvalue_tostring_internal (jvalue_ref val, jschema_ref schema, bool schemaNecessary)
{
	SANITY_CHECK_POINTER(val);
	CHECK_POINTER_RETURN_VALUE(val, "null");

	if (UNLIKELY(schemaNecessary && jis_null_schema(schema))) {
		PJ_LOG_ERR("Attempt to generate JSON stream without a schema even though it is mandatory");
		return NULL;
	}

	if (!val->m_toString) {
		StreamStatus error;
		JStreamRef generating = jstreamInternal (schema, TOP_None);
		jvalue_to_string_append (val, generating);
		val->m_toString = generating->finish (generating, &error);
		val->m_toStringDealloc = free;
		assert (val->m_toString != NULL);
	}

	return val->m_toString;
}

const char * jvalue_tostring (jvalue_ref val, const jschema_ref schema)
{
	if (val->m_toStringDealloc)
		val->m_toStringDealloc(val->m_toString);
	val->m_toString = NULL;
	return jvalue_tostring_internal (val, schema, true);
}

/************************* JSON OBJECT API **************************************/
#define DEREF_OBJ(ref) ((ref)->value.val_obj)

static inline int key_hash_raw (raw_buffer *str) NON_NULL(1);
static inline int key_hash (jvalue_ref key) NON_NULL(1);

static inline int key_hash_raw (raw_buffer *str)
{
	// TODO: incorporate the length in the hash
	assert(str->m_str != NULL);
	return OBJECT_BUCKET_MODULO(str->m_str[0] + str->m_len);
}

static void j_destroy_object (jvalue_ref ref)
{
	jkey_value_array *toFree, *nextTable;

	SANITY_CHECK_POINTER(ref);
	assert(jis_object(ref));

	for (jobject_iter i = jobj_iter_init(ref); jobj_iter_is_valid(i); i = jobj_iter_remove(i));

	toFree = DEREF_OBJ(ref).m_table.m_next;
	SANITY_KILL_POINTER(DEREF_OBJ(ref).m_table.m_next);
	SANITY_KILL_POINTER(DEREF_OBJ(ref).m_start.list.prev);
	SANITY_KILL_POINTER(DEREF_OBJ(ref).m_start.list.next);

	while (toFree) {
		SANITY_CHECK_MEMORY(toFree, sizeof(jkey_value_array));
#ifdef DEBUG_FREED_POINTERS
		for (int i = 0; i < OBJECT_BUCKET_SIZE; i++) {
//			assert(toFree->m_bucket[i].entry.key == FREED_POINTER || toFree->m_bucket[i].entry.key == NULL);
//			assert(toFree->m_bucket[i].entry.value == FREED_POINTER || toFree->m_bucket[i].entry.value == NULL);
		}
#endif

		nextTable = toFree->m_next;
		PJ_LOG_MEM("Freeing object bucket array %p", toFree);
		SANITY_FREE(free, jkey_value_array *, toFree, sizeof(jkey_value_array));
		toFree = nextTable;
	}
}

jvalue_ref jobject_create ()
{
	jvalue_ref new_obj = jvalue_create (JV_OBJECT);
	CHECK_POINTER_RETURN_NULL(new_obj);
	INIT_LIST_HEAD(&DEREF_OBJ(new_obj).m_start.list);
	return new_obj;
}

static jobject_key_value* jobject_find (jkey_value_array *toCheck, raw_buffer *key, jkey_value_array **table) NON_NULL(1);
static jobject_key_value* jobject_find2(jkey_value_array *toCheck, jvalue_ref key, jkey_value_array **table) NON_NULL(1);
static bool jobject_insert_internal (jvalue_ref object, jkey_value_array *table, jobject_key_value item) NON_NULL(1);
static bool jstring_equal_internal(jvalue_ref str, jvalue_ref other) NON_NULL(1, 2);
static inline bool jstring_equal_internal2(jvalue_ref str, raw_buffer *other) NON_NULL(1, 2);
static bool jstring_equal_internal3(raw_buffer *str, raw_buffer *other) NON_NULL(1, 2);

static bool jobject_insert_internal (jvalue_ref object, jkey_value_array *table, jobject_key_value item)
{
	int bucket;

	bucket = key_hash (item.key);

	// is a key present at the current spot in the table
	if (UNLIKELY(table->m_bucket[bucket].entry.key != NULL)) {
		// is it the same key or just a hash collision?
		if (LIKELY(!jstring_equal_internal(table->m_bucket[bucket].entry.key, item.key))) {
			// hash collision - go on to next table
			if (!table->m_next) {
				table->m_next = (jkey_value_array *) calloc (1, sizeof(jkey_value_array));
				CHECK_ALLOC_RETURN_VALUE(table->m_next, false);
			}
			return jobject_insert_internal (object, table->m_next, item);
		}
		// we're replacing an existing key, so we release our ownership over our existing children
		j_release(&table->m_bucket[bucket].entry.key);
		j_release(&table->m_bucket[bucket].entry.value);
	} else {
		// only need to add new elements to the linked list - not on replace
		ladd (& (table->m_bucket [bucket].list), DEREF_OBJ(object).m_start.list.prev);
	}

	table->m_bucket [bucket].entry.key = item.key;
	table->m_bucket [bucket].entry.value = item.value;

	assert(DEREF_OBJ(object).m_start.entry.key == NULL);
	assert(DEREF_OBJ(object).m_start.entry.value == NULL);
	assert(DEREF_OBJ(object).m_start.list.next != NULL);
	assert(DEREF_OBJ(object).m_start.list.prev == &table->m_bucket[bucket].list);
	assert(&DEREF_OBJ(object).m_start.list == table->m_bucket[bucket].list.next);

	return true;
}

static void jobject_to_string_append (jvalue_ref jref, JStreamRef generating)
{
	SANITY_CHECK_POINTER(jref);

	generating->o_begin (generating);
	if (!jis_object (jref)) {
		const char *asStr = jvalue_tostring_internal (jref, NULL, false);
		generating->string (generating, J_CSTR_TO_BUF("Internal error - not an object"));
		generating->string (generating, j_cstr_to_buffer(asStr));
		// create invalid JSON on purpose
		return;
	}

	for (jobject_iter i = jobj_iter_init (jref); jobj_iter_is_valid (i); i = jobj_iter_next (i)) {
		jobject_key_value key_value;
		if (!jobj_iter_deref (i, &key_value)) {
			generating->string (generating, J_CSTR_TO_BUF("failed to dereference"));
			return;
		}
		assert(jis_string(key_value.key));
		jvalue_to_string_append (key_value.key, generating);
		jvalue_to_string_append (key_value.value, generating);
	}

	generating->o_end (generating);
}

jvalue_ref jobject_create_var (jobject_key_value item, ...)
{
	va_list ap;
	jobject_key_value arg;
	jvalue_ref new_object = jobject_create ();

	CHECK_POINTER_RETURN_NULL(new_object);

	if (item.key != NULL) {
		assert(jis_string(item.key));
		assert(item.value != NULL);

		if (UNLIKELY(!jobject_insert_internal (new_object, & (DEREF_OBJ(new_object).m_table), item))) {
			j_release (&new_object);
			return jnull();
		}

		va_start (ap, item);
		while ( (arg = va_arg (ap, jobject_key_value)).key != NULL) {
			assert(jis_string(arg.key));
			assert(arg.value != NULL);
			if (UNLIKELY(!jobject_insert_internal (new_object, & (DEREF_OBJ(new_object).m_table), arg))) {
				PJ_LOG_ERR("Failed to insert requested key/value into new object");
				j_release (&new_object);
				new_object = jnull();
				break;
			}
		}
		va_end (ap);
	}

	return new_object;
}

jvalue_ref jobject_create_hint (int capacityHint)
{
	jvalue_ref new_object = jobject_create ();
	jkey_value_array **expansion;

	CHECK_POINTER_RETURN_NULL(new_object);

	expansion = &new_object->value.val_obj.m_table.m_next;

	while (capacityHint > OBJECT_BUCKET_SIZE) {
		assert (*expansion == NULL);
		*expansion = calloc (1, sizeof(jkey_value_array));
		CHECK_ALLOC_RETURN_VALUE(*expansion, new_object);
		expansion = & ( (*expansion)->m_next);
		capacityHint -= OBJECT_BUCKET_SIZE;
	}

	return new_object;
}

bool jis_object (jvalue_ref val)
{
	SANITY_CHECK_POINTER(val);
	CHECK_POINTER_RETURN_VALUE(val, false);
	assert_msg(s_inGdb || val->m_refCnt > 0, "%p is garbage", val);

	return val->m_type == JV_OBJECT;
}

size_t jobject_size (jvalue_ref obj)
{
	// FIXME: use a counter that is updated on insertion/removal instead of iterating every time
	size_t result = 0;

	SANITY_CHECK_POINTER(obj);

	CHECK_CONDITION_RETURN_VALUE(!jis_object(obj), result, "Attempt to retrieve size from something not an object");

	for (jobject_iter i = jobj_iter_init (obj); jobj_iter_is_valid (i); i = jobj_iter_next (i))
		result++;

	return result;
}

static jobject_key_value* jobject_find (jkey_value_array *toCheck, raw_buffer *key, jkey_value_array **table)
{
	jvalue_ref keyInTable;
	int bucket;

	SANITY_CHECK_POINTER(key->m_str);
	SANITY_CHECK_POINTER(table);

	assert(toCheck != NULL);
	assert(key->m_str != NULL);
	assert(key->m_len != 0);

	if (table) *table = toCheck;

	while (toCheck) {
		// VL: I always think an iterator is what should be used here every time I look at this
		// because for some reason I think I was stupid enough to iterate over all buckets
		// in the table.  However, I keep reminding myself - this looks at a specific bucket (based off
		// of the hash of the key) before continuing to the next bucket.  With a good hash algorithm
		// that is computationally cheap but also efficient at distribution, it'll be way
		// faster than an iterator.

		SANITY_CHECK_POINTER(toCheck);
		bucket = key_hash_raw (key);

		keyInTable = toCheck->m_bucket [bucket].entry.key;

		assert(keyInTable != jnull());
		if (keyInTable == NULL) break;

		if (jstring_equal_internal2 (keyInTable, key)) {
			if (table) *table = toCheck;
			return & (toCheck->m_bucket [bucket].entry);
		}

		if (table) *table = toCheck;
		toCheck = toCheck->m_next;
	}

	return NULL;
}

bool jobject_get_exists (jvalue_ref obj, raw_buffer key, jvalue_ref *value)
{
	jobject_key_value *result;

	assert(jis_object(obj));

	CHECK_CONDITION_RETURN_VALUE(jis_null(obj), false, "Attempt to cast null %p to object", obj);
	CHECK_CONDITION_RETURN_VALUE(!jis_object(obj), false, "Attempt to cast type %d to object (%d)", obj->m_type, JV_OBJECT);

	result = jobject_find (&DEREF_OBJ(obj).m_table, &key, NULL);
	if (result != NULL) {
		if (value) *value = result->value;
		return true;
	}

	return false;
}

bool jobject_get_exists2 (jvalue_ref obj, jvalue_ref key, jvalue_ref *value)
{
	jobject_key_value *result;

	assert(jis_object(obj));

	CHECK_CONDITION_RETURN_VALUE(jis_null(obj), false, "Attempt to cast null %p to object", obj);
	CHECK_CONDITION_RETURN_VALUE(!jis_object(obj), false, "Attempt to cast type %d to object (%d)", obj->m_type, JV_OBJECT);

	result = jobject_find2 (&DEREF_OBJ(obj).m_table, key, NULL);
	if (result != NULL) {
		if (value) *value = result->value;
		return true;
	}

	return false;
}

jvalue_ref jobject_get (jvalue_ref obj, raw_buffer key)
{
	jvalue_ref result;
	assert_msg(jis_object(obj), "%p is not an object", obj);
	assert(key.m_str != NULL);
	if (jobject_get_exists (obj, key, &result)) return result;
	return jnull ();
}

bool jobject_remove (jvalue_ref obj, raw_buffer key)
{
	jobject_key_value *entry;

	assert(jis_object(obj));
	assert(key.m_str != NULL);

	CHECK_CONDITION_RETURN_VALUE(jis_null(obj), false, "Attempt to cast null %p to object", obj);
	CHECK_CONDITION_RETURN_VALUE(!jis_object(obj), false, "Attempt to cast type %d to object (%d)", obj->m_type, JV_OBJECT);

	entry = jobject_find (&DEREF_OBJ(obj).m_table, &key, NULL);
	if (entry == NULL) return false;

	j_release (& (entry->key));
	j_release (& (entry->value));
	entry->key = NULL;

	return true;
}

bool jobject_set (jvalue_ref obj, raw_buffer key, jvalue_ref val)
{
	jvalue_ref newKey, newVal;

	newVal = jvalue_copy (val);
	//CHECK_CONDITION_RETURN_VALUE(jis_null(newVal) && !jis_null(val), false, "Failed to create a copy of the value")

	newKey = jstring_create_copy (key);
	if (jis_null (newKey)) {
		PJ_LOG_ERR("Failed to create a copy of %.*s", (int)key.m_len, key.m_str);
		j_release (&newVal);
		return false;
	}

	if (UNLIKELY(!jobject_put(obj, newKey, newVal))) {
		j_release (&newKey);
		j_release (&newVal);
		return false;
	}
	return true;
}

bool jobject_put (jvalue_ref obj, jvalue_ref key, jvalue_ref val)
{
	jkey_value_array *table;

	SANITY_CHECK_POINTER(obj);
	SANITY_CHECK_POINTER(key);
	SANITY_CHECK_POINTER(val);

	assert(jis_object(obj));
	assert(jis_string(key));
	assert(val != NULL);

	CHECK_POINTER_RETURN_NULL(key);
	CHECK_CONDITION_RETURN_VALUE(!jis_string(key), false, "%p is %d not a string (%d)", key, key->m_type, JV_STR);
	CHECK_CONDITION_RETURN_VALUE(jstring_size(key) == 0, false, "Object instance name is the empty string");

	table = &DEREF_OBJ(obj).m_table;

	if (val == NULL) {
		PJ_LOG_WARN("Please don't pass in NULL - use jnull() instead");
		val = jnull ();
	}

	jobject_insert_internal (obj, table, jkeyval(key, val));

	return true;
}

	// JSON Object iterators
jobject_iter jobj_iter_init (const jvalue_ref obj)
{
	SANITY_CHECK_POINTER(obj);

	if (obj == NULL)
		return JO_ITER(NULL);

	assert(jis_object(obj));

	CHECK_CONDITION_RETURN_VALUE(!jis_object(obj), JO_ITER(NULL), "Cannot iterate over non-object");

	SANITY_CHECK_POINTER(DEREF_OBJ(obj).m_start.list.next);

	return JO_ITER (DEREF_OBJ(obj).m_start.list.next);
}

jobject_iter jobj_iter_init_last(const jvalue_ref obj)
{
	SANITY_CHECK_POINTER(obj);

	if (obj == NULL)
		return JO_ITER(NULL);

	assert(jis_object(obj));

	CHECK_CONDITION_RETURN_VALUE(!jis_object(obj), JO_ITER(NULL), "Cannot iterator over non-object");

	SANITY_CHECK_POINTER(DEREF_OBJ(obj).m_start.list.prev);

	return JO_ITER (&DEREF_OBJ(obj).m_start.list);
}

static inline jobject_iter jobj_iter_next_internal(const jobject_iter i)
{
	SANITY_CHECK_POINTER(i.m_opaque);
	SANITY_CHECK_POINTER(((list_head *)(i.m_opaque))->next);

	return (jobject_iter) {((list_head *)(i.m_opaque))->next};
}

static inline jobject_iter jobj_iter_previous_internal(const jobject_iter i)
{
	SANITY_CHECK_POINTER(i.m_opaque);
	SANITY_CHECK_POINTER(((list_head *)(i.m_opaque))->next);

	return (jobject_iter) {((list_head *)(i.m_opaque))->prev};
}

jobject_iter jobj_iter_next (const jobject_iter i)
{
	if (UNLIKELY(!jobj_iter_is_valid(i))) {
		PJ_LOG_WARN("Invalid use of API - cannot increment an iterator that isn't valid");
		assert(false);
		return i;
	}

	return jobj_iter_next_internal(i);
}

jobject_iter jobj_iter_previous (const jobject_iter i)
{
	if (UNLIKELY(!jobj_iter_is_valid(i))) {
		PJ_LOG_WARN("Invalid use of API - cannot decrement an iterator that isn't valid");
		assert(false);
		return i;
	}

	return jobj_iter_previous_internal(i);
}

bool jobj_iter_is_valid (const jobject_iter i)
{
	SANITY_CHECK_POINTER(i.m_opaque);
	if (i.m_opaque == NULL)
		return false;

	return NULL != lentry(i.m_opaque, jo_keyval_iter, list)->entry.key;
}

jobject_iter jobj_iter_remove (jobject_iter i)
{
	if (UNLIKELY(!jobj_iter_is_valid(i))) {
		PJ_LOG_WARN("Invalid use of API - cannot remove the elements when the iterator that isn't valid");
		assert(false);
		return i;
	}

	jobject_iter next = jobj_iter_next_internal(i);

	ldel(i.m_opaque);
	SANITY_KILL_POINTER(((list_head *)i.m_opaque)->prev);
	SANITY_KILL_POINTER(((list_head *)i.m_opaque)->next);

	j_release(&lentry(i.m_opaque, jo_keyval_iter, list)->entry.key);
	j_release(&lentry(i.m_opaque, jo_keyval_iter, list)->entry.value);

	return next;
}

bool jobj_iter_deref (const jobject_iter i, jobject_key_value *value)
{
	SANITY_CHECK_POINTER(value);

	assert(jobj_iter_is_valid(i));
	assert(value != NULL);

	if (UNLIKELY(!jobj_iter_is_valid(i))) {
		PJ_LOG_WARN("Invalid use of API - attempt to dereference invalid iterator");
		return false;
	}

	memcpy (value, &lentry(i.m_opaque, jo_keyval_iter, list)->entry, sizeof(jobject_key_value));

	assert(jis_string(value->key));

	return true;
}

bool jobj_iter_equal (const jobject_iter i1, const jobject_iter i2)
{
	SANITY_CHECK_POINTER(i1.m_opaque);
	SANITY_CHECK_POINTER(i2.m_opaque);
	return (i1.m_opaque != NULL) && (i2.m_opaque != NULL) && i1.m_opaque == i2.m_opaque;
}

#undef DEREF_OBJ
/************************* JSON OBJECT API **************************************/

/************************* JSON ARRAY API  *************************************/
#define DEREF_ARR(ref) ((ref)->value.val_array)

static bool jarray_put_unsafe (jvalue_ref arr, ssize_t index, jvalue_ref val) NON_NULL(1, 3);
static inline ssize_t jarray_size_unsafe (jvalue_ref arr) NON_NULL(1);
static inline void jarray_size_increment_unsafe (jvalue_ref arr) NON_NULL(1);
static inline void jarray_size_decrement_unsafe (jvalue_ref arr) NON_NULL(1);
static jvalue_ref* jarray_get_unsafe (jvalue_ref arr, ssize_t index) NON_NULL(1);
static inline void jarray_size_set_unsafe (jvalue_ref arr, ssize_t newSize) NON_NULL(1);
static inline bool jarray_expand_capacity (jvalue_ref arr, ssize_t newSize) NON_NULL(1);
static bool jarray_expand_capacity_unsafe (jvalue_ref arr, ssize_t newSize) NON_NULL(1);
static void jarray_remove_unsafe (jvalue_ref arr, ssize_t index) NON_NULL(1);

static bool valid_array (jvalue_ref array) NON_NULL(1);
static bool valid_array (jvalue_ref array)
{
	SANITY_CHECK_POINTER(array);
	assert(jis_array(array));

	CHECK_CONDITION_RETURN_VALUE(!jis_array(array), false, "Attempt to append into non-array %p", array);

	return true;
}

static bool valid_index (ssize_t index)
{
	CHECK_CONDITION_RETURN_VALUE(index < 0, false, "Negative array index %zd", index);
	return true;
}

static bool valid_index_bounded (jvalue_ref arr, ssize_t index) NON_NULL(1);
static bool valid_index_bounded (jvalue_ref arr, ssize_t index)
{
	CHECK_CONDITION_RETURN_VALUE(!valid_array(arr), false, "Trying to test index bounds test on non-array");
	if (UNLIKELY(!valid_index(index))) return false;

	CHECK_CONDITION_RETURN_VALUE(index >= jarray_size(arr), false, "Index %zd out of bounds of array size %zd", index, jarray_size(arr));

	return true;
}

static void j_destroy_array (jvalue_ref arr)
{
	SANITY_CHECK_POINTER(arr);
	SANITY_CHECK_POINTER(DEREF_ARR(arr).m_bigBucket);
	assert(jis_array(arr));

#ifdef DEBUG_FREED_POINTERS
	for (ssize_t i = jarray_size(arr); i < DEREF_ARR(arr).m_capacity; i++) {
		jvalue_ref *outsideValue = jarray_get_unsafe(arr, i);
		assert(*outsideValue == NULL || *outsideValue == FREED_POINTER);
	}
#endif

	assert(jarray_size(arr) >= 0);

	for (int i = jarray_size(arr) - 1; i >= 0; i--)
		jarray_remove_unsafe(arr, i);

	assert(jarray_size(arr) == 0);

	PJ_LOG_MEM("Destroying array bucket at %p", DEREF_ARR(arr).m_bigBucket);
	SANITY_FREE(free, jvalue_ref *, DEREF_ARR(arr).m_bigBucket, DEREF_ARR(arr).m_capacity - ARRAY_BUCKET_SIZE);
}

static void jarray_to_string_append (jvalue_ref jref, JStreamRef generating)
{
	ssize_t i;
	assert(jis_array(jref));

	if (UNLIKELY(!generating)) {
		PJ_LOG_ERR("Cannot append string value to NULL output stream");
		return;
	}

	SANITY_CHECK_POINTER(jref);
	if (UNLIKELY(jis_null(jref))) {
		PJ_LOG_ERR("INTERNAL ERROR!!!!!!!!!! - used internal API for array --> string for JSON null");
		generating->null_value (generating);
		return;
	}

	generating->a_begin (generating);
	for (i = 0; i < jarray_size (jref); i++) {
		jvalue_ref toAppend = jarray_get (jref, i);
		SANITY_CHECK_POINTER(toAppend);
		jvalue_to_string_append (toAppend, generating);
	}
	generating->a_end (generating);
}

jvalue_ref jarray_create (jarray_opts opts)
{
	jvalue_ref new_array = jvalue_create (JV_ARRAY);
	CHECK_ALLOC_RETURN_NULL(new_array);

	DEREF_ARR(new_array).m_capacity = ARRAY_BUCKET_SIZE;
	return new_array;
}

jvalue_ref jarray_create_var (jarray_opts opts, ...)
{
	// jarray_create_hint will take care of the capacity for us
	jvalue_ref new_array = jarray_create_hint (opts, 1);
	jvalue_ref element;

	CHECK_POINTER_RETURN_NULL(new_array);

	va_list iter;

	va_start (iter, opts);
	while ( (element = va_arg (iter, jvalue_ref)) != NULL) {
		jarray_put_unsafe (new_array, jarray_size_unsafe (new_array), element);
	}
	va_end (iter);

	return new_array;
}

/**
 * Create an empty array with the specified properties and the hint that the array will eventually contain capacityHint elements.
 *
 * @param opts The options for the array (currently unspecified).  NULL indicates use default options.
 * @param capacityHint A guess-timate of the eventual size of the array (implementation is free to ignore this).
 * @return A reference to the created array value.  The caller has ownership.
 */
jvalue_ref jarray_create_hint (jarray_opts opts, size_t capacityHint)
{
	jvalue_ref new_array = jarray_create (opts);
	if (UNLIKELY(capacityHint == 0)) {
		PJ_LOG_INFO("Non-recommended use of API providing a hint of 0 elements.  instead, maybe use jarray_create?");
	} else if (LIKELY(new_array != NULL)) {
		jarray_expand_capacity_unsafe (new_array, capacityHint);
	}

	return new_array;
}

/**
 * Determine whether or not the reference to the JSON value represents an array.
 *
 * @param val The reference to test
 * @return True if it is an array, false otherwise.
 */
bool jis_array (jvalue_ref val)
{
	SANITY_CHECK_POINTER(val);
	CHECK_POINTER_RETURN_VALUE(val, false);
	assert(s_inGdb || val->m_refCnt > 0);

	return val->m_type == JV_ARRAY;
}

ssize_t jarray_size (jvalue_ref arr)
{
	CHECK_CONDITION_RETURN_VALUE(!valid_array(arr), 0, "Attempt to get array size of non-array %p", arr);
	return jarray_size_unsafe (arr);
}

static inline ssize_t jarray_size_unsafe (jvalue_ref arr)
{
	assert(jis_array(arr));

	return DEREF_ARR(arr).m_size;
}

static inline void jarray_size_increment_unsafe (jvalue_ref arr)
{
	assert(jis_array(arr));

	++DEREF_ARR(arr).m_size;

	assert(jarray_size_unsafe(arr) <= DEREF_ARR(arr).m_capacity);
}

static inline void jarray_size_decrement_unsafe (jvalue_ref arr)
{
	assert(jis_array(arr));

	--DEREF_ARR(arr).m_size;

	assert(jarray_size_unsafe(arr) >= 0);
}

static inline void jarray_size_set_unsafe (jvalue_ref arr, ssize_t newSize)
{
	assert(jis_array(arr));
	assert(newSize <= DEREF_ARR(arr).m_capacity);

	DEREF_ARR(arr).m_size = newSize;
}

static jvalue_ref* jarray_get_unsafe (jvalue_ref arr, ssize_t index)
{
	assert(valid_array(arr));
	assert(index >= 0);
	assert(index < DEREF_ARR(arr).m_capacity);

	if (UNLIKELY(OUTSIDE_ARR_BUCKET_RANGE(index))) {
		return &DEREF_ARR(arr).m_bigBucket [index - ARRAY_BUCKET_SIZE];
	}
	assert(index < ARRAY_BUCKET_SIZE);
	return &DEREF_ARR(arr).m_smallBucket [index];
}

jvalue_ref jarray_get (jvalue_ref arr, ssize_t index)
{
	jvalue_ref result;

	CHECK_CONDITION_RETURN_VALUE(!valid_array(arr), 0, "Attempt to get array size of non-array %p", arr);
	CHECK_CONDITION_RETURN_VALUE(!valid_index_bounded(arr, index), jnull(), "Attempt to get array element from %p with out-of-bounds index value %zd", arr, index);

	result = * (jarray_get_unsafe (arr, index));
	if (result == NULL)
	// need to fix up in case we haven't assigned anything to that space - it's initialized to NULL (JSON undefined)
	result = jnull ();
	return result;
}

static void jarray_remove_unsafe (jvalue_ref arr, ssize_t index)
{
	ssize_t i;
	jvalue_ref *hole, *toMove;
	ssize_t array_size;

	assert(valid_index_bounded(arr, index));

	hole = jarray_get_unsafe (arr, index);
	assert (hole != NULL);
	j_release (hole);

	array_size = jarray_size_unsafe (arr);

	// Shift down all elements
	for (i = index + 1; i < array_size; i++) {
		toMove = jarray_get_unsafe (arr, i);
		*hole = *toMove;
		hole = toMove;
	}

	jarray_size_decrement_unsafe (arr);

	// This is necessary because someone else might reference this position, and
	// they need to know that it's empty (in case they need to free it).
	*hole = NULL;
}

bool jarray_remove (jvalue_ref arr, ssize_t index)
{
	assert(valid_array(arr));
	assert(valid_index_bounded(arr, index));

	CHECK_CONDITION_RETURN_VALUE(!valid_array(arr), false, "Attempt to get array size of non-array %p", arr);
	CHECK_CONDITION_RETURN_VALUE(!valid_index_bounded(arr, index), jnull(), "Attempt to get array element from %p with out-of-bounds index value %zd", arr, index);

	jarray_remove_unsafe (arr, index);

	return true;
}

static inline bool jarray_expand_capacity (jvalue_ref arr, ssize_t newSize)
{
	assert(jis_array(arr));

	CHECK_CONDITION_RETURN_VALUE(!jis_array(arr), false, "Attempt to expand something that wasn't a JSON array reference: %p", arr);

	return jarray_expand_capacity_unsafe (arr, newSize);
}

static bool jarray_expand_capacity_unsafe (jvalue_ref arr, ssize_t newSize)
{
	assert(jis_array(arr));
	assert(newSize >= 0);

	if (newSize > DEREF_ARR(arr).m_capacity) {
		// m_capacity is always a minimum of the bucket size
		assert(OUTSIDE_ARR_BUCKET_RANGE(newSize));
		assert(newSize > ARRAY_BUCKET_SIZE);
		jvalue_ref *newBigBucket = realloc (DEREF_ARR(arr).m_bigBucket, sizeof(jvalue_ref) * (newSize - ARRAY_BUCKET_SIZE));
		if (UNLIKELY(newBigBucket == NULL)) {
			assert(false);
			return false;
		}

		PJ_LOG_MEM("Resized %p from %zu bytes to %p with %zu bytes", DEREF_ARR(arr).m_bigBucket, sizeof(jvalue_ref)*(DEREF_ARR(arr).m_capacity - ARRAY_BUCKET_SIZE), newBigBucket, sizeof(jvalue_ref)*(newSize - ARRAY_BUCKET_SIZE));

		for (ssize_t x = DEREF_ARR(arr).m_capacity - ARRAY_BUCKET_SIZE; x < newSize - ARRAY_BUCKET_SIZE; x++)
			newBigBucket[x] = NULL;

		DEREF_ARR(arr).m_bigBucket = newBigBucket;
		DEREF_ARR(arr).m_capacity = newSize;
	}

	return true;
}

static bool jarray_put_unsafe (jvalue_ref arr, ssize_t index, jvalue_ref val)
{
	jvalue_ref *old;
	SANITY_CHECK_POINTER(arr);
	assert(jis_array(arr));

	if (!jarray_expand_capacity_unsafe (arr, index + 1)) {
		PJ_LOG_WARN("Failed to expand array to allocate element - memory allocation problem?");
		return false;
	}

	old = jarray_get_unsafe(arr, index);
	j_release(old);
	*old = val;

	if (index >= jarray_size_unsafe (arr)) jarray_size_set_unsafe (arr, index + 1);

	return true;
}

bool jarray_set (jvalue_ref arr, ssize_t index, jvalue_ref val)
{
	jvalue_ref arr_val;

	assert(jis_array(arr));

	CHECK_CONDITION_RETURN_VALUE(!jis_array(arr), false, "Attempt to get array size of non-array %p", arr);
	CHECK_CONDITION_RETURN_VALUE(index < 0, false, "Attempt to set array element for %p with negative index value %zd", arr, index);

	if (UNLIKELY(val == NULL)) {
		PJ_LOG_WARN("incorrect API use - please pass an actual reference to a JSON null if that's what you want - assuming that's what you meant");
		val = jnull ();
	}

	arr_val = jvalue_copy (val);
	CHECK_ALLOC_RETURN_VALUE(arr_val, false);

	return jarray_put_unsafe (arr, index, arr_val);
}

bool jarray_put (jvalue_ref arr, ssize_t index, jvalue_ref val)
{
	assert(jis_array(arr));

	CHECK_CONDITION_RETURN_VALUE(!jis_array(arr), false, "Attempt to insert into non-array %p", arr);
	CHECK_CONDITION_RETURN_VALUE(index < 0, false, "Attempt to insert array element for %p with negative index value %zd", arr, index);

	if (UNLIKELY(val == NULL)) {
		PJ_LOG_WARN("incorrect API use - please pass an actual reference to a JSON null if that's what you want - assuming that's the case");
		val = jnull ();
	}

	return jarray_put_unsafe (arr, index, val);
}

bool jarray_append (jvalue_ref arr, jvalue_ref val)
{
	SANITY_CHECK_POINTER(val);
	SANITY_CHECK_POINTER(arr);

	assert(jis_array(arr));
	assert(jis_null(val) || jis_object(val) ||
			jis_array(val) || jis_string(val) ||
			jis_number(val) || jis_boolean(val));
	CHECK_CONDITION_RETURN_VALUE(!jis_array(arr), false, "Attempt to append into non-array %p", arr);
	CHECK_CONDITION_RETURN_VALUE(!valid_array(arr), false, "Attempt to append into non-array %p", arr);

	if (UNLIKELY(val == NULL)) {
		PJ_LOG_WARN("incorrect API use - please pass an actual reference to a JSON null if that's what you want - assuming that's the case");
		val = jnull ();
	}

	return jarray_put_unsafe (arr, jarray_size_unsafe (arr), val);
}

/**
 * Insert the value into the array before the specified position.
 *
 * arr[index] now contains val, with all elements shifted appropriately.
 *
 * NOTE: It is unspecified behaviour to modify val after passing it to this array
 *
 * @param arr
 * @param index
 * @param val
 *
 * @see jarray_append
 * @see jarray_put
 */
bool jarray_insert(jvalue_ref arr, ssize_t index, jvalue_ref val)
{
	ssize_t j;

	SANITY_CHECK_POINTER(arr);
	assert(arr != NULL);

	CHECK_CONDITION_RETURN_VALUE(!valid_array(arr), false, "Array to insert into isn't a valid reference to a JSON DOM node: %p", arr);
	CHECK_CONDITION_RETURN_VALUE(index < 0, false, "Invalid index - must be >= 0: %zd", index);

	{
		jvalue_ref *toMove, *hole;
		// we increment the size of the array
		jarray_put_unsafe(arr, jarray_size_unsafe(arr), jnull());

		// stopping at the first jis_null as an optimization is actually
		// wrong because we change the array structure.  we have to move up
		// all the elements.
		hole = jarray_get_unsafe(arr, jarray_size_unsafe(arr) - 1);

		for (j = jarray_size_unsafe(arr) - 1; j > index; j--, hole = toMove) {
			toMove = jarray_get_unsafe(arr, j - 1);
			*hole = *toMove;
		}

		*hole = val;
	}

	return true;
}

bool jarray_splice (jvalue_ref array, ssize_t index, ssize_t toRemove, jvalue_ref array2, ssize_t begin, ssize_t end, JSpliceOwnership ownership)
{
	ssize_t i, j;
	size_t removable = toRemove;
	jvalue_ref *valueInOtherArray;
	jvalue_ref valueToInsert;

	if (LIKELY(removable)) {
		CHECK_CONDITION_RETURN_VALUE(!valid_index_bounded(array, index), false, "Splice index is invalid");
		CHECK_CONDITION_RETURN_VALUE(!valid_index_bounded(array, index + toRemove - 1), false, "To remove amount is out of bounds of array");
	} else {
		CHECK_CONDITION_RETURN_VALUE(!valid_array(array), false, "Array isn't valid");
		if (index < 0) index = 0;
	}
	CHECK_CONDITION_RETURN_VALUE(begin >= end, false, "Invalid range to copy from second array: [%zd, %zd)", begin, end); // set notation
	CHECK_CONDITION_RETURN_VALUE(!valid_index_bounded(array2, begin), false, "Start index is invalid for second array");
	CHECK_CONDITION_RETURN_VALUE(!valid_index_bounded(array2, end - 1), false, "End index is invalid for second array");
	CHECK_CONDITION_RETURN_VALUE(toRemove < 0, false, "Invalid amount %zd to remove during splice", toRemove);

	for (i = index, j = begin; removable && j < end; i++, removable--, j++) {
		assert(valid_index_bounded(array, i));
		assert(valid_index_bounded(array2, j));
		valueInOtherArray = jarray_get_unsafe(array2, j);
		valueToInsert = *valueInOtherArray;
		assert(valueInOtherArray != NULL);
		switch (ownership) {
			case SPLICE_TRANSFER:
				*valueInOtherArray = NULL;
				jarray_size_decrement_unsafe (array2);
				break;
			case SPLICE_NOCHANGE:
				break;
			case SPLICE_COPY:
				valueToInsert = jvalue_copy(valueToInsert);
				break;
		}
		jarray_put_unsafe (array, i, valueToInsert);
	}

	if (removable != 0) {
		assert (j == end);
		assert (toRemove > end - begin);

		while (removable) {
			jarray_remove_unsafe (array, i);
			removable--;
		}
	} else if (j < end) {
		assert (toRemove < end - begin);
		assert (removable == 0);

		jarray_expand_capacity_unsafe (array, jarray_size_unsafe (array) + (end - j));

		// insert any remaining elements that don't overlap the amount to remove
		for (; j < end; j++, i++) {
			assert(valid_index_bounded(array2, j));

			valueInOtherArray = jarray_get_unsafe(array2, j);
			valueToInsert = *valueInOtherArray;
			assert(valueInOtherArray != NULL);
			switch (ownership) {
				case SPLICE_TRANSFER:
					*valueInOtherArray = NULL;
					jarray_size_decrement_unsafe (array2);
					break;
				case SPLICE_NOCHANGE:
					break;
				case SPLICE_COPY:
					valueToInsert = jvalue_copy(valueToInsert);
					break;
			}
			if (UNLIKELY(!jarray_insert(array, i, valueToInsert))) {
				PJ_LOG_ERR("How did this happen? Failed to insert %zd from second array into %zd of first array", j, i);
				return false;
			}
		}
	} else {
		assert (toRemove == end - begin);
	}
	return true;
}

bool jarray_splice_inject (jvalue_ref array, ssize_t index, jvalue_ref arrayToInject, JSpliceOwnership ownership)
{
	return jarray_splice (array, index, 0, arrayToInject, 0, jarray_size (arrayToInject), ownership);
}

bool jarray_splice_append (jvalue_ref array, jvalue_ref arrayToAppend, JSpliceOwnership ownership)
{
	return jarray_splice (array, jarray_size (array) - 1, 0, arrayToAppend, 0, jarray_size (arrayToAppend), ownership);
}

#undef DEREF_ARR

/****************************** JSON STRING API ************************/
#define DEREF_STR(ref) ((ref)->value.val_str)

#define SANITY_CHECK_JSTR_BUFFER(jval) 					\
	do {								\
		SANITY_CHECK_POINTER(jval);				\
		SANITY_CHECK_POINTER(DEREF_STR(jval).m_data.m_str);	\
		SANITY_CHECK_MEMORY(DEREF_STR(jval).m_data.m_str, DEREF_STR(jval).m_data.m_len);	\
		SANITY_CHECK_POINTER(DEREF_STR(jval).m_dealloc);	\
	} while (0)

static inline void jstring_to_string_append (jvalue_ref jref, JStreamRef generating)
{
	generating->string (generating, DEREF_STR(jref).m_data);
}

static void j_destroy_string (jvalue_ref str)
{
	assert(str != &JEMPTY_STR);
	SANITY_CHECK_JSTR_BUFFER(str);
#ifdef _DEBUG
	if (str == NULL) {
		PJ_LOG_ERR("Internal error - string reference to release the string buffer for is NULL");
		return;
	}
#endif
	if (DEREF_STR(str).m_dealloc) {
		PJ_LOG_MEM("Destroying string %p", DEREF_STR(str).m_data.m_str);
		SANITY_CLEAR_MEMORY(DEREF_STR(str).m_data.m_str, DEREF_STR(str).m_data.m_len);
		SANITY_FREE_CUST(DEREF_STR(str).m_dealloc, char *, DEREF_STR(str).m_data.m_str, DEREF_STR(str).m_data.m_len);
	}
	PJ_LOG_MEM("Changing string %p to NULL for %p", DEREF_STR(str).m_data.m_str, str);
	SANITY_KILL_POINTER(DEREF_STR(str).m_data.m_str);
	SANITY_CLEAR_VAR(DEREF_STR(str).m_data.m_len, -1);
}

static inline int key_hash (jvalue_ref key)
{
	assert(jis_string(key));
	return key_hash_raw (&DEREF_STR(key).m_data);
}

jvalue_ref jstring_empty ()
{
	return &JEMPTY_STR;
}

jvalue_ref jstring_create (const char *cstring)
{
	return jstring_create_utf8 (cstring, strlen (cstring));
}

jvalue_ref jstring_create_utf8 (const char *cstring, ssize_t length)
{
	if (length < 0) length = strlen (cstring);
	return jstring_create_copy (j_str_to_buffer (cstring, length));
}

jvalue_ref jstring_create_copy (raw_buffer str)
{
	char *copyBuffer;
	copyBuffer = calloc (str.m_len + SAFE_TERM_NULL_LEN, sizeof(char));
	if (copyBuffer == NULL) {
		PJ_LOG_ERR("Failed to allocate space for private string copy");
		return jnull();
	}
	memcpy(copyBuffer, str.m_str, str.m_len);

	jvalue_ref new_str = jstring_create_nocopy_full(j_str_to_buffer(copyBuffer, str.m_len), free);
	CHECK_POINTER_RETURN_NULL(new_str);

	DEREF_STR(new_str).m_data.m_len = str.m_len;
	SANITY_CHECK_JSTR_BUFFER(new_str);

	return new_str;
}

static jobject_key_value* jobject_find2(jkey_value_array *toCheck, jvalue_ref key, jkey_value_array **table)
{
	SANITY_CHECK_JSTR_BUFFER(key);
	return jobject_find(toCheck, &DEREF_STR(key).m_data, table);
}

bool jis_string (jvalue_ref str)
{
#ifdef DEBUG_FREED_POINTERS
	if (str->m_type == JV_STR)
		SANITY_CHECK_JSTR_BUFFER(str);
#endif
	CHECK_POINTER_RETURN_VALUE(str, false);
	assert(s_inGdb || str->m_refCnt > 0);

	return str->m_type == JV_STR;
}

jvalue_ref jstring_create_nocopy (raw_buffer val)
{
	return jstring_create_nocopy_full (val, NULL);
}

jvalue_ref jstring_create_nocopy_full (raw_buffer val, jdeallocator buffer_dealloc)
{
	jvalue_ref new_string;

	SANITY_CHECK_POINTER(val.m_str);
	SANITY_CHECK_MEMORY(val.m_str, val.m_len);
	CHECK_CONDITION_RETURN_VALUE(val.m_str == NULL, jnull(), "Invalid string to set JSON string to NULL");
	if (val.m_len == 0) {
		if (buffer_dealloc) buffer_dealloc((void *)val.m_str);
		return &JEMPTY_STR;
	}

	new_string = jvalue_create (JV_STR);
	CHECK_POINTER_RETURN_NULL(new_string);

	DEREF_STR(new_string).m_dealloc = buffer_dealloc;
	DEREF_STR(new_string).m_data = val;

	SANITY_CHECK_JSTR_BUFFER(new_string);

	return new_string;
}

ssize_t jstring_size (jvalue_ref str)
{
	SANITY_CHECK_JSTR_BUFFER(str);
	CHECK_CONDITION_RETURN_VALUE(!jis_string(str), 0, "Invalid parameter - %d is not a string (%d)", str->m_type, JV_STR);

	assert(DEREF_STR(str).m_data.m_str);

	return DEREF_STR(str).m_data.m_len;
}

raw_buffer jstring_get (jvalue_ref str)
{
	SANITY_CHECK_JSTR_BUFFER(str);
	char *str_copy;

	// performs the error checking for us as well
	raw_buffer raw_str = jstring_get_fast (str);
	if (UNLIKELY(raw_str.m_str == NULL)) return raw_str;

	str_copy = calloc (raw_str.m_len + 1, sizeof(char));
	if (str_copy == NULL) {
		return j_str_to_buffer (NULL, 0);
	}

	memcpy (str_copy, raw_str.m_str, raw_str.m_len);

	return j_str_to_buffer (str_copy, raw_str.m_len);
}

raw_buffer jstring_get_fast (jvalue_ref str)
{
	SANITY_CHECK_JSTR_BUFFER(str);
	CHECK_CONDITION_RETURN_VALUE(!jis_string(str), j_str_to_buffer(NULL, 0), "Invalid API use - attempting to get string buffer for non JSON string %p", str);

	return DEREF_STR(str).m_data;
}

static bool jstring_equal_internal(jvalue_ref str, jvalue_ref other)
{
	SANITY_CHECK_JSTR_BUFFER(str);
	SANITY_CHECK_JSTR_BUFFER(other);
	return str == other ||
			jstring_equal_internal2(str, &DEREF_STR(other).m_data);
}

static inline bool jstring_equal_internal2(jvalue_ref str, raw_buffer *other)
{
	SANITY_CHECK_JSTR_BUFFER(str);
	SANITY_CHECK_MEMORY(other->m_str, other->m_len);
	return jstring_equal_internal3(&DEREF_STR(str).m_data, other);
}

static bool jstring_equal_internal3(raw_buffer *str, raw_buffer *other)
{
	SANITY_CHECK_MEMORY(str->m_str, str->m_len);
	SANITY_CHECK_MEMORY(other->m_str, other->m_len);
	return str->m_str == other->m_str ||
			(
					str->m_len == other->m_len &&
					memcmp(str->m_str, other->m_str, str->m_len) == 0
			);
}

bool jstring_equal (jvalue_ref str, jvalue_ref other)
{
	SANITY_CHECK_JSTR_BUFFER(str);
	SANITY_CHECK_JSTR_BUFFER(other);

	if (UNLIKELY(!jis_string(str) || !jis_string(other))) {
		PJ_LOG_WARN("attempting to check string equality but not using a JSON string");
		return false;
	}

	return jstring_equal_internal(str, other);
}

bool jstring_equal2 (jvalue_ref str, raw_buffer other)
{
	if (UNLIKELY(!jis_string(str))) {
		PJ_LOG_WARN("attempting to check string equality but not a JSON string");
		return false;
	}

	return jstring_equal_internal2(str, &other);
}

#undef DEREF_STR
/******************************** JSON STRING API **************************************/
#define DEREF_NUM(ref) ((ref)->value.val_num)

static void j_destroy_number (jvalue_ref num)
{
	SANITY_CHECK_POINTER(num);
	assert(jis_number(num));

	if (DEREF_NUM(num).m_type != NUM_RAW) {
		return;
	}

	assert(DEREF_NUM(num).value.raw.m_str != NULL);
	SANITY_CHECK_POINTER(DEREF_NUM(num).value.raw.m_str);

	if (DEREF_NUM(num).m_rawDealloc) {
		PJ_LOG_MEM("Destroying raw numeric string %p", DEREF_NUM(num).value.raw.m_str);
		DEREF_NUM(num).m_rawDealloc ((char *)DEREF_NUM(num).value.raw.m_str);
	}
	PJ_LOG_MEM("Clearing raw numeric string from %p to NULL for %p", DEREF_NUM(num).value.raw.m_str, num);
	SANITY_KILL_POINTER(DEREF_NUM(num).value.raw.m_str);
	SANITY_CLEAR_VAR(DEREF_NUM(num).value.raw.m_len, 0);
}

static void jnumber_to_string_append (jvalue_ref jref, JStreamRef generating)
{
	SANITY_CHECK_POINTER(jref);
	if (DEREF_NUM(jref).m_error) {
		PJ_LOG_WARN("converting a number that has an error (%d) set to a string", DEREF_NUM(jref).m_error);
	}

	switch (DEREF_NUM(jref).m_type) {
		case NUM_RAW:
			assert(DEREF_NUM(jref).value.raw.m_len != 0);
			generating->number (generating, DEREF_NUM(jref).value.raw);
			break;
		case NUM_FLOAT:
			generating->floating (generating, DEREF_NUM(jref).value.floating);
			break;
		case NUM_INT:
			generating->integer (generating, DEREF_NUM(jref).value.integer);
			break;
		default:
			// mismatched on purpose so that generation yields an error
			assert(false);
			generating->o_begin (generating);
			raw_buffer asStrBuf = J_CSTR_TO_BUF("Error - Unrecognized number type");
			generating->string (generating, asStrBuf);
			generating->integer (generating, jref->value.val_num.m_type);
			break;
	}
}

jvalue_ref jnumber_duplicate (jvalue_ref num)
{
	assert (jis_number(num));

	switch (DEREF_NUM(num).m_type) {
	case NUM_RAW:
		return jnumber_create(DEREF_NUM(num).value.raw);
	case NUM_FLOAT:
		return jnumber_create_f64(DEREF_NUM(num).value.floating);
	case NUM_INT:
		return jnumber_create_i64(DEREF_NUM(num).value.integer);
	}
	assert(false);
	return jnull();
}

jvalue_ref jnumber_create (raw_buffer str)
{
	char *createdBuffer = NULL;
	jvalue_ref new_number;

	assert(str.m_str != NULL);
	assert(str.m_len > 0);

	CHECK_POINTER_RETURN_VALUE(str.m_str, jnull());
	CHECK_CONDITION_RETURN_VALUE(str.m_len <= 0, jnull(), "Invalid length parameter for numeric string %s", str.m_str);

	createdBuffer = (char *) calloc (str.m_len + NUM_TERM_NULL, sizeof(char));
	CHECK_ALLOC_RETURN_VALUE(createdBuffer, jnull());

	memcpy (createdBuffer, str.m_str, str.m_len);
	str.m_str = createdBuffer;
	new_number = jnumber_create_unsafe(str, free);
	if (jis_null(new_number))
		free(createdBuffer);

	return new_number;
}

jvalue_ref jnumber_create_unsafe (raw_buffer str, jdeallocator strFree)
{
	jvalue_ref new_number;

	assert(str.m_str != NULL);
	assert(str.m_len > 0);

	CHECK_POINTER_RETURN_VALUE(str.m_str, jnull());
	CHECK_CONDITION_RETURN_VALUE(str.m_len == 0, jnull(), "Invalid length parameter for numeric string %s", str.m_str);

	new_number = jvalue_create (JV_NUM);
	CHECK_ALLOC_RETURN_NULL(new_number);

	DEREF_NUM(new_number).m_type = NUM_RAW;
	DEREF_NUM(new_number).value.raw = str;
	DEREF_NUM(new_number).m_rawDealloc = strFree;

	return new_number;
}

jvalue_ref jnumber_create_f64 (double number)
{
	jvalue_ref new_number;

	CHECK_CONDITION_RETURN_VALUE(isnan(number), jnull(), "NaN has no representation in JSON");
	CHECK_CONDITION_RETURN_VALUE(isinf(number), jnull(), "Infinity has no representation in JSON");

	new_number = jvalue_create (JV_NUM);
	CHECK_ALLOC_RETURN_NULL(new_number);

	DEREF_NUM(new_number).m_type = NUM_FLOAT;
	DEREF_NUM(new_number).value.floating = number;

	return new_number;
}

jvalue_ref jnumber_create_i32 (int32_t number)
{
	return jnumber_create_i64 (number);
}

jvalue_ref jnumber_create_i64 (int64_t number)
{
	jvalue_ref new_number;

	new_number = jvalue_create (JV_NUM);
	CHECK_ALLOC_RETURN_NULL(new_number);

	DEREF_NUM(new_number).m_type = NUM_INT;
	DEREF_NUM(new_number).value.integer = number;

	return new_number;
}

jvalue_ref jnumber_create_converted(raw_buffer raw)
{
	jvalue_ref new_number;

	new_number = jvalue_create(JV_NUM);
	CHECK_ALLOC_RETURN_NULL(new_number);

	if (CONV_OK != jstr_to_i64(&raw, &DEREF_NUM(new_number).value.integer)) {
		DEREF_NUM(new_number).m_error = jstr_to_double(&raw, &DEREF_NUM(new_number).value.floating);
		if (DEREF_NUM(new_number).m_error != CONV_OK) {
			PJ_LOG_ERR("Number '%.*s' doesn't convert perfectly to a native type",
					(int)raw.m_len, raw.m_str);
			assert(false);
		}
	}

	return new_number;
}

int jnumber_compare(jvalue_ref number, jvalue_ref toCompare)
{
	SANITY_CHECK_POINTER(number);
	SANITY_CHECK_POINTER(toCompare);

	assert(jis_number(number));
	assert(jis_number(toCompare));

	switch (DEREF_NUM(toCompare).m_type) {
		case NUM_FLOAT:
			return jnumber_compare_f64(number, DEREF_NUM(toCompare).value.floating);
		case NUM_INT:
			return jnumber_compare_i64(number, DEREF_NUM(toCompare).value.integer);
		case NUM_RAW:
		{
			int64_t asInt;
			double asFloat;
			if (CONV_OK == jstr_to_i64(&DEREF_NUM(toCompare).value.raw, &asInt))
				return jnumber_compare_i64(number, asInt);
			if (CONV_OK != jstr_to_double(&DEREF_NUM(toCompare).value.raw, &asFloat)) {
				PJ_LOG_ERR("Comparing against something that can't be represented as a float: '%.*s'",
						(int)DEREF_NUM(toCompare).value.raw.m_len, DEREF_NUM(toCompare).value.raw.m_str);
			}
			return jnumber_compare_f64(number, asFloat);
		}
		default:
			PJ_LOG_ERR("Unknown type for toCompare - corruption?");
			assert(false);
			return -50;
	}
}

int jnumber_compare_i64(jvalue_ref number, int64_t toCompare)
{
	SANITY_CHECK_POINTER(number);
	assert(jis_number(number));

	switch (DEREF_NUM(number).m_type) {
		case NUM_FLOAT:
			return DEREF_NUM(number).value.floating > toCompare ? 1 :
				(DEREF_NUM(number).value.floating < toCompare ? -1 : 0);
		case NUM_INT:
			return DEREF_NUM(number).value.integer > toCompare ? 1 :
				(DEREF_NUM(number).value.integer < toCompare ? -1 : 0);
		case NUM_RAW:
		{
			int64_t asInt;
			if (CONV_OK == jstr_to_i64(&DEREF_NUM(number).value.raw, &asInt)) {
				return asInt > toCompare ? 1 :
						(asInt < toCompare ? -1 : 0);
			}
			double asFloat;
			if (CONV_OK != jstr_to_double(&DEREF_NUM(number).value.raw, &asFloat)) {
				PJ_LOG_ERR("Comparing '%"PRId64 "' against something that can't be represented as a float: '%.*s'",
						toCompare, (int)DEREF_NUM(number).value.raw.m_len, DEREF_NUM(number).value.raw.m_str);
			}
			return asFloat > toCompare ? 1 : (asFloat < toCompare ? -1 : 0);
		}
		default:
			PJ_LOG_ERR("Unknown type - corruption?");
			assert(false);
			return -50;
	}
}

int jnumber_compare_f64(jvalue_ref number, double toCompare)
{
	SANITY_CHECK_POINTER(number);
	assert(jis_number(number));

	switch (DEREF_NUM(number).m_type) {
		case NUM_FLOAT:
			return DEREF_NUM(number).value.floating > toCompare ? 1 :
				(DEREF_NUM(number).value.floating < toCompare ? -1 : 0);
		case NUM_INT:
			return DEREF_NUM(number).value.integer > toCompare ? 1 :
				(DEREF_NUM(number).value.integer < toCompare ? -1 : 0);
		case NUM_RAW:
		{
			int64_t asInt;
			if (CONV_OK == jstr_to_i64(&DEREF_NUM(number).value.raw, &asInt)) {
				return asInt > toCompare ? 1 :
						(asInt < toCompare ? -1 : 0);
			}
			double asFloat;
			if (CONV_OK != jstr_to_double(&DEREF_NUM(number).value.raw, &asFloat)) {
				PJ_LOG_ERR("Comparing '%lf' against something that can't be represented as a float: '%.*s'",
						toCompare, (int)DEREF_NUM(number).value.raw.m_len, DEREF_NUM(number).value.raw.m_str);
			}
			return asFloat > toCompare ? 1 : (asFloat < toCompare ? -1 : 0);
		}
		default:
			PJ_LOG_ERR("Unknown type - corruption?");
			assert(false);
			return -50;
	}
}

bool jnumber_has_error (jvalue_ref number)
{
	return DEREF_NUM(number).m_error != CONV_OK;
}

bool jis_number (jvalue_ref num)
{
	SANITY_CHECK_POINTER(num);
	CHECK_POINTER_RETURN_VALUE(num, false);
	assert(s_inGdb || num->m_refCnt > 0);

	return num->m_type == JV_NUM;
}

int64_t jnumber_deref_i64(jvalue_ref num)
{
	int64_t result;
	ConversionResultFlags fail;
	assert(jnumber_get_i64(num, &result) == CONV_OK);
	if (CONV_OK != (fail = jnumber_get_i64(num, &result))) {
		PJ_LOG_WARN("Converting json value to a 64-bit integer but ignoring the conversion error: %d", fail);
	}
	return result;
}


raw_buffer jnumber_deref_raw(jvalue_ref num)
{
	// initialized to 0 just to get around compiler warning for
	// now - it is really up to the caller to ensure they do not
	// call this on something that is not a raw number.
	raw_buffer result = { 0 };
	assert(jnumber_get_raw(num, &result) == CONV_OK);
	jnumber_get_raw(num, &result);
	return result;
}

ConversionResultFlags jnumber_get_i32 (jvalue_ref num, int32_t *number)
{
	SANITY_CHECK_POINTER(num);

	assert(num != NULL);
	assert(number != NULL);
	assert(jis_number(num));

	CHECK_POINTER_RETURN_VALUE(num, CONV_BAD_ARGS);
	CHECK_POINTER_RETURN_VALUE(number, CONV_BAD_ARGS);
	CHECK_CONDITION_RETURN_VALUE(!jis_number(num), CONV_BAD_ARGS, "Trying to access %d as a number", num->m_type);

	switch (DEREF_NUM(num).m_type) {
		case NUM_FLOAT:
			return jdouble_to_i32 (DEREF_NUM(num).value.floating, number) | DEREF_NUM(num).m_error;
		case NUM_INT:
			return ji64_to_i32 (DEREF_NUM(num).value.integer, number) | DEREF_NUM(num).m_error;
		case NUM_RAW:
			assert(DEREF_NUM(num).value.raw.m_str != NULL);
			assert(DEREF_NUM(num).value.raw.m_len > 0);
			return jstr_to_i32 (&DEREF_NUM(num).value.raw, number) | DEREF_NUM(num).m_error;
		default:
			PJ_LOG_ERR("internal error - numeric type is unrecognized (%d)", (int)DEREF_NUM(num).m_type);
			assert(false);
			return CONV_GENERIC_ERROR;
	}
}

ConversionResultFlags jnumber_get_i64 (jvalue_ref num, int64_t *number)
{
	SANITY_CHECK_POINTER(num);

	assert(num != NULL);
	assert(number != NULL);
	assert(jis_number(num));

	CHECK_POINTER_RETURN_VALUE(num, CONV_BAD_ARGS);
	CHECK_POINTER_RETURN_VALUE(number, CONV_BAD_ARGS);
	CHECK_CONDITION_RETURN_VALUE(!jis_number(num), CONV_BAD_ARGS, "Trying to access %d as a number", num->m_type);

	switch (DEREF_NUM(num).m_type) {
		case NUM_FLOAT:
			return jdouble_to_i64 (DEREF_NUM(num).value.floating, number) | DEREF_NUM(num).m_error;
		case NUM_INT:
			*number = DEREF_NUM(num).value.integer;
			return DEREF_NUM(num).m_error;
		case NUM_RAW:
			assert(DEREF_NUM(num).value.raw.m_str != NULL);
			assert(DEREF_NUM(num).value.raw.m_len > 0);
			return jstr_to_i64 (&DEREF_NUM(num).value.raw, number) | DEREF_NUM(num).m_error;
		default:
			PJ_LOG_ERR("internal error - numeric type is unrecognized (%d)", (int)DEREF_NUM(num).m_type);
			assert(false);
			return CONV_GENERIC_ERROR;
	}
}

ConversionResultFlags jnumber_get_f64 (jvalue_ref num, double *number)
{
	SANITY_CHECK_POINTER(num);

	assert(num != NULL);
	assert(number != NULL);
	assert(jis_number(num));

	CHECK_POINTER_RETURN_VALUE(num, CONV_BAD_ARGS);
	CHECK_POINTER_RETURN_VALUE(number, CONV_BAD_ARGS);
	CHECK_CONDITION_RETURN_VALUE(!jis_number(num), CONV_BAD_ARGS, "Trying to access %d as a number", num->m_type);

	switch (DEREF_NUM(num).m_type) {
		case NUM_FLOAT:
			*number = DEREF_NUM(num).value.floating;
			return DEREF_NUM(num).m_error;
		case NUM_INT:
			return ji64_to_double (DEREF_NUM(num).value.integer, number) | DEREF_NUM(num).m_error;
		case NUM_RAW:
			assert(DEREF_NUM(num).value.raw.m_str != NULL);
			assert(DEREF_NUM(num).value.raw.m_len > 0);
			return jstr_to_double (&DEREF_NUM(num).value.raw, number) | DEREF_NUM(num).m_error;
		default:
			PJ_LOG_ERR("internal error - numeric type is unrecognized (%d)", (int)DEREF_NUM(num).m_type);
			assert(false);
			return CONV_GENERIC_ERROR;
	}
}

ConversionResultFlags jnumber_get_raw (jvalue_ref num, raw_buffer *result)
{
	SANITY_CHECK_POINTER(num);

	assert(num != NULL);
	assert(result != NULL);
	assert(jis_number(num));

	CHECK_POINTER_RETURN_VALUE(num, CONV_BAD_ARGS);
	CHECK_POINTER_RETURN_VALUE(result, CONV_BAD_ARGS);
	CHECK_CONDITION_RETURN_VALUE(!jis_number(num), CONV_BAD_ARGS, "Trying to access %d as a number", num->m_type);

	switch (DEREF_NUM(num).m_type) {
		case NUM_FLOAT:
		case NUM_INT:
			return CONV_NOT_A_RAW_NUM;
		case NUM_RAW:
			assert(DEREF_NUM(num).value.raw.m_str != NULL);
			assert(DEREF_NUM(num).value.raw.m_len > 0);
			*result = DEREF_NUM(num).value.raw;
			return CONV_OK;
		default:
			PJ_LOG_ERR("internal error - numeric type is unrecognized (%d)", (int)DEREF_NUM(num).m_type);
			assert(false);
			return CONV_GENERIC_ERROR;
	}
}

#undef DEREF_NUM
/*** JSON Boolean operations ***/
#define DEREF_BOOL(ref) ((ref)->value.val_bool)

static inline void j_destroy_boolean (jvalue_ref boolean)
{
}

static inline void jboolean_to_string_append (jvalue_ref jref, JStreamRef generating)
{
	generating->boolean (generating, DEREF_BOOL(jref).value);
}

bool jis_boolean (jvalue_ref jval)
{
	SANITY_CHECK_POINTER(jval);
	assert(s_inGdb || jval->m_refCnt > 0);
	return jval->m_type == JV_BOOL;
}

jvalue_ref jboolean_create (bool value)
{
	jvalue_ref new_bool = jvalue_create (JV_BOOL);
	if (LIKELY(new_bool != NULL)) {
		DEREF_BOOL(new_bool).value = value;
	}
	return new_bool;
}

bool jboolean_deref (jvalue_ref boolean)
{
	bool result;
	assert (jis_null(boolean) || CONV_OK == jboolean_get(boolean, &result));
	jboolean_get (boolean, &result);
	return result;
}

/**
 * Retrieve the native boolean representation of this reference.
 *
 * The following equivalencies are made for the various JSON types & bool:
 * NUMBERS: 0, NaN = false, everything else = true
 * STRINGS: empty = false, non-empty = true
 * NULL: false
 * ARRAY: true
 * OBJECT: true
 * @param val The reference to the JSON value
 * @param value Where to write the boolean value to.
 * @return CONV_OK if val represents a JSON boolean type, otherwise CONV_NOT_A_BOOLEAN.
 */
ConversionResultFlags jboolean_get (jvalue_ref val, bool *value)
{
	SANITY_CHECK_POINTER(val);

	if (value) *value = false;

	CHECK_POINTER_MSG_RETURN_VALUE(val, CONV_NOT_A_BOOLEAN, "Attempting to use a C NULL as a JSON value reference");
	CHECK_POINTER_MSG_RETURN_VALUE(value, (jis_boolean(val) ? CONV_OK : CONV_NOT_A_BOOLEAN), "Non-recommended API use - value is not pointing to a valid boolean");
	assert(val->m_refCnt > 0);

	switch (val->m_type) {
		case JV_BOOL:
			if (value) *value = DEREF_BOOL(val).value;
			return CONV_OK;

		case JV_NULL:
			PJ_LOG_INFO("Attempting to convert NULL to boolean");
			if (value) *value = false;
			break;
		case JV_OBJECT:
			PJ_LOG_WARN("Attempting to convert an object to a boolean - always true");
			if (value) *value = true;
			break;
		case JV_ARRAY:
			PJ_LOG_WARN("Attempting to convert an array to a boolean - always true");
			if (value) *value = true;
			break;
		case JV_STR:
			PJ_LOG_WARN("Attempt to convert a string to a boolean - testing if string is empty");
			if (value) *value = jstring_size (val) != 0;
			break;
		case JV_NUM:
		{
			double result;
			ConversionResultFlags conv_result;
			PJ_LOG_WARN("Attempting to convert a number to a boolean - testing if number is 0");
			conv_result = jnumber_get_f64 (val, &result);
			if (value) *value = (conv_result == CONV_OK && result != 0);
			break;
		}
	}

	return CONV_NOT_A_BOOLEAN;
}

