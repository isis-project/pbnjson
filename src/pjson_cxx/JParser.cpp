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

#include <JParser.h>
#include <JErrorHandler.h>
#include <pbnjson.h>
#include <JResolver.h>
#include "liblog.h"

namespace pbnjson {

JSchemaResolutionResult sax_schema_resolver(JSchemaResolverRef resolver, jschema_ref *resolvedSchema)
{
	JParser *__this = reinterpret_cast<JParser *>(resolver->m_userCtxt);
	return __this->resolve(resolver, resolvedSchema);
}

/**
 * Need this class to get around the member visibility restriction (& not having to declare all of the functions as
 * friends).
 */
class PJSONCXX_LOCAL SaxBounce {
public:
	static inline bool oo(JParser *p) { return p->jsonObjectOpen(); }
	static inline bool ok(JParser *p, const std::string& key) { return p->jsonObjectKey(key); }
	static inline bool oc(JParser *p) { return p->jsonObjectClose(); }
	static inline bool ao(JParser *p) { return p->jsonArrayOpen(); }
	static inline bool ac(JParser *p) { return p->jsonArrayClose(); }

	static inline bool s(JParser *p, const std::string& s) { return p->jsonString(s); }
	static inline bool n(JParser *p, const std::string& n) { return p->jsonNumber(n); }
	static inline bool n(JParser *p, int64_t n) { return p->jsonNumber(n); }
	static inline bool n(JParser *p, double n, ConversionResultFlags f) { return p->jsonNumber(n, f); }
	static inline bool b(JParser *p, bool v) { return p->jsonBoolean(v); }
	static inline bool N(JParser *p) { return p->jsonNull(); }
	static inline JParser::NumberType conversionToUse(JParser * const p) { return p->conversionToUse(); }

};

static int __obj_start(JSAXContextRef ctxt)
{
	return SaxBounce::oo(static_cast<JParser *>(jsax_getContext(ctxt)));
}

static int __obj_key(JSAXContextRef ctxt, const char *key, size_t len)
{
	return SaxBounce::ok(static_cast<JParser *>(jsax_getContext(ctxt)), std::string(key, len));
}

static int __obj_end(JSAXContextRef ctxt)
{
	return SaxBounce::oc(static_cast<JParser *>(jsax_getContext(ctxt)));
}

static int __arr_start(JSAXContextRef ctxt)
{
	return SaxBounce::ao(static_cast<JParser *>(jsax_getContext(ctxt)));
}

static int __arr_end(JSAXContextRef ctxt)
{
	return SaxBounce::ac(static_cast<JParser *>(jsax_getContext(ctxt)));
}

static int __string(JSAXContextRef ctxt, const char *str, size_t len)
{
	return SaxBounce::s(static_cast<JParser *>(jsax_getContext(ctxt)), std::string(str, len));
}

static int __number(JSAXContextRef ctxt, const char *number, size_t len)
{
	JParser *p = static_cast<JParser *>(jsax_getContext(ctxt));
	switch (SaxBounce::conversionToUse(p)) {
		case JParser::JNUM_CONV_RAW:
			return SaxBounce::n(p, std::string(number, len));
		case JParser::JNUM_CONV_NATIVE:
		{
			jvalue_ref toConv = jnumber_create_unsafe(j_str_to_buffer(number, len), NULL);
			int64_t asInteger;
			double asFloat;
			ConversionResultFlags toFloatErrors;

			if (CONV_OK == jnumber_get_i64(toConv, &asInteger)) {
				return SaxBounce::n(p, (asInteger));
			}
			toFloatErrors = jnumber_get_f64(toConv, &asFloat);
			return SaxBounce::n(p, asFloat, toFloatErrors);
		}
		default:
			PJ_LOG_ERR("Actual parser hasn't told us a valid type for how it wants numbers presented to it");
			return 0;
	}
}

static int __boolean(JSAXContextRef ctxt, bool value)
{
	return SaxBounce::b(static_cast<JParser *>(jsax_getContext(ctxt)), value);
}

static int __jnull(JSAXContextRef ctxt)
{
	return SaxBounce::N(static_cast<JParser *>(jsax_getContext(ctxt)));
}

static bool __err_parser(void *ctxt, JSAXContextRef parseCtxt)
{
	JParser *parser = static_cast<JParser *>(jsax_getContext(parseCtxt));
	JErrorHandler* handler = static_cast<JErrorHandler *>(ctxt);
	if (handler)
		handler->syntax(parser, JErrorHandler::ERR_SYNTAX_GENERIC, "unknown error parsing");
	return false;
}

static bool __err_schema(void *ctxt, JSAXContextRef parseCtxt)
{
	JParser *parser = static_cast<JParser *>(jsax_getContext(parseCtxt));
	JErrorHandler* handler = static_cast<JErrorHandler *>(ctxt);
	if (handler)
		handler->schema(parser, JErrorHandler::ERR_SCHEMA_GENERIC, "unknown schema violation parsing");
	return false;
}

static bool __err_unknown(void *ctxt, JSAXContextRef parseCtxt)
{
	JParser *parser = static_cast<JParser *>(jsax_getContext(parseCtxt));
	JErrorHandler* handler = static_cast<JErrorHandler *>(ctxt);
	if (handler)
		handler->misc(parser, "unknown error parsing");
	return false;
}

static inline raw_buffer strToRawBuffer(const std::string& str)
{
	return j_str_to_buffer(str.c_str(), str.length());
}

JParser::JParser(JResolver* schemaResolver)
	: m_errors(NULL), m_resolver(schemaResolver)
{
}

JParser::JParser(const JParser& other)
	: m_keyStack(other.m_keyStack), m_stateStack(other.m_stateStack), m_errors(other.m_errors)
{

}

JParser::~JParser()
{

}

bool JParser::parse(const std::string& input, const JSchema& schema, JErrorHandler *errors)
{
	PJSAXCallbacks callbacks = {
		__obj_start, __obj_key, __obj_end, __arr_start, __arr_end, __string, __number, __boolean, __jnull,
	};

	JErrorCallbacks errorHandler;
	errorHandler.m_parser = __err_parser;
	errorHandler.m_schema = __err_schema;
	errorHandler.m_unknown = __err_unknown;
	errorHandler.m_ctxt = errors;

	JSchemaResolver externalRefResolver;
	externalRefResolver.m_resolve = NULL;
	externalRefResolver.m_userCtxt = this;

	JSchemaInfo schemaInfo;
	jschema_info_init(&schemaInfo, schema.peek(), &externalRefResolver, &errorHandler);

	void *ctxt = this;
	m_errors = errors;
	bool parsed = jsax_parse_ex(&callbacks, strToRawBuffer(input), &schemaInfo, &ctxt, false);

	if (!parsed) {
		if (errors) errors->parseFailed(this, "");
		return false;
	}

	return true;
}

JSchemaResolutionResult JParser::resolve(JSchemaResolverRef resolver, jschema_ref *resolvedSchema)
{
	JSchema::Resource *simpleResource = new JSchema::Resource(resolver->m_ctxt, JSchema::Resource::CopySchema);
	JSchema parent(simpleResource);

	std::string resource(resolver->m_resourceToResolve.m_str, resolver->m_resourceToResolve.m_len);

	JResolver::ResolutionRequest request(parent, resource);
	JSchemaResolutionResult result;
	JSchema resolvedWrapper(m_resolver->resolve(request, result));

	std::cerr << "Resolving" << resource << std::endl;
	if (result == SCHEMA_RESOLVED) {
		*resolvedSchema = jschema_copy(resolvedWrapper.peek());
	} else
		*resolvedSchema = NULL;
	return result;
}

JErrorHandler* JParser::errorHandlers() const
{
	return m_errors;
}

void JParser::setErrorHandlers(JErrorHandler* errors)
{
	m_errors = errors;
}

JErrorHandler* JParser::getErrorHandler() const
{
	return m_errors;
}

JParser::ParserPosition JParser::getPosition() const
{
	return (JParser::ParserPosition){ -1, -1 };
}

JParser::DocumentState::DocumentState()
{
}

JParser::DocumentState::DocumentState(const DocumentState& other)
{
}

JParser::DocumentState::~DocumentState()
{
}

}
