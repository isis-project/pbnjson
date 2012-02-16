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

#ifndef JSCHEMA_TYPES_INTERNAL_H_
#define JSCHEMA_TYPES_INTERNAL_H_

#include "jparse_types.h"
#include "jgen_types.h"

typedef enum {
	ST_STR = 1,
	ST_INT = 2,
	ST_NUM = 2 | 4,
	ST_BOOL = 8,
	ST_OBJ = 16,
	ST_ARR = 32,
	ST_NULL = 64,
	ST_ANY = (ST_STR | ST_NUM | ST_INT | ST_BOOL | ST_OBJ | ST_ARR | ST_NULL),
	ST_ERR = ST_ANY + 1, /// this works only because ST_ANY is a bitwise combination of all the others. +1 means ST_ERR & anything other than ST_ERR is 0.
} SchemaType;

typedef unsigned int SchemaTypeBitField;

/**
 * This structure & any nestested structures (included jvalues)
 * cannot be changed while parsing except to resolve external references
 */
typedef struct jschema {
	int m_refCnt;
	jvalue_ref m_validation; /// a LIFO array of schemas to use when our current schema is missing a key. each element is a DOM of the schema.  [0] is the top level schema that was parsed
#if ALLOW_LOCAL_REFS
	jvalue_ref m_top; /// the schema that $ would resolve to - the outer-most object in this schema
#endif
#if 0
	bool m_negative; /// if this schema represents a match for invalid input
#endif
	const char *m_backingMMap;
	size_t m_backingMMapSize;
} SchemaWrapper, * SchemaWrapperRef;

typedef struct SchemaState {
	SchemaTypeBitField m_allowedTypes; /// bit-field lookup summary of the high-level types allowed in this position. "minimized" to the concrete type when input is provided
	SchemaTypeBitField m_disallowedTypes; /// bit-field lookup summary of the high-level types not allowed in this position.
	SchemaWrapperRef m_schema; /// the schema at the current spot to validate against
	struct SchemaState *m_parent;	/// the state to pop up to when this state has matched/been invalidated
	jvalue_ref m_seenKeys; /// an array of strings representing the keys that were seen for this object.  Implies m_types is ST_OBJ
	size_t m_numItems; /// a counter of the number of elements in this array.  Implies m_types is ST_ARR
	bool m_arrayOpened; /// whether or not the array schema got the opening bracket (to differentiate between when the array opens and a nested array)
} * SchemaStateRef;

typedef struct SchemaResolution {
	JSchemaResolverRef m_resolver;
	JErrorCallbacksRef m_errorHandler;
} * SchemaResolutionRef;

#if !defined(NDEBUG) && !defined(TRACK_SCHEMA_PARSING)
#define TRACK_SCHEMA_PARSING 1
#elif !defined(TRACK_SCHEMA_PARSING)
#define TRACK_SCHEMA_PARSING 0
#endif

typedef struct ValidationState {
	SchemaStateRef m_state;
	struct SchemaResolution m_resolutionHandlers;
#if TRACK_SCHEMA_PARSING
	bool m_parsingSchema;
	#define START_TRACKING_SCHEMA(validationState) (validationState)->m_parsingSchema = true
	#define END_TRACKING_SCHEMA(validationState) (validationState)->m_parsingSchema = false
#else
	#define START_TRACKING_SCHEMA(validationState) do {} while (0)
	#define END_TRACKING_SCHEMA(validationState) do {} while (0)
#endif
} * ValidationStateRef;

typedef void (*GeneratorInjector)(jvalue_ref valueToInject);

typedef enum InjectorType {
	PARSING,
	GENERATING,
} InjectorType;

typedef struct SAXInjector {
	union {
		JSAXContextRef m_parsing;
		JStreamRef m_generating;
	} m_injector;
	InjectorType m_type;	/// the type used to generate
} SAXInjector;

#endif /* JSCHEMA_TYPES_INTERNAL_H_ */
