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
 * TestSAX.h
 *
 *  Created on: Sep 22, 2009
 */

#ifndef TESTSAX_H_
#define TESTSAX_H_

#include <QTest>

#include <pbnjson.h>
#include <vector>

namespace pjson {

namespace testc {

class TestSAX : public QObject
{
	Q_OBJECT

public:
	TestSAX();
	virtual ~TestSAX();

private:
	typedef std::vector<jvalue_ref> JSONValues;
	JSONValues m_managed;

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

private slots:
	void initTestCase();
	void cleanupTestCase();

	void testGenerator_data();
	void testGenerator();
	void testParser_data();
	void testParser();
};

}

}

#endif /* TESTSAX_H_ */
