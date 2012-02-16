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

#include <JSchema.h>
#include <JSchemaFragment.h>

#include <pbnjson.h>

namespace pbnjson {

#ifndef SK_DISALLOWED
#define SK_DISALLOWED "disallowed"
#endif

JSchema JSchema::NullSchema()
{
	static JSchemaFragment NO_VALID_INPUT_SCHEMA(
		"{\"" SK_DISALLOWED "\":\"any\"}"
	);
	return NO_VALID_INPUT_SCHEMA;
}

JSchema::JSchema(const JSchema& other)
	: m_resource(other.m_resource)
{
	if (m_resource)
		m_resource->ref();
}

JSchema::~JSchema()
{
	if (m_resource && m_resource->unref())
		delete m_resource;
}

JSchema& JSchema::operator=(const JSchema& other)
{
	if (m_resource != other.m_resource) {
		if (m_resource && m_resource->unref()) delete m_resource;
		if (other.m_resource) other.m_resource->ref();
		m_resource = other.m_resource;
	}
	return *this;
}

JSchema::JSchema(Resource *resource)
	: m_resource(resource)
{
}

bool JSchema::isInitialized() const
{
	return m_resource != NULL;
}

JSchema::Resource::Resource()
	: m_refCnt(1), m_data(NULL), m_schema(NULL)
{
}

JSchema::Resource::Resource(jschema_ref schema, SchemaOwnership ownership)
	: m_refCnt(1), m_data(NULL), m_schema(schema)
{
	if (ownership == CopySchema)
		m_schema = jschema_copy(m_schema);
}

JSchema::Resource::Resource(void *data, jschema_ref schema, SchemaOwnership ownership)
	: m_refCnt(1), m_data(data), m_schema(schema)
{
	if (ownership == CopySchema)
		m_schema = jschema_copy(m_schema);
}

JSchema::Resource::~Resource()
{
	jschema_release(&m_schema);
}

void* JSchema::Resource::data() const
{
	return m_data;
}

jschema_ref JSchema::Resource::schema()
{
	return m_schema;
}

void JSchema::Resource::ref()
{
	m_refCnt++;
}

bool JSchema::Resource::unref()
{
	if (--m_refCnt == 0) {
		return true;
	}
	return false;
}

}

