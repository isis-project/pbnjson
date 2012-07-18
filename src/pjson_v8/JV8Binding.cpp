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
 * JV8Binding.cpp
 *
 *  Created on: Feb 19, 2010
 */

#include <pbnjson_v8.hpp>

#include <cassert>
#include <pbnjson/v8/JV8Parser.h>
#include <pbnjson.h>

using namespace v8;

namespace pbnjson {

	Handle<Value> parseString(const Arguments& args)
	{
		Handle<Value> parsed;

		{
#if 0
			HandleScope scope;
			Persistent<Context> m_context = Context::New();
			assert(!m_context.IsEmpty());
			m_context->Enter();
#endif

			Handle<Function> keyValueConverter = Local<Function>::Cast(args[1]);

			{
				JV8Parser parser(keyValueConverter);
				if (!parser.parse(args[0]->ToString(), JSchemaFragment("{}"))) {
					Local<String> errMsg = String::Concat(args[0]->ToString(), String::New("is not valid JSON"));
					ThrowException(Exception::SyntaxError(errMsg));
				}

				parsed = parser.parsed();
				assert(parsed->IsObject());
			}

#if 0
			m_context->Exit();
			m_context.Dispose();
#endif
		}

		return parsed;
	}

	void InitJSON2(Handle<v8::ObjectTemplate>& globalTemplate)
	{
		Handle<ObjectTemplate> json2Template = ObjectTemplate::New();
		json2Template->Set("parse", FunctionTemplate::New(parseString));
		globalTemplate->Set("JSON2", json2Template);
	}
}
