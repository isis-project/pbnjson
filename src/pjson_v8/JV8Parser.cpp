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
 * JV8Parser.cpp
 *
 *  Created on: Feb 19, 2010
 */

#include <pbnjson/v8/JV8Parser.h>

#include <cassert>
#include <limits>

using namespace std;
using namespace v8;

namespace pbnjson {
	JV8Parser::JV8Parser(Handle<v8::Function> converter) :
		JParser(NULL), m_keyValConverter(converter)
	{
		m_parentKey = String::New("pbnjson::parent");
	}

	JV8Parser::~JV8Parser()
	{
	}

	bool JV8Parser::parse(Handle<v8::String> input, const JSchema &schema, JErrorHandler *errors)
	{
		int len = input->Utf8Length();
		char *utf8str = new char[len];
		input->WriteUtf8(utf8str, len);
		string utf8(utf8str, len);
		delete [] utf8str;

		return JParser::parse(utf8, schema, errors);
	}

	Handle<v8::Value> JV8Parser::parsed()
	{
		return m_parsed;
	}

	static void appendArray(Handle<Value> array, Handle<Value> value)
	{
		Handle<v8::Array> _array = Handle<v8::Array>::Cast(array);
		_array->Set(v8::Integer::NewFromUnsigned(_array->Length()), value);
	}

	static void appendKeyVal(Handle<Value> obj, Handle<Value> key, Handle<Value> value)
	{
		Handle<v8::Array> _object = Handle<v8::Array>::Cast(obj);
		_object->Set(key, value);
	}

	bool JV8Parser::jsonObjectOpen()
	{
		Handle<v8::Object> newParent = v8::Object::New();

		if (!m_parents.empty()) {
			if (m_parents.top()->IsArray()) {
				appendArray(m_parents.top(), newParent);
			} else {
				assert(!m_key.IsEmpty());
				assert(m_key->IsString());
				assert(m_parents.top()->IsObject());

				appendKeyVal(m_parents.top(), m_key, newParent);
			}
		}

		m_parents.push(newParent);
		m_key.Clear();

		return true;
	}

	bool JV8Parser::jsonObjectKey(const string& key)
	{
		assert(m_parents.top()->IsObject());
		m_key = String::NewExternal(new JV8LazyString(key));
		return true;
	}

	bool JV8Parser::jsonObjectClose()
	{
		m_parsed = m_parents.top();
		assert(m_parsed->IsObject());
		m_parents.pop();

		return true;
	}

	bool JV8Parser::jsonArrayOpen()
	{
		Handle<v8::Array> newParent = v8::Array::New(0);

		if (!m_parents.empty()) {
			if (m_parents.top()->IsArray()) {
				appendArray(m_parents.top(), newParent);
			} else {
				assert(!m_key.IsEmpty());
				assert(m_key->IsString());
				assert(m_parents.top()->IsObject());

				appendKeyVal(m_parents.top(), m_key, newParent);
			}
		}

		m_parents.push(newParent);
		m_key.Clear();
		return true;
	}

	bool JV8Parser::jsonArrayClose()
	{
		m_parsed = m_parents.top();
		assert(m_parsed->IsArray());
		m_parents.pop();
		return true;
	}

	bool JV8Parser::jsonString(const string& s)
	{
		if (m_parents.empty())
			return false;

		Handle<String> str = String::NewExternal(new JV8LazyString(s));

		if (m_parents.top()->IsArray()) {
			appendArray(m_parents.top(), str);
		} else {
			appendKeyVal(m_parents.top(), m_key, String::New(s.c_str(), s.size()));
		}
		return true;
	}

	bool JV8Parser::jsonNumber(int64_t number)
	{
		Handle<Number> num;

		if (m_parents.empty())
			return false;

		if (number > 0) {
			if (number > numeric_limits<uint32_t>::max())
				return false;
			num = Integer::New((int32_t)number);
		} else if (number < numeric_limits<int32_t>::min()) {
			return false;
		} else {
			num = Integer::New((int32_t)number);
		}

		if (m_parents.top()->IsArray()) {
			appendArray(m_parents.top(), num);
		} else {
			assert(!m_key.IsEmpty());
			assert(m_parents.top()->IsObject());
			appendKeyVal(m_parents.top(), m_key, num);
		}
		return true;
	}

	bool JV8Parser::jsonNumber(double &number, ConversionResultFlags asFloat)
	{
		if (m_parents.empty())
			return false;

		Handle<Value> num = Number::New(number);

		if (m_parents.top()->IsArray()) {
			appendArray(m_parents.top(), num);
		} else {
			assert(!m_key.IsEmpty());
			assert(m_parents.top()->IsObject());
			appendKeyVal(m_parents.top(), m_key, num);
		}

		return true;
	}

	bool JV8Parser::jsonBoolean(bool truth)
	{
		if (m_parents.empty())
			return false;

		Handle<Boolean> boolean = Boolean::New(truth);

		if (m_parents.top()->IsArray()) {
			appendArray(m_parents.top(), boolean);
		} else {
			assert(!m_key.IsEmpty());
			assert(m_parents.top()->IsObject());
			appendKeyVal(m_parents.top(), m_key, boolean);
		}

		return true;
	}

	bool JV8Parser::jsonNull()
	{
		if (m_parents.empty())
			return false;

		Handle<Value> null = Null();

		if (m_parents.top()->IsArray()) {
			appendArray(m_parents.top(), null);
		} else {
			assert(!m_key.IsEmpty());
			assert(m_parents.top()->IsObject());
			appendKeyVal(m_parents.top(), m_key, null);
		}

		return true;
	}
}
