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
 * TestParse.h
 *
 *  Created on: Mar 11, 2009
 */

#ifndef TESTPARSE_H_
#define TESTPARSE_H_

#include <QTest>
#include <QByteArray>
#include <QObject>
#include <string>
#include <vector>
#include <pbnjson.h>
#include <QDebug>

namespace pjson {
namespace testc {

class TestParse : public QObject
{
	Q_OBJECT

public:
	TestParse();
	virtual ~TestParse();

private:
	typedef std::vector<jvalue_ref> JSONValues;
	typedef std::vector<jschema_ref> JSONSchemas;
	JSONValues m_managed;
	JSONSchemas m_managedSchemas;

	jvalue_ref manage(jvalue_ref toManage)
	{
		qDebug() << "Managing" << toManage;
		/**
		 * NOTE: If you are using this as sample code on how to use PJSON correctly,
		 * note that the values are released automatically at the end of every test
		 * (please don't free it in a destructor somewhere - you'll likely "leak" memory)
		 */
		if (!jis_null(toManage))
			m_managed.push_back(toManage);

		return toManage;
	}

	jschema_ref manage(jschema_ref toManage)
	{
		/**
		 * NOTE: If you are using this as sample code on how to use PJSON correctly,
		 * note that the values are released automatically at the end of every test
		 * (please don't free it in a destructor somewhere - you'll likely "leak" memory)
		 */
		if (toManage != NULL)
			m_managedSchemas.push_back(toManage);

		return toManage;
	}

private slots:
	void initTestCase(); /// before all tests
	void init(); /// before each test
	void cleanup(); /// after after each test function
	void cleanupTestCase();	/// after all tests

	void testParseDoubleAccuracy();
	void testParseFile_data();
	void testParseFile();
};

}
}

#endif /* TESTPARSE_H_ */
