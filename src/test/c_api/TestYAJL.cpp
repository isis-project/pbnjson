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
 * TestYAJL.cpp
 *
 *  Created on: Sep 22, 2009
 */

#include "TestYAJL.h"
#include <QTest>
#include <QtDebug>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>

namespace pjson {

namespace test {

typedef struct
{
	int map_open;
	int map_key;
	int map_close;

	int array_open;
	int array_close;

	int string;
	int number;
	int boolean;
	int null;
} MyCtxt;

TestYAJL::TestYAJL() {
	// TODO Auto-generated constructor stub

}

TestYAJL::~TestYAJL() {
	// TODO Auto-generated destructor stub
}

int yos(void *ctxt) { ((MyCtxt *)ctxt)->map_open++; return 1; }
int yok(void *ctxt, const unsigned char *s, unsigned int len) { ((MyCtxt *)ctxt)->map_key++; return 1; }
int yoc(void *ctxt) { ((MyCtxt *)ctxt)->map_close++; return 1; }

int yas(void *ctxt) { ((MyCtxt *)ctxt)->array_open++; return 1; }
int yac(void *ctxt) { ((MyCtxt *)ctxt)->array_close++; return 1; }

int ys(void *ctxt, const unsigned char * s, unsigned int l) { ((MyCtxt *)ctxt)->string++; return 1; }
int yn(void *ctxt, const char *n, unsigned int l) { ((MyCtxt *)ctxt)->number++; return 1; }
int yb(void *ctxt, int) { ((MyCtxt *)ctxt)->boolean++; return 1; }
int yN(void *ctxt) { ((MyCtxt *)ctxt)->null++; return 1; }

static yajl_callbacks myCallbacks = {
	yN,
	yb,
	NULL,
	NULL,
	yn,
	ys,
	yos,
	yok,
	yoc,
	yas,
	yac,
};

void TestYAJL::testGenerator()
{
	yajl_gen g;
	const unsigned char* buf;
	unsigned int len;

	g = yajl_gen_alloc(NULL, NULL);
	yajl_gen_map_open(g);
	yajl_gen_map_close(g);
	QCOMPARE(yajl_gen_get_buf(g, &buf, &len), yajl_gen_status_ok);
	QCOMPARE(len, 2U);
	QCOMPARE(reinterpret_cast<const char *>(buf), "{}");
	yajl_gen_free(g);

	g = yajl_gen_alloc(NULL, NULL);
	yajl_gen_map_open(g);
	yajl_gen_string(g, reinterpret_cast<const unsigned char*>("TEST"), sizeof("TEST") - 1);
	yajl_gen_number(g, "53", sizeof("53") - 1);
	yajl_gen_map_close(g);
	QCOMPARE(yajl_gen_get_buf(g, (const unsigned char **)&buf, (unsigned int*)&len), yajl_gen_status_ok);
	QCOMPARE(len, 11U);
	QCOMPARE(reinterpret_cast<const char *>(buf), "{\"TEST\":53}");
	yajl_gen_free(g);
}

void TestYAJL::testParser_data()
{
	QTest::addColumn<QString>("input");
	QTest::addColumn<MyCtxt>("expectedStats");

	QTest::newRow("empy object") <<
		"{}" << (MyCtxt){ 1, 0, 1 };

	QTest::newRow("empty array") <<
		"[]" << (MyCtxt){ 0, 0, 0, 1, 1, 0, 0, 0, 0 };

	QTest::newRow("simple object") <<
		"{\"returnVal\":0,\"result\":\"foo\",\"param\":true,\"test\":null}" <<             (MyCtxt){ 1, 4, 1, 0, 0, 1, 1, 1, 1 };

	QTest::newRow("simple array") <<
		"[true, false, 5.0, 6114e67,\"adfadsfa\", \"asdfasd\",{},[null, null]]" << (MyCtxt){ 1, 0, 1, 2, 2, 2, 2, 2, 2 };

	QTest::newRow("complex object") <<
		"[true, false, {\"a\":[\"b\", \"c\", \"d\", 5, 6, 7, null, \"see\"], \"b\":{}, \"c\":50.7e90}, null]" <<
		(MyCtxt){ 2, 3, 2, 2, 2, 4, 4, 2, 2};
}

void TestYAJL::testParser()
{
	QFETCH(QString, input);
	QFETCH(MyCtxt, expectedStats);
	MyCtxt actualStats = { 0 };
	QByteArray utf8Input = input.toUtf8();
	yajl_status errStatus;

	QVERIFY(expectedStats.map_open == expectedStats.map_close);
	QVERIFY(expectedStats.array_open == expectedStats.array_close);

	yajl_handle h = yajl_alloc(&myCallbacks, NULL, NULL, &actualStats);
	QVERIFY(h != NULL);
	if ((errStatus = yajl_parse(h, (const unsigned char *)utf8Input.constData(), utf8Input.size())) != yajl_status_ok) {
		unsigned char *errStr = yajl_get_error(h, 1, (unsigned char *)utf8Input.constData(), utf8Input.size());
		qWarning() << "Failed to parse:" << (char *)errStr;
		yajl_free_error(h, errStr);
		QCOMPARE((int)errStatus, (int)yajl_status_ok);
	}
	QCOMPARE(yajl_parse_complete(h), yajl_status_ok);
	yajl_free(h);

	QCOMPARE(actualStats.map_open, expectedStats.map_open);
	QCOMPARE(actualStats.map_key, expectedStats.map_key);
	QCOMPARE(actualStats.map_close, expectedStats.map_close);

	QCOMPARE(actualStats.array_open, expectedStats.array_open);
	QCOMPARE(actualStats.array_close, expectedStats.array_close);

	QCOMPARE(actualStats.string, expectedStats.string);
	QCOMPARE(actualStats.number, expectedStats.number);
	QCOMPARE(actualStats.boolean, expectedStats.boolean);

	QCOMPARE(actualStats.null, expectedStats.null);
}

}

}

Q_DECLARE_METATYPE(pjson::test::MyCtxt);

QTEST_APPLESS_MAIN(pjson::test::TestYAJL);
