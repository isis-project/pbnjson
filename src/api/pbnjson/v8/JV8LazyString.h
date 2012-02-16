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
 * JV8LazyString.h
 *
 *  Created on: Feb 23, 2010
 */

#ifndef JV8LAZYSTRING_H_
#define JV8LAZYSTRING_H_

#include <pbnjson_v8.hpp>
#include <string>
#include <stdint.h>

namespace pbnjson {
	class JV8LazyString : public v8::String::ExternalStringResource {
	public:
		JV8LazyString(const char *s, size_t l);
		JV8LazyString(const std::string& input);
		~JV8LazyString();

		const uint16_t* data() const;
		size_t length() const;

	private:
		std::string m_str;
		const char *m_cstr;
		size_t m_len;

		mutable uint16_t *m_utf16;
		mutable size_t m_utf16len;
	};
}

#endif /* JV8LAZYSTRING_H_ */
