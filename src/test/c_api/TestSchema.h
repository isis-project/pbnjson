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

#ifndef TEST_SCHEMA_H_
#define TEST_SCHEMA_H_

#include <QObject>
#include <QStringList>
#include <pbnjson.h>

namespace pjson {
	namespace testc {

struct Inputs {
	QStringList fileNames;
	QList<raw_buffer> fileData;
};

class TestSchema : public QObject {
	Q_OBJECT

public:
	TestSchema(raw_buffer schemaStr, const Inputs& inputs, bool shouldValidate);
	virtual ~TestSchema();

private:
	raw_buffer m_schema;
	QStringList m_fileNames;
	QList<raw_buffer> m_inputs;
	bool m_mustPass;

	jschema_ref m_schemaDOM;
	QList<QByteArray> m_dataNames;

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void cleanupTestCase();

	void validateInput_data();
	void validateInput();

	void testSimpleSchema();
};

	}
}

#endif /* TEST_SCHEMA_H_ */
