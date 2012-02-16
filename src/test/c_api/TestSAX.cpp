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
 * TestSAX.cpp
 *
 *  Created on: Sep 22, 2009
 */

#include "TestSAX.h"
#include <QObject>
#include <QtDebug>
#include <QByteArray>
#include <QMap>
#include <pbnjson.h>
#include <string>
#include <iostream>
#include "JSXMLConverter.h"

Q_DECLARE_METATYPE(jvalue_ref);
Q_DECLARE_METATYPE(std::string);
Q_DECLARE_METATYPE(QDomDocument);

namespace pjson {

namespace testc {

template <class T>
class auto_cptr {
private:
	T* m_ptr;

public:
	auto_cptr(T* allocated = NULL) : m_ptr(allocated){}
	auto_cptr(auto_cptr& other) : m_ptr(other.m_ptr) { other.m_ptr = NULL; }
	~auto_cptr() { free(m_ptr); }

	T* operator()() const { return m_ptr; }
	operator T*() const { return m_ptr; }

	auto_cptr& operator=(auto_cptr& other) {
		if (this != &other) {
			free(m_ptr);
			m_ptr = other.m_ptr;
			other.m_ptr = NULL;
		}
		return *this;
	}
};

struct MySaxCtxt {
	QStringList *built;
	bool storeActualValue;
};

static inline QStringList& myGetOpcodes(JSAXContextRef ctxt)
{
	MySaxCtxt *myctxt = (MySaxCtxt*)jsax_getContext(ctxt);
	return *myctxt->built;
}

static inline QStringList& myGetOpcodes(JSAXContextRef ctxt, bool &actualValue)
{
	MySaxCtxt *myctxt = (MySaxCtxt*)jsax_getContext(ctxt);
	actualValue = myctxt->storeActualValue;
	return *myctxt->built;
}

static int sax_ostart(JSAXContextRef ctxt)
{
//	qDebug() << "{";
	bool useValue;
	QStringList &l = myGetOpcodes(ctxt, useValue);
	if (useValue) {
		if (!l.isEmpty() && l.last() != ":" && l.last() != "{" && l.last() != "[")
			l << ",";
	}
	l << "{";
	return 1;
}

static int sax_oend(JSAXContextRef ctxt)
{
//	qDebug() << "}";
	myGetOpcodes(ctxt) << "}";
	return 1;
}

static int sax_astart(JSAXContextRef ctxt)
{
//	qDebug() << "[";
	bool useValue;
	QStringList &l = myGetOpcodes(ctxt, useValue);
	if (useValue) {
#if 0
		QByteArray ultimate = l.last().toUtf8();
		QByteArray penUltimate = l.last().toUtf8();
		const char *a = ultimate.constData();
		const char * b = penUltimate.constData();
#endif
		if (!l.isEmpty() && l.last() != ":" && l.last() != "{" && l.last() != "[")
			l << ",";
	}
	l << "[";
	return 1;
}

static int sax_aend(JSAXContextRef ctxt)
{
//	qDebug() << "]";
	myGetOpcodes(ctxt) << "]";
	return 1;
}

static int sax_string(JSAXContextRef ctxt, char *input, size_t len)
{
//	qDebug() << "string";
	bool useValue;
	QStringList &l = myGetOpcodes(ctxt, useValue);
	if (useValue) {
		if (l.last() != ":" && l.last() != "{" && l.last() != "[")
			l << ",";
		l << "\"" << QString::fromUtf8(input, len) << "\"";
	}
	else l << "s";
	return 1;
}

static int sax_key(JSAXContextRef ctxt, char *input, size_t len)
{
//	qDebug() << "key";
	bool useValue;
	QStringList &l = myGetOpcodes(ctxt, useValue);
	if (useValue) {
		if (l.last() != "{")
			l << ",";
		l << "\"" << QString::fromUtf8(input, len) << "\"" << ":";
	}
	else l << "s";
	return 1;
}

static int sax_num(JSAXContextRef ctxt, char *input, size_t len)
{
//	qDebug() << "number";
	bool useValue;
	QStringList &l = myGetOpcodes(ctxt, useValue);
	if (useValue) {
		if (l.last() != ":" && l.last() != "{" && l.last() != "[")
			l << ",";
		l << QString::fromAscii(input, len);
	}
	else l << "n";
	return 1;
}

static int sax_bool(JSAXContextRef ctxt, bool value)
{
//	qDebug() << "bool";
	bool useValue;
	QStringList &l = myGetOpcodes(ctxt, useValue);
	if (useValue) {
		if (l.last() != ":" && l.last() != "{" && l.last() != "[")
			l << ",";
		l << (value ? "true" : "false");
	}
	else l << "b";
	return 1;
}

static int sax_null(JSAXContextRef ctxt)
{
//	qDebug() << "null";
	bool useValue;
	QStringList &l = myGetOpcodes(ctxt, useValue);
	if (useValue) {
		if (l.last() != ":" && l.last() != "{" && l.last() != "[")
			l << ",";
		l << "null";
	}
	else l << "N";
	return 1;
}
static void generate(jvalue_ref jval, JStreamRef s)
{
	if (jis_object(jval)) {
		s->o_begin(s);
		typedef QMap<JValueWrapper, jvalue_ref> JObject;
		JObject sorted;
		for (jobject_iter i = jobj_iter_init(jval); jobj_iter_is_valid(i); i = jobj_iter_next(i)) {
			jobject_key_value keyval;
			if (!jobj_iter_deref(i, &keyval))
				throw "Invalid iterator";
			if (!jis_string(keyval.key))
				throw "invalid key";
			sorted.insert(keyval.key, keyval.value);
		}

		for (JObject::const_iterator i = sorted.constBegin(); i != sorted.constEnd(); i++) {
			generate(i.key(), s);
			generate(i.value(), s);
		}
		s->o_end(s);
	} else if (jis_array(jval)) {
		s->a_begin(s);
		for (ssize_t i = 0; i < jarray_size(jval); i++)
			generate(jarray_get(jval, i), s);
		s->a_end(s);
	} else if (jis_string(jval)) {
		s->string(s, jstring_get_fast(jval));
	} else if (jis_number(jval)) {
		raw_buffer asStr;
		int64_t integer;
		double floating;
		if (CONV_OK == jnumber_get_raw(jval, &asStr) && asStr.m_str != NULL)
			s->number(s, asStr);
		else if (CONV_OK == jnumber_get_i64(jval, &integer))
			s->integer(s, integer);
		else if (CONV_OK == jnumber_get_f64(jval, &floating))
			s->floating(s, floating);
		else
			throw "No error-free storage for this number?";
	} else if (jis_null(jval)) {
		s->null_value(s);
	} else {
		bool val;
		if (CONV_OK != jboolean_get(jval, &val))
			throw "unrecognized type";
		s->boolean(s, val);
	}
}

TestSAX::TestSAX() {
	// TODO Auto-generated constructor stub

}

TestSAX::~TestSAX() {
	// TODO Auto-generated destructor stub
}

void TestSAX::initTestCase()
{
	m_managed.clear();
}

void TestSAX::cleanupTestCase()
{
	for (size_t i = 0; i < m_managed.size(); i++) {
		j_release(&m_managed[i]);
	}
}

void TestSAX::testParser_data()
{
	QTest::addColumn<QByteArray>("input");
	QTest::addColumn<QStringList>("expectedOpcodes");

	// { = start object
	// } = end object
	// [ = start array
	// ] = end array
	// N = null
	// s = string
	// n = number
	// b = boolean
	QTest::newRow("empty object") <<
		QByteArray("{}") <<
		(QStringList() << "{" << "}");

	QTest::newRow("empty array") <<
		QByteArray("[]") <<
		(QStringList() << "[" << "]");

	QTest::newRow("simple object 1") <<
		QByteArray("{\"returnValue\":     true}") <<
		(QStringList() << "{" << "s" << "b" << "}");

#define O_NUM "s" << "n"
	QTest::newRow("simple object 1") <<
		QByteArray("{\"n1\": 2147483647, \"n2\": 2147483648  , \"n3\": 4294967295, \"n4\":4294967296  }") <<
		(QStringList() << "{" << O_NUM << O_NUM << O_NUM << O_NUM << "}");
#undef O_NUM
}

static PJSAXCallbacks myCallbacks = {
		(jsax_object_start)sax_ostart,
		(jsax_object_key)sax_key,
		(jsax_object_start)sax_oend,

		(jsax_array_start)sax_astart,
		(jsax_array_end)sax_aend,

		(jsax_string)sax_string,
		(jsax_number)sax_num,
		(jsax_boolean)sax_bool,
		(jsax_null)sax_null,
};

void TestSAX::testParser()
{
	QFETCH(QByteArray, input);
	QFETCH(QStringList, expectedOpcodes);
	QStringList opcodes;
	MySaxCtxt myctxt = { &opcodes, false };

	try {

		void *userData = &myctxt;
		JSchemaInfo schemaInfo;
		jschema_info_init(&schemaInfo, jschema_all(),
				NULL /* not a good idea - but we're using the empty schema anyways */,
				NULL /* use the default error handler - no recovery possible */);

		QVERIFY2(jsax_parse_ex(&myCallbacks, j_str_to_buffer(input.constData(), input.size()), &schemaInfo, &userData, true), input);
		QCOMPARE(opcodes, expectedOpcodes);
		QCOMPARE(userData, &myctxt);
	} catch (const char *msg) {
		qWarning() << opcodes << "vs" << expectedOpcodes;
		QFAIL(msg);
	}
}

static inline QDomDocument domFromString(QByteArray str)
{
	QDomDocument asDOM;
	if (!asDOM.setContent(str, true))
		throw "Unable to create DOM for xml";
	return asDOM;
}

static QString jsxml(QString body)
{
	static QString openDocument = "<?xml version='1.0' encoding='utf-8'?>\
<jsxml xmlns:jsxml='http://palm.com/pbnjson' xmlns:obj='http://palm.com/pbnjson/object' xmlns:arr='http://palm.com/pbnjson/array'			\
	xmlns:str='http://palm.com/pbnjson/string'  xmlns:num='http://palm.com/pbnjson/number' xmlns:bool='http://palm.com/pbnjson/boolean'		\
	jsxml:libVersion='0.2' jsxml:library='pbnjson' jsxml:jsonEngine='yajl' jsxml:engineVersion=''>";
	static QString closeDocument = "</jsxml>";
	static QStringList document = (QStringList() << openDocument << "" << closeDocument);

	document.replace(1, body);
	return document.join("");
}

static QString simplify(QString formattedJSON)
{
	QStringList values;
	MySaxCtxt myctxt = { &values, true };
	QByteArray input = formattedJSON.toUtf8();
	void *userData = &myctxt;

	JSchemaInfo info;
	jschema_info_init(&info, jschema_all(),
			NULL /* not a good idea - but we're using the empty schema anyways */,
			NULL /* use the default error handler - no recovery possible */);

	if (!jsax_parse_ex(&myCallbacks, j_str_to_buffer(input.constData(), input.size()), &info, &userData, true)) {
		qWarning() << "Unable to parse" << formattedJSON;
		throw "bad input";
	}
	if (userData != &myctxt)
		throw "context switched unexpectadly";

	return values.join("");
}

void TestSAX::testGenerator_data()
{

	QTest::addColumn<jvalue_ref>("genInstructions");
	QTest::addColumn<QString>("rawJSON");
	QTest::addColumn<QString>("xmlDescription");

	QTest::newRow("sanity object check") <<
		manage(jobject_create()) << "{}" << jsxml("<jsxml:object obj:length='0' />");

	QTest::newRow("sanity array check") <<
		manage(jarray_create(NULL)) << "[]" << jsxml("<jsxml:array arr:length='0' />");

	QTest::newRow("numeric array check") <<
		manage(jarray_create_var(NULL,
			jnumber_create_i32(0),
			jnumber_create_i32(1),
			jnumber_create_i32(2),
			jnumber_create_i32(3),
			jnumber_create_i64(4),
			J_END_ARRAY_DECL)) << "[0,1,2,3,4]" << jsxml(
				"<jsxml:array arr:length='5'>"
					"<arr:number arr:index='0' num:format='integer'>0</arr:number>"
					"<arr:number arr:index='1' num:format='integer'>1</arr:number>"
					"<arr:number arr:index='2' num:format='integer'>2</arr:number>"
					"<arr:number arr:index='3' num:format='integer'>3</arr:number>"
					"<arr:number arr:index='4' num:format='integer'>4</arr:number>"
				"</jsxml:array>");

	QTest::newRow("complex json check") <<
		manage(jobject_create_var(
				jkeyval(J_CSTR_TO_JVAL("numbers"),
						jarray_create_var(NULL,
								jnumber_create(J_CSTR_TO_BUF("23532325323.2352398322348239083290480923823489238023890e90238049823908234908234098234238")),
								jnumber_create(J_CSTR_TO_BUF("-23532325323.2352398322348239083290480923823489238023890e90238049823908234908234098234238")),
								jnumber_create(J_CSTR_TO_BUF("23532325323.2352398322348239083290480923823489238023890e-90238049823908234908234098234238")),
								jnumber_create(J_CSTR_TO_BUF("23532325323.2352398322348239083290480923823489238023890e+90238049823908234908234098234238")),
								jnumber_create(J_CSTR_TO_BUF("-23532325323.2352398322348239083290480923823489238023890E+90238049823908234908234098234238")),
								J_END_ARRAY_DECL
						)
				),

				jkeyval(J_CSTR_TO_JVAL("object1"),
						jobject_create_var(
								jkeyval(J_CSTR_TO_JVAL("test2"), J_CSTR_TO_JVAL("test1")),
								jkeyval(J_CSTR_TO_JVAL("test1"), J_CSTR_TO_JVAL("test2")),
								J_END_OBJ_DECL
						)
				),

				jkeyval(J_CSTR_TO_JVAL("array1"),
						jarray_create_var(NULL,
							J_CSTR_TO_JVAL("testing1"), J_CSTR_TO_JVAL("test2"),
							jnumber_create_i32(0), jnumber_create_i32(50),
							jnumber_create_f64(42323.0234234), jnumber_create(J_CSTR_TO_BUF("2354235235.23523892389e23343")),
							jarray_create_var(NULL, jboolean_create(true), jboolean_create(false), jnull(), J_END_ARRAY_DECL),
							jnull(),
							J_CSTR_TO_JVAL(""),
							J_CSTR_TO_JVAL(""),
							J_END_ARRAY_DECL
						)
				),

				J_END_OBJ_DECL
		)) <<
		// the instructions are sorted by key
		// array1 -> numbers -> object1
		simplify("{																								\
			\"array1\": [																						\
				\"testing1\",\"test2\",0,50,42323.0234234,2354235235.23523892389e23343,[true,false,null],null,\"\",\"\"		\
			],															\
			\"numbers\":[													\
				23532325323.2352398322348239083290480923823489238023890e90238049823908234908234098234238,		\
				-23532325323.2352398322348239083290480923823489238023890e90238049823908234908234098234238,		\
				23532325323.2352398322348239083290480923823489238023890e-90238049823908234908234098234238,		\
				23532325323.2352398322348239083290480923823489238023890e+90238049823908234908234098234238,		\
				-23532325323.2352398322348239083290480923823489238023890E+90238049823908234908234098234238		\
			],														\
			\"object1\":{													\
				\"test1\":\"test2\",											\
				\"test2\":\"test1\"											\
			}\
		}") <<
		jsxml("\
<jsxml:object obj:length='3'>																									\
		<obj:array obj:key='array1' arr:length='10'>																			\
				<arr:string arr:index='0' str:length='8'>testing1</arr:string>													\
				<arr:string arr:index='1' str:length='5'>test2</arr:string>														\
				<arr:number arr:index='2' num:format='integer'>0</arr:number>													\
				<arr:number arr:index='3' num:format='integer'>50</arr:number>													\
				<arr:number arr:index='4' num:format='floating'>42323.0234234</arr:number>										\
				<arr:number arr:index='5' num:format='string' num:length='28'>2354235235.23523892389e23343</arr:number>			\
				<arr:array arr:index='6' arr:length='3'>																		\
					<arr:boolean arr:index='0'>true</arr:boolean>																\
					<arr:boolean arr:index='1'>false</arr:boolean>																\
					<arr:null arr:index='2'/>																					\
				</arr:array>																									\
				<arr:null arr:index='7'></arr:null>																				\
				<arr:string str:length='8' arr:index='8'/>																		\
				<arr:string str:length='9' arr:index='8'></arr:string>															\
		</obj:array>																											\
		<obj:obj obj:key='object1' obj:length='2'>																				\
			<obj:string obj:key='test1' str:length='5'>test2</obj:string>														\
			<obj:string obj:key='test2' str:length='5'>test1</obj:string>														\
		</obj:obj>																												\
		<obj:array obj:key='numbers' arr:length='5'>																			\
				<arr:number arr:index='0' num:format='string' num:length='88'>													\
					23532325323.2352398322348239083290480923823489238023890e90238049823908234908234098234238					\
				</arr:number>																									\
				<arr:number arr:index='1' num:format='string' num:length='89'>													\
					-23532325323.2352398322348239083290480923823489238023890e90238049823908234908234098234238					\
				</arr:number>																									\
				<arr:number arr:index='2' num:format='string' num:length='89'>													\
					23532325323.2352398322348239083290480923823489238023890e-90238049823908234908234098234238					\
				</arr:number>																									\
				<arr:number arr:index='3' num:format='string' num:length='89'>													\
					23532325323.2352398322348239083290480923823489238023890e+90238049823908234908234098234238					\
				</arr:number>																									\
				<arr:number arr:index='4' num:format='string' num:length='90'>													\
					-23532325323.2352398322348239083290480923823489238023890E+90238049823908234908234098234238					\
				</arr:number>																									\
		</obj:array>																											\
</jsxml:object>");
}

void TestSAX::testGenerator()
{
	QFETCH(jvalue_ref, genInstructions);
	QFETCH(QString, rawJSON);
	QFETCH(QString, xmlDescription);

	QByteArray generatedOutput;

	try {
		QDomDocument expectedXML;
		JStreamRef generator = jstream(NULL);
		generate(genInstructions, generator);
		StreamStatus result;
		auto_cptr<char> output(generator->finish(generator, &result));
		if (output == NULL) {
			qWarning() << "Failure to generate output:" << result;
			throw "Unable to generate output";
		}
		generatedOutput = QByteArray::fromRawData(output, strlen(output));
		QCOMPARE(QString(generatedOutput), rawJSON);

		QDomDocument generatedXML = JSXMLConverter::convert(generatedOutput);
		QString err;
		int line, column;
		if (!expectedXML.setContent(xmlDescription, true, &err, &line, &column)) {
			qWarning() << err << "@ line" << line << "character" << column;
			std::cerr << qPrintable(xmlDescription) << std::endl << qPrintable(QString(column, '-')) << std::endl;
			QFAIL("Failed to create xml");
		}

		QVERIFY(JSXMLConverter::domEquivalent(generatedXML, expectedXML));
	} catch (const char *msg) {
		QFAIL(msg);
	} catch (...) {
		QFAIL("unexpected exception");
	}
}

}

}

QTEST_APPLESS_MAIN(pjson::testc::TestSAX)
