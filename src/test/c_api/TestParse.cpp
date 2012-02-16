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
 * TestParse.cpp
 *
 *  Created on: Mar 11, 2009
 */

#include "TestParse.h"
#include <QTest>
#include <QtDebug>
#include <QMetaType>
#include <iostream>
#include <cassert>
#include <limits>
#include <execinfo.h>
#include <QList>
#include <QString>
#include <QFileInfo>

#include <pbnjson.h>

#include "../QBacktrace.h"

Q_DECLARE_METATYPE(jvalue_ref);
Q_DECLARE_METATYPE(ConversionResult);
Q_DECLARE_METATYPE(bool);

#if 0
	CONV_OK = 0,
	CONV_POSITIVE_OVERFLOW = 0x1, /// - the value for integers MUST be clamped to the largest representable number. for doubles, it will be positive infinity
	CONV_NEGATIVE_OVERFLOW = 0x2, /// - the value for integers MUST be clamped to the smallest representable number. for doubles, it will be negative infinity
/** CONV_POSITIVE_OVERFLOW | CONV_NEGATIVE_OVERFLOW */
#define CONV_OVERFLOW (CONV_POSITIVE_OVERFLOW | CONV_NEGATIVE_OVERFLOW)
	CONV_INFINITY = 0x4, ///  the value for integers MUST be clamped to the largest representable number
#define CONV_POSITIVE_INFINITY (CONV_POSITIVE_OVERFLOW | CONV_INFINITY)
#define CONV_NEGATIVE_INFINITY (CONV_NEGATIVE_OVERFLOW | CONV_INFINITY)
	CONV_PRECISION_LOSS = 0x8, /// bit is set if a double is requested but the int cannot be represented perfectly or an int is requested but the double has floating point
	CONV_NOT_A_NUM = 0x10, /// non-0 value + CONV_NOT_A_NUM = there's an integer approximation
	CONV_NOT_A_STRING = 0x20, /// returned if the type is not a string - the raw string representation is still returned as appropriate
	CONV_NOT_A_BOOLEAN = 0x40, /// returned if the type is not a boolean - the value written is always the boolean approximation
	CONV_NOT_A_RAW_NUM = 0x80, /// if an attempt is made to get the raw number from a JSON Number backed by a numeric primitive
	CONV_BAD_ARGS = 0x40000000, /// if the provided arguments are bogous (MUST NOT OVERLAP WITH ANY OTHER ERROR CODES)
	CONV_GENERIC_ERROR = 0x80000000 /// if some other unspecified error occured (MUST NOT OVERLAP WITH ANY OTHER ERROR CODES)
#endif

static const char* conv_result_str(ConversionResult res)
{
	switch (res) {
	case CONV_OK:
		return "ok";
	case CONV_POSITIVE_OVERFLOW:
		return "+ overflow";
	case CONV_NEGATIVE_OVERFLOW:
		return "- overflow";
	case CONV_INFINITY:
		return "infinity";
	case CONV_PRECISION_LOSS:
		return "precision loss";
	case CONV_NOT_A_NUM:
		return "not a number";
	case CONV_NOT_A_STRING:
		return "not a string";
	case CONV_NOT_A_BOOLEAN:
		return "not a boolean";
	case CONV_NOT_A_RAW_NUM:
		return "not a raw num";
	case CONV_BAD_ARGS:
		return "bad args";
	case CONV_GENERIC_ERROR:
		return "generic error";
	default:
		return "unknown error";
	}
}

static std::string result_str(ConversionResultFlags res) __attribute__((unused));
static std::string result_str(ConversionResultFlags res)
{
	std::string result;

	if (res == CONV_OK)
		return conv_result_str(CONV_OK);

#define CHECK_ERR(flag) \
	do {\
		if (res & (flag)) result += conv_result_str(flag);\
	} while (0)

	CHECK_ERR(CONV_POSITIVE_OVERFLOW);
	CHECK_ERR(CONV_NEGATIVE_OVERFLOW);
	CHECK_ERR(CONV_INFINITY);
	CHECK_ERR(CONV_PRECISION_LOSS);
	CHECK_ERR(CONV_NOT_A_NUM);
	CHECK_ERR(CONV_NOT_A_STRING);
	CHECK_ERR(CONV_NOT_A_BOOLEAN);
	CHECK_ERR(CONV_NOT_A_RAW_NUM);
	CHECK_ERR(CONV_BAD_ARGS);
	CHECK_ERR(CONV_GENERIC_ERROR);

	return result;

#undef CHECK_ERR
}

#define PRINT_BACKTRACE(fd, size) \
		do {\
			void *tracePtrs[size];\
			int count;\
			count = backtrace(tracePtrs, size);\
			backtrace_symbols_fd(tracePtrs, count, fd);\
		} while(0)

#define MY_VERIFY(statement) \
do {\
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__)) { \
    	PRINT_BACKTRACE(STDERR_FILENO, 16);\
    	/*qDebug() << "\n\t" << getBackTrace().join("\n\t") << "\n";*/\
		throw TestFailure();\
    } \
} while (0)

#define MY_COMPARE(actual, expected) \
do {\
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__)) { \
    	PRINT_BACKTRACE(STDERR_FILENO, 16);\
    	/*qDebug() << "\n\t" << getBackTrace().join("\n\t") << "\n";*/\
        throw TestFailure();\
    } \
} while (0)

namespace pjson {
namespace testc {

class TestFailure
{

};

TestParse::TestParse()
{
}

TestParse::~TestParse()
{

}

void TestParse::initTestCase()
{
	m_managed.clear();
	m_managedSchemas.clear();
}

void TestParse::init()
{
}

void TestParse::cleanup()
{
}

void TestParse::cleanupTestCase()
{
	for (size_t i = 0; i < m_managed.size(); i++) {
		qDebug() << "unmanaging" << m_managed[i];
		j_release(&m_managed[i]);
	}

	for (size_t i = 0; i < m_managedSchemas.size(); i++) {
		jschema_release(&m_managedSchemas[i]);
	}
}

void TestParse::testParseDoubleAccuracy()
{
	std::string jsonRaw("{\"errorCode\":0,\"timestamp\":1.268340607585E12,\"latitude\":37.390067,\"longitude\":-122.037626,\"horizAccuracy\":150,\"heading\":0,\"velocity\":0,\"altitude\":0,\"vertAccuracy\":0}");
	JSchemaInfo schemaInfo;

	jvalue_ref parsed;
	double longitude;

	jschema_info_init(&schemaInfo, jschema_all(), NULL, NULL);

	parsed = manage(jdom_parse(j_cstr_to_buffer(jsonRaw.c_str()), DOMOPT_NOOPT, &schemaInfo));
	QVERIFY(jis_object(parsed));
	QVERIFY(jis_number(jobject_get(parsed, J_CSTR_TO_BUF("longitude"))));
	QCOMPARE(jnumber_get_f64(jobject_get(parsed, J_CSTR_TO_BUF("longitude")), &longitude), (ConversionResultFlags)CONV_OK);
	QCOMPARE(longitude, -122.037626);
}

void TestParse::testParseFile_data()
{
	QTest::addColumn<QString>("file_name");

	QList<QString> files;
	files << "file_parse_test"
	;

	Q_FOREACH(QString file, files) {
		std::string tag = file.toStdString();
		QTest::newRow(tag.c_str()) << file;
	}
}

static bool identical(jvalue_ref obj1, jvalue_ref obj2)
{
	if (jis_object(obj1)) {
		if (!jis_object(obj2))
			return false;

		jobject_iter iter1 = jobj_iter_init(obj1);
		jobject_iter iter2 = jobj_iter_init(obj2);

		int numKeys1 = 0;
		int numKeys2 = 0;

		bool moreKeys = true;

		while (moreKeys) {
			moreKeys = false;

			if (jobj_iter_is_valid(iter1)) {
				numKeys1++;
				moreKeys = true;
				iter1 = jobj_iter_next(iter1);
			}
			if (jobj_iter_is_valid(iter2)) {
				numKeys2++;
				moreKeys = true;
				iter2 = jobj_iter_next(iter2);
			}
		}

		if (numKeys1 != numKeys2)
			return false;

		for (iter1 = jobj_iter_init(obj1); jobj_iter_is_valid(iter1); iter1 = jobj_iter_next(iter1)) {
			jobject_key_value keyval;
			jvalue_ref obj2Val;

			if (!jobj_iter_deref(iter1, &keyval))
				abort();

			if (!jobject_get_exists(obj2, jstring_get_fast(keyval.key), &obj2Val))
				return false;

			if (!identical(keyval.value, obj2Val))
				return false;
		}

		return true;
	}

	if (jis_array(obj1)) {
		if (!jis_array(obj2))
			return false;

		if (jarray_size(obj1) != jarray_size(obj2))
			return false;

		int ni = jarray_size(obj1);
		for (int i = 0; i < ni; i++) {
			if (!identical(jarray_get(obj1, i), jarray_get(obj2, i)))
				return false;
		}

		return true;
	}

	if (jis_string(obj1)) {
		if (!jis_string(obj2))
			return false;

		return jstring_equal(obj1, obj2);
	}

	if (jis_number(obj1)) {
		if (!jis_number(obj2))
			return false;

		return jnumber_compare(obj1, obj2) == 0;
	}

	if (jis_boolean(obj1)) {
		if (!jis_boolean(obj2))
			return false;

		bool b1, b2;
		return jboolean_get(obj1, &b1) == CONV_OK &&
		       jboolean_get(obj2, &b2) == CONV_OK &&
		       b1 == b2;
	}

	if (jis_null(obj1)) {
		if (!jis_null(obj2))
			return false;

		return true;
	}

	abort();
	return false;
}

void TestParse::testParseFile()
{
	QFETCH(QString, file_name);

	QString appName = QCoreApplication::arguments().at(0);
	QString filePrefix = QFileInfo(appName).absolutePath() + "/";
	std::string jsonInput = (filePrefix + file_name + ".json").toStdString();
	std::string jsonSchema = (filePrefix + file_name + ".schema").toStdString();

	JSchemaInfo schemaInfo;
	jvalue_ref inputNoMMap;
	jvalue_ref inputMMap;

	jschema_ref schema = manage(jschema_parse_file(jsonSchema.c_str(), NULL));
	QVERIFY(schema != NULL);

	jschema_info_init(&schemaInfo, schema, NULL, NULL);
	inputNoMMap = manage(jdom_parse_file(jsonInput.c_str(), &schemaInfo, JFileOptNoOpt));
	QVERIFY(!jis_null(inputNoMMap));

	inputMMap = manage(jdom_parse_file(jsonInput.c_str(), &schemaInfo, JFileOptMMap));
	QVERIFY(!jis_null(inputMMap));

	QVERIFY(identical(inputNoMMap, inputMMap));
}

}
}

QTEST_MAIN(pjson::testc::TestParse);
