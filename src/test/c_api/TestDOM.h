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
#include <pbnjson.h>

namespace pjson {
namespace testc {

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

	typedef std::vector<jvalue_ref> JSONValues;
	JSONValues m_managed;

	void validateBool(jvalue_ref jbool, bool expected, JType expectedType = TJBOOL);
	void validateString(jvalue_ref jstr, QByteArray expected);
	template <typename T>
	void validateNumber(jvalue_ref jnum, T expected, ConversionResultFlags conversionResult = CONV_OK);

	jvalue_ref getChild(jvalue_ref obj, std::string key);
	jvalue_ref getChild(jvalue_ref obj, size_t i);

	jvalue_ref getChildObject(jvalue_ref obj, std::string key);
	jvalue_ref getChildObject(jvalue_ref arr, size_t i);

	jvalue_ref getChildArray(jvalue_ref obj, std::string key);
	jvalue_ref getChildArray(jvalue_ref arr, size_t i);

	jvalue_ref getChildString(jvalue_ref obj, std::string key);
	jvalue_ref getChildString(jvalue_ref arr, size_t i);

	jvalue_ref getChildNumber(jvalue_ref obj, std::string key);
	jvalue_ref getChildNumber(jvalue_ref arr, size_t i);

	jvalue_ref getChildBool(jvalue_ref obj, std::string key);
	jvalue_ref getChildBool(jvalue_ref arr, size_t i);

	jvalue_ref getChildNull(jvalue_ref obj, std::string key);
	jvalue_ref getChildNull(jvalue_ref arr, size_t i);

	jvalue_ref manage(jvalue_ref toManage)
	{
		/**
		 * NOTE: If you are using this as sample code on how to use PJSON correctly,
		 * note that the values are released automatically at the end of every test
		 * (please don't free it in a destructor somewhere - you'll likely "leak" memory)
		 */
		if (!jis_null(toManage))
			m_managed.push_back(toManage);

		return toManage;
	}

	void testValist(jobject_key_value item, ...);

private slots:
	void initTestCase(); /// before all tests
	void init(); /// before each test
	void cleanup(); /// after after each test function
	void cleanupTestCase();	/// after all tests

	void testObjectSimple();
	void testObjectComplicated();
	void testObjectPut();

	void testArraySimple();
	void testArrayComplicated();

	void testStringSimple_data();
	void testStringSimple();
	void testStringDealloc();

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
