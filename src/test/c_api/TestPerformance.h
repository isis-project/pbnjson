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

#ifndef TEST_PERFORMANCE_H_
#define TEST_PERFORMANCE_H_

#include <QObject>
#include <pbnjson.h>
#include <QStringList>

namespace pbnjson {
namespace testc {

class TestPerformance : public QObject
{
Q_OBJECT
public:
	typedef enum {
		CJSON,
		PBNJSON,
		YAJL,
		PBNJSON_SAX,
	} TestLibrary;

	TestPerformance();

private:

	static QList<JDOMOptimizationFlags> NO_OPTS;
	static QList<JDOMOptimizationFlags> ALL_OPTS;
	static QList<jschema_ref> EMPTY_SCHEMA;
	QStringList smallInputs;
	QStringList bigInputs;

	void parseCJSON(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema);
	void parsePBNJSON(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema);
	void parseSAX(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema);
	void parseYAJL(raw_buffer input, JDOMOptimizationFlags opt, jschema_ref schema);

	int domData(QStringList jsonInputs, const char *type,
			TestLibrary lib,
			QList<JDOMOptimizationFlags> opts,
			QList<jschema_ref> schemas = EMPTY_SCHEMA);

private slots:
	void initTestCase();
	void cleanupTestCase();

	void testParser_data();
	void testParser();
	void testSAX_data();
	void testSAX();
};

}

}

#endif /* TEST_PERFORMANCE_H_ */
