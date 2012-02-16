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
 * TestSchemaSanity.cpp
 *
 *  Created on: Oct 29, 2009
 */

#include "TestSchemaSanity.h"
#include <QTest>
#include <QtDebug>

Q_DECLARE_METATYPE(raw_buffer);

namespace pjson {

	namespace testc {
		TestSchemaSanity::TestSchemaSanity()
		{
		}

		TestSchemaSanity::~TestSchemaSanity() {
			// TODO Auto-generated destructor stub
		}

		void TestSchemaSanity::initTestCase()
		{
		}

		void TestSchemaSanity::init()
		{
		}

		void TestSchemaSanity::cleanup()
		{
		}

		void TestSchemaSanity::cleanupTestCase()
		{
		}

		void TestSchemaSanity::testSimpleSchema()
		{
			JSchemaInfo schemaInfo;
			jschema_info_init(&schemaInfo, jschema_all(), NULL, NULL);
			QVERIFY(jsax_parse(NULL, J_CSTR_TO_BUF("[]"), &schemaInfo));
			QVERIFY(jsax_parse(NULL, J_CSTR_TO_BUF("{}"), &schemaInfo));
		}

		void TestSchemaSanity::testSchemaReuse()
		{
			jschema_ref all = jschema_parse(J_CSTR_TO_BUF("{}"), JSCHEMA_DOM_NOOPT, NULL);
			JSchemaInfo schemaInfo;
			jschema_info_init(&schemaInfo, all, NULL, NULL);

			for (int i = 0; i < 10; i++)
				QVERIFY(jsax_parse(NULL, J_CSTR_TO_BUF("{}"), &schemaInfo));

			for (int i = 0; i < 10; i++)
				QVERIFY(jsax_parse(NULL, J_CSTR_TO_BUF("[]"), &schemaInfo));

			jschema_release(&all);
		}
	}

}

QTEST_APPLESS_MAIN(pjson::testc::TestSchemaSanity);
