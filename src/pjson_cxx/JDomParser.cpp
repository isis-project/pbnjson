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
 * JDomParser.cpp
 *
 *  Created on: Sep 28, 2009
 */

#include <JDomParser.h>
#include <pbnjson.h>
#include "JErrorHandler.h"
#include <JSchema.h>

#include <JResolver.h>

namespace pbnjson {

static inline raw_buffer strToRawBuffer(const std::string& str)
{
	return j_str_to_buffer(str.c_str(), str.length());
}

static bool __err_parser(void *ctxt, JSAXContextRef parseCtxt)
{
	JDomParser *parser = static_cast<JDomParser *>(ctxt);
	JErrorHandler* handler = parser->getErrorHandler();
	if (handler)
		handler->syntax(parser, JErrorHandler::ERR_SYNTAX_GENERIC, "unknown error parsing");
	return false;
}

static bool __err_schema(void *ctxt, JSAXContextRef parseCtxt)
{
	JDomParser *parser = static_cast<JDomParser *>(ctxt);
	JErrorHandler* handler = parser->getErrorHandler();
	if (handler)
		handler->schema(parser, JErrorHandler::ERR_SCHEMA_GENERIC, "unknown schema violation parsing");
	return false;
}

static bool __err_unknown(void *ctxt, JSAXContextRef parseCtxt)
{
	JDomParser *parser = static_cast<JDomParser *>(ctxt);
	JErrorHandler* handler = parser->getErrorHandler();
	if (handler)
		handler->misc(parser, "unknown error parsing");
	return false;
}

JDomParser::JDomParser(JResolver *resolver)
	: JParser(resolver), m_optimization(DOMOPT_NOOPT), m_resolver(resolver)
{
}

JDomParser::~JDomParser()
{
}

JSchemaInfo JDomParser::prepare(const JSchema& schema, const JSchemaResolver& resolver, const JErrorCallbacks& cErrCbs, JErrorHandler *errors)
{
	JSchemaInfo schemaInfo;

	jschema_info_init(&schemaInfo,
	                  schema.peek(),
	                  const_cast<JSchemaResolverRef>(&resolver),
	                  const_cast<JErrorCallbacksRef>(&cErrCbs));

	setErrorHandlers(errors);

	return schemaInfo;
}


JSchemaResolver JDomParser::prepareResolver() const
{
	JSchemaResolver resolver;
	resolver.m_resolve = sax_schema_resolver;
	resolver.m_userCtxt = const_cast<JDomParser *>(this);
	return resolver;
}

JErrorCallbacks JDomParser::prepareCErrorCallbacks() const
{
	/*
 	 *  unfortunately, I can't see a way to re-use the C++ sax parsing code
 	 *  while at the same time using the C code that builds the DOM.
 	 */
	JErrorCallbacks cErrCallbacks;
	cErrCallbacks.m_parser = __err_parser;
	cErrCallbacks.m_schema = __err_schema;
	cErrCallbacks.m_unknown = __err_unknown;
	cErrCallbacks.m_ctxt = const_cast<JDomParser *>(this);
	return cErrCallbacks;
}

bool JDomParser::parse(const std::string& input, const JSchema& schema, JErrorHandler *errors)
{
	JSchemaResolver resolver = prepareResolver();
	JErrorCallbacks errCbs = prepareCErrorCallbacks();
	JSchemaInfo schemaInfo = prepare(schema, resolver, errCbs, errors);

	m_dom = jdom_parse(strToRawBuffer(input), m_optimization, &schemaInfo);

	if (m_dom.isNull()) {
		if (errors) errors->parseFailed(this, "");
		return false;
	}

	return true;
}

bool JDomParser::parseFile(const std::string &file, const JSchema &schema, JFileOptimizationFlags optimization, JErrorHandler *errors)
{
	JSchemaResolver resolver = prepareResolver();
	JErrorCallbacks errCbs = prepareCErrorCallbacks();
	JSchemaInfo schemaInfo = prepare(schema, resolver, errCbs, errors);

	m_dom = jdom_parse_file(file.c_str(), &schemaInfo, optimization);

	if (m_dom.isNull()) {
		if (errors) errors->parseFailed(this, "");
		return false;
	}

	return true;
}

JValue JDomParser::getDom() {
	return m_dom;
}

}
