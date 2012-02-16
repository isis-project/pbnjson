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
 * TestDom.cpp
 *
 *  Created on: Sep 15, 2009
 */

#include "TestDOM.h"
#include <QTest>
#include <QtDebug>
#include <QMetaType>
#include <iostream>
#include <cassert>
#include <limits>
#include <execinfo.h>

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

template <>
void TestDOM::validateNumber<int32_t>(jvalue_ref jnum, int32_t expected, ConversionResultFlags conversionResult);
template <>
void TestDOM::validateNumber<int64_t>(jvalue_ref jnum, int64_t expected, ConversionResultFlags conversionResult);
template <>
void TestDOM::validateNumber<double>(jvalue_ref jnum, double expected, ConversionResultFlags conversionResult);
template <>
void TestDOM::validateNumber<raw_buffer>(jvalue_ref jnum, raw_buffer expected, ConversionResultFlags conversionResult);

#define GET_CHILD(var, type, jval, id) \
	try {\
		var = getChild ## type(jval, id); \
	} catch (...) { \
		std::cerr << "Child retrieval failure at '" __FILE__ " :: " << __LINE__ << "'" << std::endl;\
		throw;\
	}

#define VAL_NUM(type, jval, expectVal, expectConvResult) \
		try {\
			validateNumber<type>(jval, expectVal, expectConvResult); \
		} catch (...) { \
			std::cerr << "Number validation failure at '" __FILE__ " :: " << __LINE__ << "'" << std::endl;\
			throw;\
		}

#define VAL_NUM_OK(type, jval, expectVal) VAL_NUM(type, jval, expectVal, CONV_OK)

#define VAL_STR(jval, expected) \
	try {\
		validateString(jval, expected); \
	} catch (...) { \
		std::cerr << "String validation failure at '" __FILE__ " :: " << __LINE__ << "'" << std::endl;\
		throw;\
	}

#define VAL_BOOL(jval, expected, expectedType)\
	try {\
		validateBool(jval, expected, expectedType); \
	} catch (...) { \
		std::cerr << "String validation failure at '" __FILE__ " :: " << __LINE__ << "'" << std::endl;\
		throw;\
	}

#define VAL_BOOL_AS_BOOL(jval, expected) VAL_BOOL(jval, expected, TJBOOL)

TestDOM::TestDOM()
{
	//testValist(jkeyval((jvalue_ref)0x1, (jvalue_ref)0x2), jkeyval(NULL, NULL));
}

TestDOM::~TestDOM()
{

}

void TestDOM::initTestCase()
{
	m_managed.clear();
}

void TestDOM::init()
{
}

void TestDOM::cleanup()
{
}

void TestDOM::cleanupTestCase()
{
	for (size_t i = 0; i < m_managed.size(); i++) {
		j_release(&m_managed[i]);
	}
}

void TestDOM::testObjectSimple()
{
	assert(!jis_null(manage(J_CSTR_TO_JVAL("abc"))));
	assert(!jis_null(manage(J_CSTR_TO_JVAL("def"))));
	assert(!jis_null(manage(jnumber_create(J_CSTR_TO_BUF("5463")))));

	jvalue_ref simpleObject = manage(jobject_create_var(
		jkeyval(J_CSTR_TO_JVAL("abc"), J_CSTR_TO_JVAL("def")),
		jkeyval(J_CSTR_TO_JVAL("def"), jnumber_create(J_CSTR_TO_BUF("5463"))),
		J_END_OBJ_DECL
	));

	QVERIFY(jis_object(simpleObject));

	try {
		jvalue_ref jstr, jnum;

		GET_CHILD(jstr, String, simpleObject, "abc");
		VAL_STR(jstr, "def");

		GET_CHILD(jnum, Number, simpleObject, "def");
		VAL_NUM_OK(int32_t, jnum, 5463);
	} catch (TestFailure /*f*/) {
		return;
	}
}

void TestDOM::testObjectComplicated()
{
	// sanity check that assumptions about limits of double storage
	// are correct
	static const int64_t maxDblPrecision = 0x1FFFFFFFFFFFFFLL;
	static const int64_t minDblPrecision = -0x1FFFFFFFFFFFFFLL;
	static const int64_t positiveOutsideDblPrecision = maxDblPrecision + 2; // +1 doesn't work because it's the same (it truncates a 0)
	static const int64_t negativeOutsideDblPrecision = minDblPrecision - 2; // +1 doesn't work because it's the same (it truncates a 0)
	static const int64_t maxInt32 = std::numeric_limits<int32_t>::max();
	static const int64_t minInt32 = std::numeric_limits<int32_t>::min();
	static const char weirdString[] = "long and complicated \" string ' with \" $*U@*(&#(@*&";
	static const char veryLargeNumber[] = "645458489754321564894654151561684894456464513215648946543132189489461321684.2345646544e509";

	{
		double check;

		check = (double)maxDblPrecision;
		QVERIFY((int64_t)check == maxDblPrecision);

		check = (double)minDblPrecision;
		QVERIFY((int64_t)check == minDblPrecision);

		check = (double)(positiveOutsideDblPrecision);
		QVERIFY((int64_t)check != positiveOutsideDblPrecision);

		check = (double)(negativeOutsideDblPrecision);
		QVERIFY((int64_t)check != negativeOutsideDblPrecision);
	}

	QVERIFY(std::numeric_limits<double>::has_quiet_NaN);
	QVERIFY(std::numeric_limits<double>::has_signaling_NaN);

	// unfortunately, C++ doesn't allow us to use J_CSTR_TO_JVAL which is what I would use
	// for string literals under C.
	jvalue_ref complexObject = manage (jobject_create_var(
		// J_CSTR_TO_JVAL or J_CSTR_TO_JVAL is interchangeable only if you use string literals.
		// J_CSTR_TO_JVAL is going to be faster as your string gets larger
		jkeyval(J_CSTR_TO_JVAL("bool1"), jboolean_create(true)),
		jkeyval(J_CSTR_TO_JVAL("bool2"), jboolean_create(false)),
		jkeyval(J_CSTR_TO_JVAL("numi32_1"), jnumber_create_i32(0)),
		jkeyval(J_CSTR_TO_JVAL("numi32_2"), jnumber_create_i32(-50)),
		jkeyval(J_CSTR_TO_JVAL("numi32_3"), jnumber_create_i32(12345323)),
		jkeyval(J_CSTR_TO_JVAL("numi64_1"), jnumber_create_i64(maxInt32 + 1)),
		jkeyval(J_CSTR_TO_JVAL("numi64_2"), jnumber_create_i64(minInt32 - 1)),
		jkeyval(J_CSTR_TO_JVAL("numi64_3"), jnumber_create_i64(0)),
		jkeyval(J_CSTR_TO_JVAL("numi64_4"), jnumber_create_i64(maxDblPrecision)),
		jkeyval(J_CSTR_TO_JVAL("numi64_5"), jnumber_create_i64(minDblPrecision)),
		jkeyval(J_CSTR_TO_JVAL("numi64_6"), jnumber_create_i64(positiveOutsideDblPrecision)),
		jkeyval(J_CSTR_TO_JVAL("numi64_7"), jnumber_create_i64(negativeOutsideDblPrecision)),
		jkeyval(J_CSTR_TO_JVAL("numf64_1"), jnumber_create_f64(0.45642156489)),
		jkeyval(J_CSTR_TO_JVAL("numf64_2"), jnumber_create_f64(-54897864.14)),
		jkeyval(J_CSTR_TO_JVAL("numf64_3"), jnumber_create_f64(-54897864)),
		jkeyval(J_CSTR_TO_JVAL("numf64_4"), jnumber_create_f64(std::numeric_limits<double>::infinity())),
		jkeyval(J_CSTR_TO_JVAL("numf64_5"), jnumber_create_f64(-std::numeric_limits<double>::infinity())),
		jkeyval(J_CSTR_TO_JVAL("numf64_6"), jnumber_create_f64(-std::numeric_limits<double>::quiet_NaN())),
		jkeyval(J_CSTR_TO_JVAL("str1"), jnull()),
		jkeyval(J_CSTR_TO_JVAL("str2"), jnull()),
		jkeyval(J_CSTR_TO_JVAL("str3"), jstring_empty()),
		jkeyval(J_CSTR_TO_JVAL("str4"), J_CSTR_TO_JVAL("foo")),
		jkeyval(J_CSTR_TO_JVAL("str5"), J_CSTR_TO_JVAL(weirdString)),
		jkeyval(J_CSTR_TO_JVAL("obj1"),
				jobject_create_var(
						jkeyval(J_CSTR_TO_JVAL("num_1"), jnumber_create(J_CSTR_TO_BUF("64.234"))),
						jkeyval(J_CSTR_TO_JVAL("num_2"), jnumber_create(J_CSTR_TO_BUF(veryLargeNumber))),
						J_END_OBJ_DECL
				)
		),
		J_END_OBJ_DECL
	));

	jvalue_ref jbool, jnum, jstr, jobj;

	GET_CHILD(jbool, Bool, complexObject, "bool1");
	VAL_BOOL_AS_BOOL(jbool, true);

	GET_CHILD(jbool, Bool, complexObject, "bool2");
	VAL_BOOL_AS_BOOL(jbool, false);

	GET_CHILD(jnum, Number, complexObject, "numi32_1");
	VAL_NUM_OK(int32_t, jnum, 0);

	GET_CHILD(jnum, Number, complexObject, "numi32_2");
	VAL_NUM_OK(int32_t, jnum, -50);

	GET_CHILD(jnum, Number, complexObject, "numi32_3");
	VAL_NUM_OK(int32_t, jnum, 12345323);

	GET_CHILD(jnum, Number, complexObject, "numi64_1");
	VAL_NUM(int32_t, jnum, std::numeric_limits<int32_t>::max(), CONV_POSITIVE_OVERFLOW);
	VAL_NUM_OK(int64_t, jnum, maxInt32 + 1);

	GET_CHILD(jnum, Number, complexObject, "numi64_2");
	VAL_NUM(int32_t, jnum, std::numeric_limits<int32_t>::min(), CONV_NEGATIVE_OVERFLOW);
	VAL_NUM_OK(int64_t, jnum, minInt32 - 1);

	GET_CHILD(jnum, Number, complexObject, "numi64_3");
	VAL_NUM_OK(int64_t, jnum, 0);

	GET_CHILD(jnum, Number, complexObject, "numi64_4");
	VAL_NUM_OK(int64_t, jnum, maxDblPrecision);
	VAL_NUM_OK(double, jnum, (double)maxDblPrecision);

	GET_CHILD(jnum, Number, complexObject, "numi64_5");
	VAL_NUM_OK(int64_t, jnum, minDblPrecision);
	VAL_NUM_OK(double, jnum, (double)minDblPrecision);

	GET_CHILD(jnum, Number, complexObject, "numi64_6");
	VAL_NUM_OK(int64_t, jnum, positiveOutsideDblPrecision);
	VAL_NUM(double, jnum, (double)positiveOutsideDblPrecision, CONV_PRECISION_LOSS);

	GET_CHILD(jnum, Number, complexObject, "numi64_7");
	VAL_NUM_OK(int64_t, jnum, negativeOutsideDblPrecision);
	VAL_NUM(double, jnum, (double)negativeOutsideDblPrecision, CONV_PRECISION_LOSS);

	GET_CHILD(jnum, Number, complexObject, "numf64_1");
	VAL_NUM_OK(double, jnum, 0.45642156489);
	VAL_NUM(int64_t, jnum, 0, CONV_PRECISION_LOSS);

	GET_CHILD(jnum, Number, complexObject, "numf64_2");
	VAL_NUM_OK(double, jnum, -54897864.14);
	VAL_NUM(int64_t, jnum, -54897864, CONV_PRECISION_LOSS);

	GET_CHILD(jnum, Number, complexObject, "numf64_3");
	VAL_NUM_OK(double, jnum, -54897864);
	VAL_NUM_OK(int64_t, jnum, -54897864);


	GET_CHILD(jnum, Null, complexObject, "numf64_4"); // + inf
	GET_CHILD(jnum, Null, complexObject, "numf64_5"); // - inf
	GET_CHILD(jnum, Null, complexObject, "numf64_6"); // NaN

	GET_CHILD(jstr, Null, complexObject, "str1");
	GET_CHILD(jstr, Null, complexObject, "str2");

	GET_CHILD(jstr, String, complexObject, "str3");
	VAL_STR(jstr, "");

	GET_CHILD(jstr, String, complexObject, "str4");
	VAL_STR(jstr, "foo");

	GET_CHILD(jstr, String, complexObject, "str5");
	VAL_STR(jstr, weirdString);

	GET_CHILD(jobj, Object, complexObject, "obj1");
	GET_CHILD(jnum, Number, jobj, "num_1");
	VAL_NUM(int64_t, jnum, 64, CONV_PRECISION_LOSS);
	VAL_NUM_OK(double, jnum, 64.234);

	GET_CHILD(jnum, Number, jobj, "num_2");
	VAL_NUM(int64_t, jnum, std::numeric_limits<int64_t>::max(), CONV_POSITIVE_OVERFLOW | CONV_PRECISION_LOSS);
	VAL_NUM(double, jnum, std::numeric_limits<double>::infinity(), CONV_POSITIVE_OVERFLOW);
	VAL_NUM_OK(raw_buffer, jnum, J_CSTR_TO_BUF(veryLargeNumber));
}

void TestDOM::testObjectPut()
{
	jvalue_ref obj = manage(jobject_create());
	jvalue_ref val;

	// name collision
	jobject_put(obj, J_CSTR_TO_JVAL("test1"), jnumber_create_i32(5));
	jobject_put(obj, J_CSTR_TO_JVAL("test1"), J_CSTR_TO_JVAL("test2"));

	// replacement of object
	// valgrind will fail if this was done improperly
	val = jobject_get(obj, J_CSTR_TO_BUF("test1"));
	QCOMPARE(jis_string(val), true);

	// should be the same pointer since I used a 0-copy string
	// (assuming the c-compiler interns strings)
	QCOMPARE(jstring_get_fast(val).m_str, "test2");
}

void TestDOM::testArraySimple()
{
	jvalue_ref simple_arr = manage(jarray_create_var(NULL,
			J_CSTR_TO_JVAL("index 0"),
			jnumber_create(J_CSTR_TO_BUF("1")),
			jnumber_create_i32(2),
			jboolean_create(false),
			jboolean_create(true),
			jnull(),
			J_CSTR_TO_JVAL(""),
			jnumber_create_f64(7),
			J_END_ARRAY_DECL));
	jvalue_ref val;

	QCOMPARE(jarray_size(simple_arr), (ssize_t)8);

	GET_CHILD(val, String, simple_arr, 0);
	VAL_STR(val, "index 0");
	VAL_BOOL(val, true, TJSTR);

	GET_CHILD(val, Number, simple_arr, 1);
	VAL_NUM_OK(raw_buffer, val, J_CSTR_TO_BUF("1"));
	VAL_NUM_OK(int32_t, val, 1);
	VAL_BOOL(val, true, TJNUM);

	GET_CHILD(val, Number, simple_arr, 2);
	VAL_NUM_OK(int32_t, val, 2);

	GET_CHILD(val, Bool, simple_arr, 3);
	VAL_BOOL_AS_BOOL(val, false);

	GET_CHILD(val, Bool, simple_arr, 4);
	VAL_BOOL_AS_BOOL(val, true);

	GET_CHILD(val, Null, simple_arr, 5);
	QCOMPARE(val, jnull());
	VAL_BOOL(val, false, TJNULL);

	GET_CHILD(val, String, simple_arr, 6);
	VAL_STR(val, "");
	VAL_BOOL(val, false, TJSTR);

	GET_CHILD(val, Number, simple_arr, 7);
	VAL_NUM_OK(int32_t, val, 7);
}

void TestDOM::testArrayComplicated()
{
	jvalue_ref arr = manage(jarray_create(NULL));
	jvalue_ref child;

	QVERIFY(jis_array(arr));

	for (int32_t i = 0; i < 100; i++) {
		jarray_append(arr, jnumber_create_i32(i));
		QCOMPARE(jarray_size(arr), (ssize_t) i + 1);
		GET_CHILD(child, Number, arr, i);
		VAL_NUM_OK(int32_t, child, i);
		if (i > 20)
			QVERIFY(jis_number(getChildNumber(arr, 20)));
	}

	QCOMPARE(jarray_size(arr), (ssize_t)100);
	QVERIFY(jis_number(getChildNumber(arr, 20)));

	for (int32_t i = 0; i < 100; i++) {
		int32_t element;
		GET_CHILD(child, Number, arr, i);
		QVERIFY(jis_number(child));
		QCOMPARE(jnumber_get_i32(child, &element), (ConversionResultFlags)CONV_OK);
		QCOMPARE(element, i);
	}
}

void TestDOM::testStringSimple_data()
{
	QTest::addColumn<QByteArray>("string");
	QTest::addColumn<QByteArray>("utf8Result"); // the result when not supplying the length to the string
	QTest::addColumn<QByteArray>("rawResult");

#define QB_STR(str) str, sizeof(str) - 1
	QTest::newRow("regular ascii string") <<
		QByteArray::fromRawData(QB_STR("foo bar")) <<
		QByteArray::fromRawData(QB_STR("foo bar")) <<
		QByteArray::fromRawData(QB_STR("foo bar"));

	QTest::newRow("ascii string w/ embedded null") <<
		QByteArray::fromRawData(QB_STR("foo bar. the quick brown\0 fox jumped over the lazy dog.")) <<
		QByteArray::fromRawData(QB_STR("foo bar. the quick brown")) <<
		QByteArray::fromRawData(QB_STR("foo bar. the quick brown\0 fox jumped over the lazy dog."));
#undef QB_STR
}

void TestDOM::testStringSimple()
{
	QFETCH(QByteArray, string);
	QFETCH(QByteArray, utf8Result);
	QFETCH(QByteArray, rawResult);

	jvalue_ref str1 = manage(jstring_create(string.constData()));
	QVERIFY(jis_string(str1));
	VAL_STR(str1, utf8Result);

	jvalue_ref str2 = manage(jstring_create_nocopy(j_str_to_buffer(string.constData(), string.size())));
	VAL_STR(str2, rawResult);

	if (utf8Result != rawResult) {
		QVERIFY(!jstring_equal2(str1, j_str_to_buffer(rawResult.constData(), rawResult.size())));
		QVERIFY(!jstring_equal2(str2, j_str_to_buffer(utf8Result.constData(), utf8Result.size())));
	}
}

static volatile bool *flagToChange = NULL;
static void strdealloc(void *str)
{
	free(str);
	if (flagToChange != NULL) {
		*flagToChange = true;
		flagToChange = NULL;
	} else {
		qWarning("Deallocation routine used without setting the flag to notify of free");
	}
}

void TestDOM::testStringDealloc()
{
	volatile bool dealloced = false;
#define str "the quick brown fox jumped over the lazy dog"
	QCOMPARE(sizeof(str), (size_t)45);
	QCOMPARE(strlen(str), (size_t)44);
	raw_buffer srcString = J_CSTR_TO_BUF(str);
	QCOMPARE(srcString.m_len, (long)44);

	char *dynstr = (char *)calloc(srcString.m_len + 1, sizeof(char));
	ptrdiff_t dynstrlen = strlen(strncpy(dynstr, srcString.m_str, srcString.m_len));
	QCOMPARE(dynstrlen, (ptrdiff_t)srcString.m_len);

	flagToChange = &dealloced;

	jvalue_ref created_string = jstring_create_nocopy_full(j_str_to_buffer(dynstr, dynstrlen), strdealloc);
	jvalue_ref old_ref = created_string;
	QVERIFY(dealloced == false);
	j_release(&created_string);
#ifndef NDEBUG
	// we might not compile with DEBUG_FREED_POINTERS even in non-release mode
	QVERIFY(created_string == (void *)0xdeadbeef || created_string == old_ref);
#else
	QVERIFY(created_string == old_ref);
#endif
	QVERIFY(dealloced == true);
#undef str
}

void TestDOM::testInteger32Simple()
{
}

void TestDOM::testInteger320()
{

}

void TestDOM::testInteger32Limits()
{

}

void TestDOM::testInteger64Simple()
{

}

void TestDOM::testInteger640()
{

}

void TestDOM::testInteger64Limits()
{

}

void TestDOM::testDouble0()
{

}

void TestDOM::testDoubleInfinities()
{

}

void TestDOM::testDoubleNaN()
{

}

void TestDOM::testDoubleFromInteger()
{

}

void TestDOM::testBoolean_data()
{
	QTest::addColumn<jvalue_ref>("jvalToTest");
	QTest::addColumn<ConversionResultFlags>("expectedConvResult");
	QTest::addColumn<bool>("expectedValue");

	QTest::newRow("true bool") <<
		manage(jboolean_create(true)) <<
		(unsigned int)CONV_OK <<
		true;

	QTest::newRow("false bool") <<
		manage(jboolean_create(false)) <<
		(unsigned int)CONV_OK <<
		false;

	QTest::newRow("null") <<
		jnull() <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		false;

#ifndef NDEBUG
	// __attribute__((non_null)) might let the compiler optimize out
	// the check.  this might also break non-gcc compilers
	QTest::newRow("undefined") <<
		(jvalue_ref)NULL <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		false;
#endif

	QTest::newRow("empty object") <<
		manage(jobject_create()) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non empty object") <<
		manage(jobject_create_var(jkeyval(J_CSTR_TO_JVAL("nothing"), jnull()), J_END_OBJ_DECL)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("empty array") <<
		manage(jarray_create(NULL)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non empty array (single null element)") <<
		manage(jarray_create_var(NULL, jnull(), J_END_ARRAY_DECL)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non empty array (single false element)") <<
		manage(jarray_create_var(NULL, jboolean_create(false), J_END_ARRAY_DECL)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("empty string") <<
		manage(J_CSTR_TO_JVAL("")) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("non-empty string") <<
		manage(J_CSTR_TO_JVAL("false")) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("0 number") <<
		manage(jnumber_create_i64(0)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("0 number 2") <<
		manage(jnumber_create_f64(0)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("0 number 3") <<
		manage(jnumber_create_unsafe(J_CSTR_TO_BUF("0"), NULL)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("0 number 4") <<
		manage(jnumber_create_unsafe(J_CSTR_TO_BUF("0.0"), NULL)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("non-0 number") <<
		manage(jnumber_create_unsafe(J_CSTR_TO_BUF("124"), NULL)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non-0 number 1") <<
		manage(jnumber_create_i64(1)) <<
		(unsigned int)CONV_NOT_A_BOOLEAN <<
		true;
}

void TestDOM::testBoolean()
{
	QFETCH(jvalue_ref, jvalToTest);
	QFETCH(ConversionResultFlags, expectedConvResult);
	QFETCH(bool, expectedValue);

	ConversionResultFlags actualConversion;
	bool actualAsBool;

	actualConversion = jboolean_get(jvalToTest, &actualAsBool);
	QCOMPARE(actualAsBool, expectedValue);
	QCOMPARE(actualConversion, expectedConvResult);
}

/***************************************************** HELPER ROUTINES ***************************************************/


void TestDOM::validateBool(jvalue_ref jbool, bool expected, JType expectedType)
{
	bool actual;

	switch (expectedType) {
		case TJOBJ:
			MY_VERIFY(jis_object(jbool));
			break;
		case TJARR:
			MY_VERIFY(jis_array(jbool));
			break;
		case TJSTR:
			MY_VERIFY(jis_string(jbool));
			break;
		case TJNUM:
			MY_VERIFY(jis_number(jbool));
			break;
		case TJNULL:
			MY_VERIFY(jis_null(jbool));
			break;
		default:
			MY_COMPARE(jboolean_get(jbool, &actual), (ConversionResultFlags)CONV_OK);
			MY_COMPARE(actual, expected);
			return;
	}

	MY_COMPARE(jboolean_get(jbool, &actual), (ConversionResultFlags)CONV_NOT_A_BOOLEAN);
	MY_COMPARE(actual, expected);
}

void TestDOM::validateString(jvalue_ref jstr, QByteArray expected)
{
	raw_buffer actualBuffer;
	QByteArray actualBufferWrapper;

	MY_VERIFY(jis_string(jstr));
	actualBuffer = jstring_get_fast(jstr);
	actualBufferWrapper = QByteArray::fromRawData(actualBuffer.m_str, actualBuffer.m_len);

	MY_COMPARE(actualBufferWrapper, expected);
	MY_VERIFY(jstring_equal2(jstr, (raw_buffer){expected.constData(), expected.size()}));
}

template <>
void TestDOM::validateNumber<int32_t>(jvalue_ref jnum, int32_t expected, ConversionResultFlags conversionResult)
{
	int32_t actual;

	MY_VERIFY(jis_number(jnum));
	try {
		MY_COMPARE(jnumber_get_i32(jnum, &actual), conversionResult);
	} catch(...) {
		std::cerr << "Error [" << actual << "!=" << expected << "] is: '" << result_str(jnumber_get_i32(jnum, &actual)) << "' but expecting '" << result_str(conversionResult) << "'" << std::endl;
		throw;
	}
	MY_COMPARE(actual, expected);
}

template <>
void TestDOM::validateNumber<int64_t>(jvalue_ref jnum, int64_t expected, ConversionResultFlags conversionResult)
{
	int64_t actual;

	MY_VERIFY(jis_number(jnum));
	try {
		MY_COMPARE(jnumber_get_i64(jnum, &actual), conversionResult);
	} catch(...) {
		std::cerr << "Error [" << actual << "!=" << expected << "] is: '" << result_str(jnumber_get_i64(jnum, &actual)) << "' but expecting '" << result_str(conversionResult) << "'" << std::endl;
		throw;
	}
	MY_COMPARE(actual, expected);
}

template <>
void TestDOM::validateNumber<double>(jvalue_ref jnum, double expected, ConversionResultFlags conversionResult)
{
	double actual;

	MY_VERIFY(jis_number(jnum));
	try {
		MY_COMPARE(jnumber_get_f64(jnum, &actual), conversionResult);
	} catch(...) {
		std::cerr << "Error [" << actual << "!=" << expected << "] is: '" << result_str(jnumber_get_f64(jnum, &actual)) << "' but expecting '" << result_str(conversionResult) << "'" << std::endl;
		throw;
	}

	MY_VERIFY(actual == expected);	// unfortunately QTest::qCompare does a fuzzy comparison on the number
}

template <>
void TestDOM::validateNumber<raw_buffer>(jvalue_ref jnum, raw_buffer expected, ConversionResultFlags conversionResult)
{
	raw_buffer actual;

	MY_VERIFY(conversionResult == CONV_OK || conversionResult == CONV_GENERIC_ERROR || conversionResult == CONV_BAD_ARGS || conversionResult == CONV_NOT_A_RAW_NUM);
	MY_VERIFY(jis_number(jnum));
	MY_COMPARE(jnumber_get_raw(jnum, &actual), (ConversionResultFlags) conversionResult);
	MY_COMPARE(actual.m_len, expected.m_len);
	MY_COMPARE(memcmp(actual.m_str, expected.m_str, expected.m_len), 0);
}

jvalue_ref TestDOM::getChild(jvalue_ref obj, std::string key)
{
	jvalue_ref child;

	MY_VERIFY(jis_object(obj));
	MY_VERIFY(jobject_get_exists(obj, j_str_to_buffer(key.c_str(), key.size()), &child));

	return child;
}

jvalue_ref TestDOM::getChild(jvalue_ref arr, size_t i)
{
	jvalue_ref child;

	MY_VERIFY(jis_array(arr));
	child = jarray_get(arr, i);

	return child;
}

jvalue_ref TestDOM::getChildObject(jvalue_ref obj, std::string key)
{
	jvalue_ref child = getChild(obj, key);
	MY_VERIFY(jis_object(child));

	return child;
}

jvalue_ref TestDOM::getChildArray(jvalue_ref obj, std::string key)
{
	jvalue_ref child = getChild(obj, key);
	MY_VERIFY(jis_array(child));

	return child;
}

jvalue_ref TestDOM::getChildString(jvalue_ref obj, std::string key)
{
	jvalue_ref child = getChild(obj, key);
	MY_VERIFY(jis_string(child));

	return child;
}

jvalue_ref TestDOM::getChildNumber(jvalue_ref obj, std::string key)
{
	jvalue_ref child = getChild(obj, key);
	MY_VERIFY(jis_number(child));

	return child;
}

jvalue_ref TestDOM::getChildBool(jvalue_ref obj, std::string key)
{
	jvalue_ref child = getChild(obj, key);
	MY_COMPARE(jboolean_get(child, NULL), (ConversionResultFlags)CONV_OK);

	return child;
}

jvalue_ref TestDOM::getChildNull(jvalue_ref obj, std::string key)
{
	jvalue_ref child = getChild(obj, key);
	MY_VERIFY(jis_null(child));

	return child;
}

jvalue_ref TestDOM::getChildObject(jvalue_ref arr, size_t i)
{
	jvalue_ref child = getChild(arr, i);
	MY_VERIFY(jis_object(child));

	return child;
}

jvalue_ref TestDOM::getChildArray(jvalue_ref arr, size_t i)
{
	jvalue_ref child = getChild(arr, i);
	MY_VERIFY(jis_array(child));

	return child;
}

jvalue_ref TestDOM::getChildString(jvalue_ref arr, size_t i)
{
	jvalue_ref child = getChild(arr, i);
	MY_VERIFY(jis_string(child));

	return child;
}

jvalue_ref TestDOM::getChildNumber(jvalue_ref arr, size_t i)
{
	jvalue_ref child = getChild(arr, i);
	MY_VERIFY(jis_number(child));

	return child;
}

jvalue_ref TestDOM::getChildBool(jvalue_ref arr, size_t i)
{
	jvalue_ref child = getChild(arr, i);
	MY_COMPARE(jboolean_get(child, NULL), (ConversionResultFlags)CONV_OK);

	return child;
}

jvalue_ref TestDOM::getChildNull(jvalue_ref arr, size_t i)
{
	jvalue_ref child = getChild(arr, i);
	MY_VERIFY(jis_null(child));

	return child;
}

/***************************************************** HELPER ROUTINES ***************************************************/

}
}

QTEST_APPLESS_MAIN(pjson::testc::TestDOM);
