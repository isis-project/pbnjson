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

#ifndef SCHEMA_KEYS_H_
#define SCHEMA_KEYS_H_

/** Schema Keys **/
#define SK_TYPE				"type"
#define SK_PROPS			"properties"
#define SK_MORE_PROPS		"additionalProperties"
#define SK_EXTENDS			"extends"
#define SK_REF				"$ref"
#define SK_SELF				"_self"
#define SK_ID				"id"
#define SK_PARENT_STATE 	"parent_"
#define SK_CHILD_STATES		"children_"
#define SK_CHILD_STATES_OK  "schemaPossible_"
#define SK_KEYS_SEEN		"keysSeen_"
#define SK_SCHEMA			"schema_"
#define SK_DISALLLOWED		"disallowed"
#define SK_ENUM				"enum"
#define SK_MAX_LEN			"maxLength"
#define SK_MIN_LEN			"minLength"
#define SK_REGEXP			"pattern"
#define SK_OPTIONAL			"optional"
#define SK_REQUIRED			"required"
#define SK_DEFAULT			"default"
#define SK_ITEMS			"items"
#define SK_MIN_VALUE		"minimum"
#define SK_MAX_VALUE		"maximum"
#define SK_MIN_ITEMS		"minItems"
#define SK_MAX_ITEMS		"maxItems"
#define SK_DESCRIPTION		"description"

#endif /* SCHEMA_KEYS_H_ */
