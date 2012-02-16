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

#include "TestRegression.h"
#include <pbnjson.hpp>
#include <QtDebug>

Q_DECLARE_METATYPE(std::string);
Q_DECLARE_FLAGS(QConversionResultFlags, ConversionResult)

namespace pjson {
	namespace testcxx {

static void toString(const pbnjson::JValue& val, std::string& result, bool shouldSucceed = true)
{
	using namespace pbnjson;

	JGenerator serializer;
	bool serialized = serializer.toString(val, JSchemaFragment("{}"), result);
	QCOMPARE(serialized, shouldSucceed);
}

TestRegression::TestRegression()
{
}

TestRegression::~TestRegression()
{
}

#define USE_PBNJSON using namespace pbnjson

#define INIT_SCHEMA_TEST_DATA       \
    USE_PBNJSON;                    \
    initSchemaTest();

void TestRegression::initSchemaTest()
{
	QTest::addColumn<std::string>("rawInput");
	QTest::addColumn<std::string>("schema");
	QTest::addColumn<bool>("inputMatchesSchema");
}

void TestRegression::initTestCase()
{
}

void TestRegression::init()
{
}

void TestRegression::cleanup()
{
}

void TestRegression::cleanupTestCase()
{
}

void TestRegression::testNOV99444_data()
{
	INIT_SCHEMA_TEST_DATA;

	std::string input;

	std::string faultySchema =
	"{"
	        "\"type\" : \"object\","
	        "\"properties\" : {"
	                "\"errorCode\" : {"
	                        "\"type\" : \"integer\";"
	                "}"
	        "}"
	"}";

	std::string validSchema =
	"{"
	        "\"type\" : \"object\","
	        "\"properties\" : {"
	                "\"errorCode\" : {"
	                        "\"type\" : \"integer\""
	                "}"
	        "}"
	"}";

	JValue faultyInput;

	faultyInput = Object();
	faultyInput.put("returnValue", false);
	faultyInput.put("errorCode", -1);
	faultyInput.put("errorText", "Launch helper exited with unknown return code 0");
	toString(faultyInput, input);
	QTest::newRow("bad schema test") << input << faultySchema << false;
	QTest::newRow("schema test from bug") << input << validSchema << true;
}

void TestRegression::testNOV99444()
{
	using namespace pbnjson;

	QFETCH(std::string, rawInput);
	QFETCH(std::string, schema);
	QFETCH(bool, inputMatchesSchema);

	qDebug() << "Testing" << rawInput.c_str() << "against" << schema.c_str();

	JSchema inputSchema = JSchemaFragment(schema);
	JDomParser parser;

	for (int i = 0; i < 10; i++) {
		qDebug() << "Retying" << i << "th time";
		parser.parse(rawInput, inputSchema);
	}
	bool parsed = parser.parse(rawInput, inputSchema);
	QCOMPARE(parsed, inputMatchesSchema);

	if (!parsed)
		return;

	JValue json = parser.getDom();

	int errorCode;
	QCOMPARE(json["errorCode"].asNumber<int>(errorCode), (ConversionResultFlags)CONV_OK);

	if (errorCode != 0) {
		QVERIFY(json["errorText"].isString());
	} else {
		QVERIFY(json["errorText"].isNull());
	}
}

void TestRegression::testSysmgrFailure_data()
{
	INIT_SCHEMA_TEST_DATA;

	JValue faultyInput;
	std::string input;
	std::string validSchema;

	validSchema = "{\"type\":\"object\",\"properties\":{\"quicklaunch\":{\"type\":\"boolean\",\"optional\":true},\"launcher\":{\"type\":\"boolean\",\"optional\":true},\"universal search\":{\"type\":\"boolean\",\"optional\":true}},\"additionalProperties\":false}";
	faultyInput = Object();
	faultyInput.put("universal search", false);
	faultyInput.put("launcher", false);
	toString(faultyInput, input);

	QTest::newRow("schema test from sysmgr crash") << input << validSchema << true;
}

void TestRegression::testSysmgrFailure()
{
	using namespace pbnjson;

	QFETCH(std::string, rawInput);
	QFETCH(std::string, schema);
	QFETCH(bool, inputMatchesSchema);

	JSchema inputSchema = JSchemaFragment(schema);
	JDomParser parser;
	bool parsed = parser.parse(rawInput, inputSchema);
	QCOMPARE(parsed, inputMatchesSchema);
}

	}
}

QTEST_APPLESS_MAIN(pjson::testcxx::TestRegression)
