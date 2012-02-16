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
 * JV8Parser.h
 *
 *  Created on: Feb 19, 2010
 */

#ifndef JV8PARSER_H_
#define JV8PARSER_H_

#include <pbnjson_v8.hpp>
#include <assert.h>
#include <stack>

namespace pbnjson {
	class PJSONCXX_API JV8Parser : public JParser {
	private:
		v8::Handle<v8::Value> m_parsed;
		v8::Handle<v8::Function> m_keyValConverter;
		v8::Handle<v8::Value> m_key;
		v8::Handle<v8::String> m_parentKey;
		std::stack<v8::Handle<v8::Value> > m_parents;

	public:
		JV8Parser(v8::Handle<v8::Function> converter);
		~JV8Parser();

		/**
		 * Parse the input using the given schema.
		 *
		 * @param input The JSON string to parse.  Must be a JSON object or an array.  Behaviour is undefined
		 *              if it isn't.  This is part of the JSON spec.
		 * @param schema The JSON schema to use when parsing.
		 * @param errors The error handler to use if you want more detailed information if parsing failed.
		 * @return True if we got validly formed JSON input that was accepted by the schema, false otherwise.
		 *
		 * @see JSchema
		 * @see JSchemaFile
		 * @see JErrorHandler
		 */
		bool parse(v8::Handle<v8::String> input, const JSchema &schema, JErrorHandler *errors = NULL);

		/**
		 * Returns the parsed v8 object
		 */
		v8::Handle<v8::Value> parsed();

	protected:

		bool jsonObjectOpen();
		bool jsonObjectKey(const std::string& key);
		bool jsonObjectClose();

		bool jsonArrayOpen();
		bool jsonArrayClose();

		bool jsonString(const std::string& s);
		bool jsonNumber(const std::string& n) { return false; }
		bool jsonNumber(int64_t number);
		bool jsonNumber(double &number, ConversionResultFlags asFloat);
		bool jsonBoolean(bool truth);
		bool jsonNull();

		NumberType conversionToUse() const
		{
			return JNUM_CONV_NATIVE;
		}
	};
}

#endif /* JV8PARSER_H_ */
