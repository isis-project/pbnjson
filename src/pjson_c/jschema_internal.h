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

#ifndef JSCHEMA_INTERNAL_H_
#define JSCHEMA_INTERNAL_H_

#include <japi.h>

#include <jschema_types.h>
#include "jschema_types_internal.h"
#include <jcallbacks.h>

/*
 * Ownership is transferred to the state returned.  If you want to share a schema between multiple states, make sure you
 * retain a reference before-hand.
 */
PJSON_LOCAL ValidationStateRef jschema_init(JSchemaInfoRef schemaToUse) NON_NULL(1);
PJSON_LOCAL void jschema_state_release(ValidationStateRef *state) NON_NULL(1);

PJSON_LOCAL bool jschema_isvalid(ValidationStateRef parseState);

PJSON_LOCAL bool jschema_obj(JSAXContextRef sax, ValidationStateRef parseState);
PJSON_LOCAL bool jschema_obj_end(JSAXContextRef sax, ValidationStateRef parseState);
PJSON_LOCAL bool jschema_arr(JSAXContextRef sax, ValidationStateRef parseState);
PJSON_LOCAL bool jschema_arr_end(JSAXContextRef sax, ValidationStateRef parseState);
PJSON_LOCAL bool jschema_key(JSAXContextRef sax, ValidationStateRef parseState, raw_buffer objKey);
PJSON_LOCAL bool jschema_str(JSAXContextRef sax, ValidationStateRef parseState, raw_buffer str);
PJSON_LOCAL bool jschema_num(JSAXContextRef sax, ValidationStateRef parseState, raw_buffer num);
PJSON_LOCAL bool jschema_bool(JSAXContextRef sax, ValidationStateRef parseState, bool truth);
PJSON_LOCAL bool jschema_null(JSAXContextRef sax, ValidationStateRef parseState);

/**
 * @return True if this schema is an invalid schema, false otherwise
 */
PJSON_LOCAL bool jis_null_schema(SchemaWrapperRef schema);

/**
 * @return True if this schema is equivalent to {}
 */
PJSON_LOCAL bool jis_empty_schema(SchemaWrapperRef schema);

PJSON_LOCAL JSchemaResolverRef jget_garbage_resolver();

#endif /* JSCHEMA_INTERNAL_H_ */
