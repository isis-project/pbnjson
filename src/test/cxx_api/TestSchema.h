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
#include <pbnjson.hpp>

namespace pjson {
	namespace testcxx {

namespace pj = ::pbnjson;

struct Inputs {
	QStringList fileNames;
	QList<QByteArray> fileData;
};

class TestSchema : public QObject {
	Q_OBJECT

public:
	TestSchema(const pj::JSchema& schema, const Inputs& inputs, bool shouldValidate);
	virtual ~TestSchema();

private:
	QStringList m_fileNames;
	QList<QByteArray> m_inputs;
	bool m_mustPass;

	const pj::JSchema &m_schemaDOM;
	QList<QByteArray> m_dataNames;

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void cleanupTestCase();

	void validateInput_data();
	void validateInput();
};

	}
}

#endif /* TEST_SCHEMA_H_ */
