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

#include "TestPerformance.h"
#include <QtTest>
#include <QStringList>
#include <QtDebug>

#include <cjson.h>
#include <yajl.h>

Q_DECLARE_METATYPE(pbnjson::testc::TestPerformance::TestLibrary)
Q_DECLARE_METATYPE(JDOMOptimizationFlags)
Q_DECLARE_METATYPE(jschema_ref)

QByteArray lastBadInput;

namespace pbnjson {
namespace testc {

QList<JDOMOptimizationFlags> TestPerformance::NO_OPTS;
QList<JDOMOptimizationFlags> TestPerformance::ALL_OPTS;
QList<jschema_ref> TestPerformance::EMPTY_SCHEMA;

TestPerformance::TestPerformance()
{

}

void TestPerformance::parseCJSON(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema)
{
#if HAVE_CJSON
	json_object *o = json_tokener_parse(input.m_str);
	if (!o || is_error(o)) {
		QByteArray badInput(input.m_str, input.m_len);
		if (lastBadInput != badInput) {
			qWarning() << "Potentially bad input: " << badInput;
			lastBadInput = badInput;
		} else
			qWarning() << "Bad input same as previous failure";

		throw "CJSON parse failure";
	}
	json_object_put(o);
#endif
}

void TestPerformance::parsePBNJSON(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema)
{
	JSchemaInfo schemaInfo;
	jschema_info_init(&schemaInfo, schema, NULL, NULL);
	jvalue_ref jv = jdom_parse(input, opt, &schemaInfo);

	if (jis_null(jv)) {
		QByteArray badInput(input.m_str, input.m_len);
		if (lastBadInput != badInput) {
			qWarning() << "Potentially bad input: " << badInput;
			lastBadInput = badInput;
		} else
			qWarning() << "Bad input same as previous failure";
		
		throw "PBNJSON parse failure";
	}

	j_release(&jv);
}

void TestPerformance::parseYAJL(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema)
{
#if HAVE_YAJL
	yajl_callbacks nocb = { 0 };
	yajl_handle handle = yajl_alloc(&nocb, NULL, NULL, NULL);
	if (yajl_status_ok != yajl_parse(handle, (const unsigned char *)input.m_str, input.m_len))
		goto parse_problem;

	if (yajl_status_ok != yajl_parse_complete(handle))
		goto parse_problem;

	yajl_free(handle);
	return;

parse_problem:
	yajl_free(handle);

	QByteArray badInput(input.m_str, input.m_len);
	if (lastBadInput != badInput) {
		qWarning() << "Potentially bad input: " << badInput;
		lastBadInput = badInput;
	} else
		qWarning() << "Bad input same as previous failure";
	
	throw "YAJL parse failure";

#endif
}

void TestPerformance::parseSAX(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema)
{
	JSchemaInfo schemaInfo;
	jschema_info_init(&schemaInfo, schema, NULL, NULL);

	if (!jsax_parse(NULL, input, &schemaInfo)) {
		QByteArray badInput(input.m_str, input.m_len);
		if (lastBadInput != badInput) {
			qWarning() << "Potentially bad input: " << badInput;
			lastBadInput = badInput;
		} else
			qWarning() << "Bad input same as previous failure";
		
		throw "PBNJSON SAX parse failure";
	}
}

void TestPerformance::initTestCase()
{
	NO_OPTS << DOMOPT_NOOPT;
	ALL_OPTS << DOMOPT_INPUT_NOCHANGE << DOMOPT_INPUT_NULL_TERMINATED << DOMOPT_INPUT_OUTLIVES_DOM
		<< (DOMOPT_INPUT_NOCHANGE | DOMOPT_INPUT_OUTLIVES_DOM)
		<< (DOMOPT_INPUT_NOCHANGE | DOMOPT_INPUT_OUTLIVES_DOM | DOMOPT_INPUT_NOCHANGE);
	EMPTY_SCHEMA << jschema_all();

	QString bigInput1 = "{ "
			"\"o1\" : null, "
			"\"o2\" : {}, "
			"\"a1\" : null, "
			"\"a2\" : [], "
			"\"o3\" : {"
				"\"x\" : true, "
				"\"y\" : false, "
				"\"z\" : \"\\\"es'ca'pes'\\\"\""
			"}, "
			"\"n1\" : 0"
			"                              "
			",\"n2\" : 232452312412, "
			"\"n3\" : -233243.653345e-2342 "
			"                              "
			",\"s1\" : \"adfa\","
			"\"s2\" : \"asdflkmsadfl jasdf jasdhf ashdf hasdkf badskjbf a,msdnf ;whqoehnasd kjfbnakjd "
			"bfkjads fkjasdbasdf jbasdfjk basdkjb fjkndsab fjk\","
			"\"a3\" : [ true, false, null, true, false, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}],"
			"\"a4\" : [[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[],[]],"
			"\"n4\" : 928437987349237893742897234987243987234297982347987249387,"
			"\"b1\" : true"
			"}";

	smallInputs << "{}"
		<< "[]"
		<< "[\"e1\", \"e2\", \"e3\"]"
		<< "{ \"returnValue\" : true }"
		<< "{ \"returnValue\" : true, \"results\" : [ { \"property\" : \"someName\", \"value\" : 40.5 } ] }"
		;

	bigInputs << bigInput1;

#if !HAVE_CJSON
	qWarning() << "Compiled without cjson support - no comparitive performance";
#endif

#if !HAVE_YAJL
	qWarning() << "Compiled without yajl support - no comparitive performance";
#endif
}

void TestPerformance::cleanupTestCase()
{

}

int TestPerformance::domData(QStringList jsonInput,
		const char *type, TestLibrary lib,
		QList<JDOMOptimizationFlags> opts,
		QList<jschema_ref> schemas)
{
	static char title[80] = {0};
	int rows = 0;

	const char *optStr, *gramStr, *libStr;

	JDOMOptimizationFlags noCopyFlags = DOMOPT_INPUT_NOCHANGE | DOMOPT_INPUT_OUTLIVES_DOM;
	JDOMOptimizationFlags fastToStringFlags = noCopyFlags | DOMOPT_INPUT_NULL_TERMINATED;

	for (int i = 0; i < opts.size(); i++) {
		JDOMOptimizationFlags opt = opts.at(i);
		for (int j = 0; j < schemas.size(); j++) {
			jschema_ref schema = schemas.at(j);
			for (int i = 0; i < jsonInput.size(); i++) {
				rows++;
#if !HAVE_CJSON
				if (lib == CJSON) continue;
#endif

#if !HAVE_YAJL
				if (lib == YAJL) continue;
#endif

				QString qOptStr("");
				qOptStr += (opt & noCopyFlags) ? " w/ nocopy" : " w/ copy";
				qOptStr += (opt & fastToStringFlags) ? " w/ fast tostring" : " w/o fast tostring";

				optStr = qPrintable(qOptStr);

				gramStr = (schema == jschema_all() ? " w/ no schema" : " w/ schema");
				if (lib == CJSON) libStr = "cjson";
				else if (lib == PBNJSON) libStr = "pbnjson dom";
				else if (lib == YAJL) libStr = "yajl";
				else if (lib == PBNJSON_SAX) libStr = "pbnjson sax";
				else libStr = "unrecognized parser type";

				snprintf(title, sizeof(title),
						"%s %s performance test %s%s parsing input %d",
						libStr, type, optStr, gramStr, i);
				QTest::newRow(title) << lib << jsonInput[i] << opt << schema;
			}
		}
	}
	return rows;
}

void TestPerformance::testParser_data()
{
	int rows;

	QTest::addColumn<TestLibrary>("lib");
	QTest::addColumn<QString>("input");
	QTest::addColumn<JDOMOptimizationFlags>("opt");
	QTest::addColumn<jschema_ref>("schema");

	rows = domData(smallInputs, "small dom", CJSON, NO_OPTS);
	QCOMPARE(rows, 5);
	rows = domData(smallInputs, "small dom", YAJL, NO_OPTS);
	QCOMPARE(rows, 5);
	rows = domData(smallInputs, "small dom", PBNJSON_SAX, NO_OPTS);
	QCOMPARE(rows, 5);
	rows = domData(smallInputs, "small dom", PBNJSON, ALL_OPTS);
	QCOMPARE(rows, 25);

	QCOMPARE(bigInputs.size(), 1);
	rows = domData(bigInputs, "big dom", CJSON, NO_OPTS);
	QCOMPARE(rows, 1);
	rows = domData(bigInputs, "big dom", YAJL, NO_OPTS);
	QCOMPARE(rows, 1);
	rows = domData(bigInputs, "big dom", PBNJSON_SAX, NO_OPTS);
	QCOMPARE(rows, 1);
	rows = domData(bigInputs, "big dom", PBNJSON, ALL_OPTS);
	QCOMPARE(rows, 5);
}

void TestPerformance::testParser()
{
	QFETCH(QString, input);
	QFETCH(TestLibrary, lib);
	QFETCH(JDOMOptimizationFlags, opt);
	QFETCH(jschema_ref, schema);

	QByteArray utf8 = input.toUtf8();
	raw_buffer inputStr = (raw_buffer) { utf8.constData(), utf8.size() };
	try {
		if (lib == CJSON) {
			QBENCHMARK {
				parseCJSON(inputStr, opt, schema);
			}
		} else if (lib == PBNJSON) {
			QBENCHMARK {
				parsePBNJSON(inputStr, opt, schema);
			}
		} else if (lib == YAJL) {
			QBENCHMARK {
				parseYAJL(inputStr, opt, schema);
			}
		} else if (lib == PBNJSON_SAX) {
			QBENCHMARK {
				parseSAX(inputStr, opt, schema);
			}
		} else {
			// unhandled benchmark type
			QVERIFY(0 == 1);
		}
	} catch (const char *str) {
		qWarning() << "Test failure: " << str << ".  Skipping benchmark";
	} catch (...) {
		qWarning() << "Unhandled test failure.  Skipping benchmark";
	}
}

void TestPerformance::testSAX_data()
{

}

void TestPerformance::testSAX()
{

}

}
}

QTEST_APPLESS_MAIN(pbnjson::testc::TestPerformance)

