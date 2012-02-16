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

#include <pbnjson_v8.hpp>
#include <pbnjson/c/compiler/builtins.h>

using namespace v8;

namespace pbnjson {
	JV8LazyString::JV8LazyString(const char *str, size_t length)
		: m_cstr(str), m_len(length), m_utf16(NULL), m_utf16len(0)
	{
	}

	JV8LazyString::JV8LazyString(const std::string& str)
		: m_str(str), m_utf16(NULL), m_utf16len(0)
	{
		m_cstr = m_str.c_str();
		m_len = m_str.size();
	}

	JV8LazyString::~JV8LazyString()
	{
		delete [] m_utf16;
	}

	const uint16_t* JV8LazyString::data() const
	{
		if (UNLIKELY(m_utf16 == NULL)) {
			HandleScope scope;
			Handle<String> converted = String::New(m_cstr, m_len);
			m_utf16len = converted->Length();
			m_utf16 = new uint16_t[m_utf16len];
		}
		return m_utf16;
	}

	size_t JV8LazyString::length() const
	{
		if (UNLIKELY(m_utf16 == NULL)) {
			HandleScope scope;
			Handle<String> converted = String::New(m_cstr, m_len);
			m_utf16len = converted->Length();
			m_utf16 = new uint16_t[m_utf16len];
		}
		return m_utf16len;
	}
}
