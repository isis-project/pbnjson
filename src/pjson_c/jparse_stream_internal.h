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

#ifndef JPARSE_STREAM_INTERNAL_H_
#define JPARSE_STREAM_INTERNAL_H_

#include <jtypes.h>
#include <jcallbacks.h>
#include <jparse_stream.h>

PJSON_LOCAL jvalue_ref jdom_parse_ex(raw_buffer input, JDOMOptimizationFlags optimizationMode, JSchemaInfoRef schemaInfo, bool allowComments);

/**
 * This should be safe (in terms of not breaking JSON syntax) since only the schema is using it
 * and it can only have well-formed objects.
 */
PJSON_LOCAL bool jsax_parse_inject(JSAXContextRef ctxt, jvalue_ref key, jvalue_ref value);

#endif /* JPARSE_STREAM_INTERNAL_H_ */
