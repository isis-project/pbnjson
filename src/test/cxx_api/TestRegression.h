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

#ifndef TEST_REGRESSION_H_
#define TEST_REGRESSION_H_

#include <QTest>

namespace pjson {
	namespace testcxx {

class TestRegression : public QObject
{
	Q_OBJECT
public:
	TestRegression();
	virtual ~TestRegression();

private:
	void initSchemaTest();

private slots:
	void initTestCase(); /// before all tests
	void init(); /// before each test
	void cleanup(); /// after each test function
	void cleanupTestCase(); /// after all tests

	/**
	 * https://jira.palm.com/browse/NOV-99444
	 */
	void testNOV99444();
	void testNOV99444_data(); /// the input JSON to drive the test

	void testSysmgrFailure();
	void testSysmgrFailure_data();
};

	}
}

#endif // TEST_REGRESSION_H_

