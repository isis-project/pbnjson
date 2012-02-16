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

/*
 * JGenerator.cpp
 *
 *  Created on: Sep 24, 2009
 */

#include <JGenerator.h>
#include <pbnjson.h>

#include <JResolver.h>

namespace pbnjson {

JGenerator::JGenerator(JResolver *resolver)
	: m_resolver(resolver)
{
}

JGenerator::~JGenerator() {
}

bool JGenerator::toString(const JValue &obj, const JSchema& schema, std::string &asStr)
{
	const char *str = jvalue_tostring(obj.peekRaw(), schema.peek());
	if (UNLIKELY(str == NULL)) {
		asStr = "";
		return false;
	}
	asStr = str;
	return true;
}

std::string JGenerator::serialize(const JValue &val, const JSchema &schema, JResolver *resolver)
{
	JGenerator serializer(resolver);
	std::string serialized;
	if (!serializer.toString(val, schema, serialized)) {
		serialized = "";
	}
	return serialized;
}

}
