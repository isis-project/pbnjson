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

#include <pbnjson.hpp>

#include "../QBacktrace.h"

Q_DECLARE_METATYPE(pbnjson::JValue);
Q_DECLARE_METATYPE(ConversionResultFlags);
Q_DECLARE_METATYPE(bool);

using namespace std;

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
namespace testcxx {

namespace pj = pbnjson;

class TestFailure
{

};

template <class T>
void TestDOM::validateNumber(const pj::JValue& jnum, T expected, ConversionResultFlags conversionResult)
{
	T actual;
	ConversionResultFlags conversion = jnum.asNumber(actual);

	MY_VERIFY(jnum.isNumber());
	try {
		MY_COMPARE(conversion, conversionResult);
	} catch(...) {
		std::cerr << "Error [" << actual << "!=" << expected << "] is: '" << result_str(conversion) << "' but expecting '" << result_str(conversionResult) << "'" << std::endl;
		throw;
	}
	MY_COMPARE(actual, expected);
}

template <>
void TestDOM::validateNumber<double>(const pj::JValue& jnum, double expected, ConversionResultFlags conversionResult);
template <>
void TestDOM::validateNumber<std::string>(const pj::JValue& jnum, std::string expected, ConversionResultFlags conversionResult);

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
	//testValist(jkeyval((const pj::JValue&)0x1, (const pj::JValue&)0x2), jkeyval(NULL, NULL));
}

TestDOM::~TestDOM()
{

}

void TestDOM::initTestCase()
{

}

void TestDOM::init()
{
}

void TestDOM::cleanup()
{
}

void TestDOM::cleanupTestCase()
{

}

ssize_t objectSize(const pbnjson::JValue& value)
{
	ssize_t result = 0;
	for (pbnjson::JValue::ObjectConstIterator i = value.begin(); i != value.end(); i++)
		result++;
	return result;
}

void TestDOM::testParserObjectSimple()
{
	pj::JDomParser parser(NULL);
	pj::JValue parsed;
	pj::JSchemaFragment schema("{}");

	QVERIFY(parser.parse("{}", schema));

	parsed = parser.getDom();
	QVERIFY(parsed.isObject());
	QVERIFY(parsed.begin() == parsed.end());
	QVERIFY(!(parsed.begin() != parsed.end()));
	QCOMPARE(objectSize(parsed), (ssize_t)0);
}

void TestDOM::testParserObjectComplex()
{
	pj::JValue parsed, value;
	pj::JDomParser parser(NULL);
	pj::JSchemaFragment schema("{}");

	std::string dom1Str = "{\"key1\" : null, \"key2\" : \"str\", \"key3\" : 506 }";
	QVERIFY(parser.parse(dom1Str, schema));
	parsed = parser.getDom();
	QCOMPARE(objectSize(parsed), (ssize_t)3);

	value = parsed["key1"];
	QVERIFY(value.isNull());

	value = parsed["key2"];
	QVERIFY(value.isString());
	QCOMPARE(value.asString(), std::string("str"));

	value = parsed["key3"];
	QVERIFY(value.isNumber());
	QCOMPARE(value.asNumber<std::string>(), std::string("506"));
	QCOMPARE(value.asNumber<int64_t>(), (int64_t)506);

#define OB "{"
#define OE "}"
#define AB "["
#define AE "]"

#define STR(name) "\"" name "\""
#define KEY(name) STR(name) ":"
#define E(element) element ","

	std::string dom2Str =
			OB
				KEY("key1") OB
					KEY("key1") OB
							KEY("foo") AB
									E(STR("bar"))
									E("5")
									"null"
							AE ","
							KEY("bar") "false,"
							KEY("test") STR("abcd")
					OE
				OE
			OE
	;

	QVERIFY(parser.parse(dom2Str, schema));

	parsed = parser.getDom();
	QVERIFY(parsed.isObject());
	QCOMPARE(objectSize(parsed), (ssize_t)1);

	QVERIFY(parsed["key1"].isObject());
	QCOMPARE(objectSize(parsed["key1"]), (ssize_t)1);

	QVERIFY(parsed["key1"]["key1"].isObject());
	QCOMPARE(objectSize(parsed["key1"]["key1"]), (ssize_t)3);

	QVERIFY(parsed["key1"]["key1"]["foo"].isArray());
	QCOMPARE(parsed["key1"]["key1"]["foo"].arraySize(), (ssize_t)3);

	// the (ssize_t) cast is only necessary for OSX 10.5 which uses an old compiler
	QVERIFY(parsed["key1"]["key1"]["foo"][0].isString());
	QCOMPARE(parsed["key1"]["key1"]["foo"][0].asString(), std::string("bar"));

	QVERIFY(parsed["key1"]["key1"]["foo"][1].isNumber());
	QCOMPARE(parsed["key1"]["key1"]["foo"][1].asNumber<int32_t>(), 5);

	QVERIFY(parsed["key1"]["key1"]["foo"][2].isNull());

	QVERIFY(parsed["key1"]["key1"]["bar"].isBoolean());
	QCOMPARE(parsed["key1"]["key1"]["bar"].asBool(), false);

	QVERIFY(parsed["key1"]["key1"]["test"].isString());
	QCOMPARE(parsed["key1"]["key1"]["test"].asString(), std::string("abcd"));

	// this test was causing a memory leak (otherwise it is a duplicate of above)
	// it fails because of the null (turns into ,null,], - the ',' after the null is illegal).
	std::string dom3Str =
			OB
				KEY("key1") OB
					KEY("key1") OB
							KEY("foo") AB
									E(STR("bar"))
									E("5")
									E("null")
							AE ","
							KEY("bar") "false,"
							KEY("test") STR("abcd")
					OE
				OE
			OE
	;
	QCOMPARE(parser.parse(dom3Str, schema), false);
#undef OB
#undef OE
#undef AB
#undef AE
#undef STR
#undef KEY
#undef E
}

void TestDOM::testParserArray()
{

}

void TestDOM::testParserComplex()
{

}

void TestDOM::testObjectSimple()
{
	std::string simpleObjectAsStr;
	pj::JValue simpleObject = pj::Object();
	pj::JSchemaFragment schema("{}");
	pj::JGenerator generator(NULL);

	simpleObject.put("abc", "def");
	QVERIFY(simpleObject.hasKey("abc"));
	QVERIFY(simpleObject["abc"].isString());
	QCOMPARE(simpleObject["abc"].asString(), std::string("def"));

	simpleObject.put("def", pj::NumericString("5463"));
	QVERIFY(simpleObject.hasKey("def"));
	QVERIFY(simpleObject["def"].isNumber());
	QCOMPARE(simpleObject["def"].asNumber<int32_t>(), 5463);

	QVERIFY(generator.toString(simpleObject, schema, simpleObjectAsStr));
	QCOMPARE(simpleObjectAsStr.c_str(), "{\"abc\":\"def\",\"def\":5463}");

	QVERIFY(simpleObject.isObject());

	try {
		pj::JValue jstr, jnum;

		GET_CHILD(jstr, String, simpleObject, "abc");
		VAL_STR(jstr, "def");

		GET_CHILD(jnum, Number, simpleObject, "def");
		VAL_NUM_OK(int32_t, jnum, 5463);
	} catch (TestFailure f) {
		return;
	}
}

void TestDOM::testObjectIterator()
{
	const char *inputStr = "{\"a\":{\"b\":\"c\", \"d\":5}}";
	pj::JDomParser parser;
	pj::JSchemaFragment schema("{}");
	pj::JValue parsed;
	pj::JValue::ObjectIterator i;

	QVERIFY(parser.parse(inputStr, schema));
	parsed = parser.getDom();

	QVERIFY(parsed.isObject());
	QVERIFY(parsed.hasKey("a"));
	QVERIFY(parsed["a"].isObject());
	QVERIFY(parsed["a"].hasKey("b"));
	QVERIFY(parsed["a"].hasKey("d"));
	QVERIFY(parsed["a"]["b"].isString());
	QCOMPARE(parsed["a"]["b"].asString(), string("c"));
	QVERIFY(parsed["a"]["d"].isNumber());
	QCOMPARE(parsed["a"]["d"].asNumber<int>(), 5);

	i = parsed.begin();
	QVERIFY(i != parsed.end());
	QVERIFY((*i).first.isString());
	QCOMPARE((*i).first.asString(), string("a"));
	QVERIFY((*i).second.isObject());

	i = (*i).second.begin();
	QVERIFY(i == parsed["a"].begin());
	QVERIFY(i != parsed["a"].end());
	QVERIFY((*i).first.isString());
	QCOMPARE((*i).first.asString(), string("b"));
	QVERIFY((*i).second.isString());
	QCOMPARE((*i).second.asString(), string("c"));

	i++;
	QVERIFY((*i).first.isString());
	QCOMPARE((*i).first.asString(), string("d"));
	QVERIFY((*i).second.isNumber());
	QCOMPARE((*i).second.asNumber<int>(), 5);
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
	static const char *weirdString = "long and complicated \" string ' with \" $*U@*(&#(@*&";
	static const char *veryLargeNumber = "645458489754321564894654151561684894456464513215648946543132189489461321684.2345646544e509";

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
	pj::JValue obj1Embedded = pj::Object();
	obj1Embedded.put("num_1", pj::NumericString("64.234"));
	obj1Embedded.put("num_2", pj::NumericString(veryLargeNumber));

	pj::JValue complexObject = pj::Object();
	complexObject.put("bool1", true);
	complexObject.put("bool2", false);
	complexObject.put("numi32_1", 0),
	complexObject.put("numi32_2", -50);
	complexObject.put("numi32_3", 12345323);
	complexObject.put("numi64_1", maxInt32 + 1);
	complexObject.put("numi64_2", minInt32 - 1);
	complexObject.put("numi64_3", 0);
	complexObject.put("numi64_4", maxDblPrecision);
	complexObject.put("numi64_5", minDblPrecision);
	complexObject.put("numi64_6", positiveOutsideDblPrecision);
	complexObject.put("numi64_7", negativeOutsideDblPrecision);
	complexObject.put("numf64_1", 0.45642156489);
	complexObject.put("numf64_2", -54897864.14);
	complexObject.put("numf64_3", -54897864);
	complexObject.put("numf64_4", std::numeric_limits<double>::infinity());
	complexObject.put("numf64_5", -std::numeric_limits<double>::infinity());
	complexObject.put("numf64_6", -std::numeric_limits<double>::quiet_NaN());
	complexObject.put("str1", pj::JValue());
	complexObject.put("str2", pj::JValue());
	complexObject.put("str3", "");
	complexObject.put("str4", "foo");
	complexObject.put("str5", weirdString);
	complexObject.put("obj1", obj1Embedded);

	pj::JValue jbool, jnum, jstr, jobj;

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

	GET_CHILD(jstr, String, complexObject, "str4"); // NaN
	VAL_STR(jstr, "foo");

	GET_CHILD(jstr, String, complexObject, "str5"); // NaN
	VAL_STR(jstr, weirdString);

	GET_CHILD(jobj, Object, complexObject, "obj1");
	GET_CHILD(jnum, Number, jobj, "num_1");
	VAL_NUM(int64_t, jnum, 64, CONV_PRECISION_LOSS);
	VAL_NUM_OK(double, jnum, 64.234);

	GET_CHILD(jnum, Number, jobj, "num_2");
	VAL_NUM(int64_t, jnum, std::numeric_limits<int64_t>::max(), CONV_POSITIVE_OVERFLOW | CONV_PRECISION_LOSS);
	VAL_NUM(double, jnum, std::numeric_limits<double>::infinity(), CONV_POSITIVE_OVERFLOW);
	VAL_NUM_OK(std::string, jnum, veryLargeNumber);
}

void TestDOM::testObjectPut()
{
	pj::JValue obj = pj::Object();
	obj.put("abc", 5);
	obj.put("abc", "def");

	QVERIFY(obj["abc"].isString());
	QCOMPARE(obj["abc"].asString(), std::string("def"));
}

void TestDOM::testArraySimple()
{
	pj::JValue simple_arr = pj::Array();
	pj::JValue val;

	simple_arr.put(4, true);
	QCOMPARE(simple_arr.arraySize(), (ssize_t)5);

	simple_arr.put(1, pj::NumericString("1"));
	QCOMPARE(simple_arr.arraySize(), (ssize_t)5);

	simple_arr.put(2, 2);
	QCOMPARE(simple_arr.arraySize(), (ssize_t)5);

	simple_arr.put(3, false);
	QCOMPARE(simple_arr.arraySize(), (ssize_t)5);

	simple_arr.put(5, pj::JValue());
	QCOMPARE(simple_arr.arraySize(), (ssize_t)6);

	simple_arr.put(6, "");
	QCOMPARE(simple_arr.arraySize(), (ssize_t)7);

	QCOMPARE(simple_arr[(ssize_t)0], pj::JValue());
	simple_arr.put(0, "index 0");
	QCOMPARE(simple_arr.arraySize(), (ssize_t)7);

	simple_arr.put(7, 7.0);
	QCOMPARE(simple_arr.arraySize(), (ssize_t)8);

	GET_CHILD(val, String, simple_arr, 0);
	VAL_STR(val, "index 0");
	VAL_BOOL(val, true, TJSTR);

	GET_CHILD(val, Number, simple_arr, 1);
	VAL_NUM_OK(std::string, val, "1");
	VAL_NUM_OK(int32_t, val, 1);
	VAL_BOOL(val, true, TJNUM);

	GET_CHILD(val, Number, simple_arr, 2);
	VAL_NUM_OK(int32_t, val, 2);

	GET_CHILD(val, Bool, simple_arr, 3);
	VAL_BOOL_AS_BOOL(val, false);

	GET_CHILD(val, Bool, simple_arr, 4);
	VAL_BOOL_AS_BOOL(val, true);

	GET_CHILD(val, Null, simple_arr, 5);
	QCOMPARE(val, pj::JValue());
	VAL_BOOL(val, false, TJNULL);

	GET_CHILD(val, String, simple_arr, 6);
	VAL_STR(val, "");
	VAL_BOOL(val, false, TJSTR);

	GET_CHILD(val, Number, simple_arr, 7);
	VAL_NUM_OK(int32_t, val, 7);
}

void TestDOM::testArrayComplicated()
{
	pj::JValue arr = pj::Array();
	pj::JValue child;

	QVERIFY(arr.isArray());

	for (int32_t i = 0; i < 100; i++) {
		arr.append(i);
		QCOMPARE(arr.arraySize(), (ssize_t)(i + 1));
		GET_CHILD(child, Number, arr, i);
		VAL_NUM_OK(int32_t, child, i);
	}

	QCOMPARE(arr.arraySize(), (ssize_t)100);

	for (int32_t i = 0; i < 100; i++) {
		int32_t element;
		GET_CHILD(child, Number, arr, i);
		QVERIFY(child.isNumber());
		QCOMPARE(child.asNumber(element), (ConversionResultFlags)CONV_OK);
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

	pj::JValue str1(string.constData());
	QVERIFY(str1.isString());
	VAL_STR(str1, utf8Result);

	pj::JValue str2(std::string(string.constData(), string.size()));
	VAL_STR(str2, rawResult);

	if (utf8Result != rawResult) {
		QVERIFY(str1 != str2);
	}
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
	QTest::addColumn<pj::JValue>("jvalToTest");
	QTest::addColumn<ConversionResultFlags>("expectedConvResult");
	QTest::addColumn<bool>("expectedValue");

	QTest::newRow("true bool") <<
		pj::JValue(true) <<
		(ConversionResultFlags) CONV_OK <<
		true;

	QTest::newRow("false bool") <<
		pj::JValue(false) <<
		(ConversionResultFlags) CONV_OK <<
		false;

	QTest::newRow("null") <<
		pj::JValue() <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("empty object") <<
		(pj::Object()) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non empty object") <<
		(pj::Object() << pj::JValue::KeyValue("nothing", pj::JValue())) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("empty array") <<
		(pj::Array()) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non empty array (single null element)") <<
		(pj::Array() << pj::JValue()) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non empty array (single false element)") <<
		(pj::Array() << false) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("empty string") <<
		pj::JValue("") <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("non-empty string") <<
		pj::JValue("false") <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("0 number") <<
		pj::JValue((int64_t)0) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("0 number 2") <<
		pj::JValue(0.0) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("0 number 3") <<
		(pj::JValue)pj::NumericString("0") <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("0 number 4") <<
		(pj::JValue)pj::NumericString("0.0") <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		false;

	QTest::newRow("non-0 number") <<
		(pj::JValue)pj::NumericString("124") <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;

	QTest::newRow("non-0 number 1") <<
		pj::JValue((int64_t)1) <<
		(ConversionResultFlags) CONV_NOT_A_BOOLEAN <<
		true;
}

void TestDOM::testBoolean()
{
	QFETCH(pj::JValue, jvalToTest);
	QFETCH(ConversionResultFlags, expectedConvResult);
	QFETCH(bool, expectedValue);

	bool actualAsBool;

	QCOMPARE(jvalToTest.asBool(actualAsBool), expectedConvResult);
	QCOMPARE(actualAsBool, expectedValue);
}

/***************************************************** HELPER ROUTINES ***************************************************/


void TestDOM::validateBool(const pj::JValue& jbool, bool expected, JType expectedType)
{
	bool actual;

	switch (expectedType) {
		case TJOBJ:
			MY_VERIFY(jbool.isObject());
			break;
		case TJARR:
			MY_VERIFY(jbool.isArray());
			break;
		case TJSTR:
			MY_VERIFY(jbool.isString());
			break;
		case TJNUM:
			MY_VERIFY(jbool.isNumber());
			break;
		case TJNULL:
			MY_VERIFY(jbool.isNull());
			break;
		case TJBOOL:
			MY_COMPARE(jbool.asBool(actual), (ConversionResultFlags)CONV_OK);
			MY_COMPARE(actual, expected);
			return;
		default:
			// shouldn't happen
			MY_VERIFY(0 == 1);
			return;
	}

	MY_COMPARE(jbool.asBool(actual), (ConversionResultFlags)CONV_NOT_A_BOOLEAN);
	MY_COMPARE(actual, expected);
}

void TestDOM::validateString(const pj::JValue& jstr, QByteArray expected)
{
	std::string actualBuffer;
	QByteArray actualBufferWrapper;

	MY_VERIFY(jstr.isString());
	actualBuffer = jstr.asString();
	actualBufferWrapper = QByteArray::fromRawData(actualBuffer.c_str(), actualBuffer.length());

	MY_COMPARE(actualBufferWrapper, expected);
	MY_VERIFY(jstr == std::string(expected.constData(), expected.size()));
}

template <>
void TestDOM::validateNumber<double>(const pj::JValue& jnum, double expected, ConversionResultFlags conversionResult)
{
	double actual;
	ConversionResultFlags conversion = jnum.asNumber(actual);

	MY_VERIFY(jnum.isNumber());
	try {
		MY_COMPARE(conversion, conversionResult);
	} catch(...) {
		std::cerr << "Error [" << actual << "!=" << expected << "] is: '" << result_str(conversion) << "' but expecting '" << result_str(conversionResult) << "'" << std::endl;
		throw;
	}

	MY_VERIFY(actual == expected);	// unfortunately QTest::qCompare does a fuzzy comparison on the number
}

template <>
void TestDOM::validateNumber<std::string>(const pj::JValue& jnum, std::string expected, ConversionResultFlags conversionResult)
{
	std::string actual;

	MY_VERIFY(conversionResult == CONV_OK || conversionResult == CONV_GENERIC_ERROR || conversionResult == CONV_BAD_ARGS || conversionResult == CONV_NOT_A_RAW_NUM);
	MY_VERIFY(jnum.isNumber());
	MY_COMPARE(jnum.asNumber(actual), conversionResult);
	MY_COMPARE(actual, expected);
}

pj::JValue TestDOM::getChild(const pj::JValue& obj, std::string key)
{
	MY_VERIFY(obj.isObject());
	MY_VERIFY(obj.hasKey(key));

	return obj[key];
}

pj::JValue TestDOM::getChild(const pj::JValue& arr, int i)
{
	MY_VERIFY(arr.isArray());
	MY_VERIFY(i < arr.arraySize());
	return arr[i];
}

pj::JValue TestDOM::getChildObject(const pj::JValue& obj, std::string key)
{
	pj::JValue child = getChild(obj, key);
	MY_VERIFY(child.isObject());

	return child;
}

pj::JValue TestDOM::getChildArray(const pj::JValue& obj, std::string key)
{
	pj::JValue child = getChild(obj, key);
	MY_VERIFY(child.isArray());

	return child;
}

pj::JValue TestDOM::getChildString(const pj::JValue& obj, std::string key)
{
	pj::JValue child = getChild(obj, key);
	MY_VERIFY(child.isString());

	return child;
}

pj::JValue TestDOM::getChildNumber(const pj::JValue& obj, std::string key)
{
	pj::JValue child = getChild(obj, key);
	MY_VERIFY(child.isNumber());

	return child;
}

pj::JValue TestDOM::getChildBool(const pj::JValue& obj, std::string key)
{
	pj::JValue child = getChild(obj, key);
	MY_VERIFY(child.isBoolean());

	return child;
}

pj::JValue TestDOM::getChildNull(const pj::JValue& obj, std::string key)
{
	pj::JValue child = getChild(obj, key);
	MY_VERIFY(child.isNull());

	return child;
}

pj::JValue TestDOM::getChildObject(const pj::JValue& arr, size_t i)
{
	pj::JValue child = getChild(arr, i);
	MY_VERIFY(child.isObject());

	return child;
}

pj::JValue TestDOM::getChildArray(const pj::JValue& arr, size_t i)
{
	pj::JValue child = getChild(arr, i);
	MY_VERIFY(child.isArray());

	return child;
}

pj::JValue TestDOM::getChildString(const pj::JValue& arr, size_t i)
{
	pj::JValue child = getChild(arr, i);
	MY_VERIFY(child.isString());

	return child;
}

pj::JValue TestDOM::getChildNumber(const pj::JValue& arr, size_t i)
{
	pj::JValue child = getChild(arr, i);
	MY_VERIFY(child.isNumber());

	return child;
}

pj::JValue TestDOM::getChildBool(const pj::JValue& arr, size_t i)
{
	bool value;
	pj::JValue child = getChild(arr, i);
	MY_COMPARE(child.asBool(value), (ConversionResultFlags)CONV_OK);

	return child;
}

pj::JValue TestDOM::getChildNull(const pj::JValue& arr, size_t i)
{
	pj::JValue child = getChild(arr, i);
	MY_VERIFY(child.isNull());

	return child;
}

/***************************************************** HELPER ROUTINES ***************************************************/

}
}

QTEST_APPLESS_MAIN(pjson::testcxx::TestDOM);
