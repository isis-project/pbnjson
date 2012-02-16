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
 * TestDom.h
 *
 *  Created on: Sep 15, 2009
 */

#ifndef TESTDOM_H_
#define TESTDOM_H_

#include <QTest>
#include <QByteArray>
#include <QObject>
#include <string>
#include <vector>
#include <pbnjson.hpp>

namespace pjson {
namespace testcxx {

class TestDOM : public QObject
{
	Q_OBJECT

public:
	TestDOM();
	virtual ~TestDOM();

private:
	enum JType {
		TJOBJ, TJARR, TJSTR, TJNUM, TJBOOL, TJNULL
	};

	void validateBool(const pbnjson::JValue& jbool, bool expected, JType expectedType = TJBOOL);
	void validateString(const pbnjson::JValue& jstr, QByteArray expected);
	template <typename T>
	void validateNumber(const pbnjson::JValue& jnum, T expected, ConversionResultFlags conversionResult = CONV_OK);

	pbnjson::JValue getChild(const pbnjson::JValue& obj, std::string key);
	pbnjson::JValue getChild(const pbnjson::JValue& obj, int i);

	pbnjson::JValue getChildObject(const pbnjson::JValue& obj, std::string key);
	pbnjson::JValue getChildObject(const pbnjson::JValue& arr, size_t i);

	pbnjson::JValue getChildArray(const pbnjson::JValue& obj, std::string key);
	pbnjson::JValue getChildArray(const pbnjson::JValue& arr, size_t i);

	pbnjson::JValue getChildString(const pbnjson::JValue& obj, std::string key);
	pbnjson::JValue getChildString(const pbnjson::JValue& arr, size_t i);

	pbnjson::JValue getChildNumber(const pbnjson::JValue& obj, std::string key);
	pbnjson::JValue getChildNumber(const pbnjson::JValue& arr, size_t i);

	pbnjson::JValue getChildBool(const pbnjson::JValue& obj, std::string key);
	pbnjson::JValue getChildBool(const pbnjson::JValue& arr, size_t i);

	pbnjson::JValue getChildNull(const pbnjson::JValue& obj, std::string key);
	pbnjson::JValue getChildNull(const pbnjson::JValue& arr, size_t i);

	void testValist(jobject_key_value item, ...);

private slots:
	void initTestCase(); /// before all tests
	void init(); /// before each test
	void cleanup(); /// after after each test function
	void cleanupTestCase();	/// after all tests

	void testParserObjectSimple();
	void testParserObjectComplex();
	void testParserArray();
	void testParserComplex();

	void testObjectSimple();
	void testObjectComplicated();
	void testObjectIterator();
	void testObjectPut();
	void testArraySimple();
	void testArrayComplicated();
	void testStringSimple_data();
	void testStringSimple();
	void testInteger32Simple();
	void testInteger320();
	void testInteger32Limits();
	void testInteger64Simple();
	void testInteger640();
	void testInteger64Limits();
	void testDouble0();
	void testDoubleInfinities();
	void testDoubleNaN();
	void testDoubleFromInteger();	/// this test is for integers > 2^53
	void testBoolean_data();
	void testBoolean();
};

}
}

#endif /* TESTDOM_H_ */
