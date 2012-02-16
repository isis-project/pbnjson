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
 * TestSchema.cpp
 *
 *  Created on: Oct 29, 2009
 */

#include "TestSchema.h"
#include <QTest>
#include <QtDebug>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QDir>
#include <limits>
#include <QPointer>
#include <QSharedPointer>

Q_DECLARE_METATYPE(raw_buffer);

static JSchemaResolutionResult simpleResolver(JSchemaResolverRef resolver, jschema_ref *resolved);

namespace pjson {

namespace testc {

TestSchema::TestSchema(raw_buffer schemaInput, const Inputs& inputs, bool shouldValidate)
	: m_schema(schemaInput),
	  m_fileNames(inputs.fileNames),
	  m_inputs(inputs.fileData),
	  m_mustPass(shouldValidate),
	  m_schemaDOM(NULL)
{
}

TestSchema::~TestSchema() {
	// TODO Auto-generated destructor stub
}

void TestSchema::initTestCase()
{
	m_schemaDOM = jschema_parse(m_schema,
			DOMOPT_INPUT_NOCHANGE | DOMOPT_INPUT_OUTLIVES_DOM, NULL);
	QVERIFY(m_schemaDOM != NULL);
	QCOMPARE(m_fileNames.size(), m_inputs.size());
}

void TestSchema::init()
{
}

void TestSchema::cleanup()
{
}

void TestSchema::cleanupTestCase()
{
	QVERIFY(m_schemaDOM != NULL);
	jschema_release(&m_schemaDOM);
}

void TestSchema::validateInput_data()
{
	QTest::addColumn<bool>("testDOM");
	QTest::addColumn<raw_buffer>("testInput");

	for (int i = 0; i < m_inputs.size(); i++) {
		QStringList testTitleBuilder;
		QString testTitle;
		testTitleBuilder << "" << "input file" << m_fileNames.at(i);

		testTitleBuilder[0] = "SAX";
		testTitle = testTitleBuilder.join(" ");
		m_dataNames += testTitle.toUtf8();
		QTest::newRow(m_dataNames.last().constData()) << false << m_inputs.at(i);

		testTitleBuilder[0] = "DOM";
		testTitle = testTitleBuilder.join(" ");
		m_dataNames += testTitle.toUtf8();
		QTest::newRow(m_dataNames.last().constData()) << true << m_inputs.at(i);
	}
}

void TestSchema::testSimpleSchema()
{
	JSchemaInfo schemaInfo;
	jschema_info_init(&schemaInfo, jschema_all(), NULL, NULL);
	QVERIFY(jsax_parse(NULL, J_CSTR_TO_BUF("{}"), &schemaInfo));
}

void TestSchema::validateInput()
{
	QFETCH(raw_buffer, testInput);
	QFETCH(bool, testDOM);

	JSchemaResolver resolver;
	resolver.m_resolve = simpleResolver;
	resolver.m_userCtxt = this;	// in this case, not necessary, but good style

	JSchemaInfo schemaInfo;
	jschema_info_init(&schemaInfo, m_schemaDOM,
			&resolver,
			NULL /* use the default error handler - no recovery possible */);

	const char *dataName = QTest::currentDataTag();
	QByteArray failureInfo(dataName);
	failureInfo += " (must validate... ";
	failureInfo += (m_mustPass ? "yes" : "no");
	failureInfo += "): '" + QByteArray::fromRawData(testInput.m_str, testInput.m_len);

#if BYPASS_SCHEMA
	if (!m_mustPass) {
		QEXPECT_FAIL("", "Schemas disabled - verifying schema rejects invalid input not available", Continue);
	}
#endif

	if (!testDOM) {
		QVERIFY2(jsax_parse_ex(NULL, testInput, &schemaInfo, NULL, true) == m_mustPass, failureInfo.constData());
	} else {
		jvalue_ref parsed = jdom_parse(testInput, DOMOPT_NOOPT, &schemaInfo);
		bool parseOk = !jis_null(parsed);
		j_release(&parsed);
		if (m_mustPass)
			QVERIFY2(parseOk, failureInfo.constData());
		else
			QVERIFY2(!parseOk, failureInfo.constData());
	}
}

}

}

#define NORETURN __attribute__((noreturn))

static void printUsage(FILE* output, int returnCode, int argc, char **argv) NORETURN;
static void printUsage(FILE* output, int returnCode, int argc, char **argv)
{
	fprintf(output, "Usage: %s -schema <schema> -input <toValidate> -pass <shouldValidate>", argv[0]);
	fprintf(output, "\n\t\t-schema The schema to use for validation");
	fprintf(output, "\n\t\t-input  The file or directory to use as input to validate against the schema.");
	fprintf(output, "\n\t\t        If it is a directory, than we use all files prefixed with the schema");
	fprintf(output, "\n\t\t        name (sans any extension) and with an extension .json");
	fprintf(output, "\n\t\t-pass   0 if the tests are expected to not validate against the schema,");
	fprintf(output, "\n\t\t        anything else indicates they must validate");

	exit(returnCode);
}

static QFile* map(QString path, raw_buffer &fData)
{
	const char *message;

	QFile *f = new QFile(path);

	if (!f->open(QIODevice::ReadOnly)) {
		message = "is not readable";
		goto map_failed;
	}

	if (f->size() > std::numeric_limits<typeof(fData.m_len)>::max()) {
		message = "is too big";
		goto map_failed;
	}

	if (f->size() != 0) {
		fData.m_len = f->size();
		fData.m_str = (const char *)f->map(0, f->size());
		if (fData.m_str == NULL) {
			message = "failed to memory map";
			goto map_failed;
		}
	} else {
		qDebug() << "File" << path << "is empty";
		fData.m_len = 0;
		fData.m_str = "";
	}

	return f;

map_failed:

	f->close();
	delete f;
	f = NULL;

	const char *fileNameStr = qPrintable(path);

	fprintf(stderr, "%s %s\n", fileNameStr, message);
	return f;
}

typedef QSharedPointer<QFile> FilePtr;

template <class T>
static void destroyFile(T file);

static __attribute__((unused)) void destroyFile(QPointer<QFile> file)
{
	delete file;
	Q_ASSERT (file == NULL);
}

static void destroyFile(QSharedPointer<QFile> file)
{
	file.clear();
	Q_ASSERT (file.isNull());
}

static void destroyFiles(QList<FilePtr> &fileHandles)
{
	while (!fileHandles.isEmpty())
		destroyFile(fileHandles.takeLast());
	fileHandles.clear();
}

static QDir resolutionDir;

static JSchemaResolutionResult simpleResolver(JSchemaResolverRef resolver, jschema_ref *resolved)
{
	QString resourceName = QString::fromUtf8(resolver->m_resourceToResolve.m_str, resolver->m_resourceToResolve.m_len);
	QFileInfo lookupPath(resolutionDir, resourceName + ".schema");
	if (!lookupPath.isFile() || !lookupPath.isReadable()) {
		qWarning() << "Failed to resolve" << resourceName << ".  Resolved path" << lookupPath.absoluteFilePath() << " isn't a file";
		return SCHEMA_NOT_FOUND;
	}

	QFile resourceData(lookupPath.absoluteFilePath());
	if (!resourceData.open(QIODevice::ReadOnly)) {
		qWarning() << "Failed to open" << resourceName << "(" << lookupPath.absoluteFilePath() << ")";
		return SCHEMA_IO_ERROR;
	}

	raw_buffer readSchema;
	if (resourceData.size() > std::numeric_limits<typeof(readSchema.m_len)>::max()) {
		qWarning() << "Schema" << lookupPath.absoluteFilePath() << "is too big";
		return SCHEMA_GENERIC_ERROR;
	}

	readSchema.m_len = resourceData.size();
	readSchema.m_str = new char[readSchema.m_len];

	QDataStream schemaReader(&resourceData);
	if (!readSchema.m_len == schemaReader.readRawData((char *)readSchema.m_str, readSchema.m_len)) {
		qWarning() << "Failed to read schema" << resourceName << "fully";
		delete [] readSchema.m_str;
		return SCHEMA_IO_ERROR;
	}

	*resolved = jschema_parse(readSchema, DOMOPT_NOOPT, NULL);
	delete [] readSchema.m_str;
	if (*resolved == NULL)
		return SCHEMA_INVALID;

	qDebug() << "Resolved reference for" << resourceName;
	return SCHEMA_RESOLVED;
}

int main(int argc, char **argv)
{
	// we need the -schema <schema>, -input <json file or directory containing $(basename schema)*.json> and -pass <bool>
	QStringList arguments;
	std::string schema, input;
	bool pass = true;

	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 's':
					if (strcmp(argv[i], "-schema") == 0) {
						schema = argv[++i];
						continue;
					}
					break;
				case 'i':
					if (strcmp(argv[i], "-input") == 0) {
						input = argv[++i];
						continue;
					}
					break;
				case 'p':
					if (strcmp(argv[i], "-pass") == 0) {
						pass = strcmp(argv[++i], "0") != 0;
						continue;
					}
					break;
				case '-':
					if (strcmp(argv[i], "--help") == 0) {
						printUsage(stdout, 1, argc, argv);
					}
			}
		}
		arguments += argv[i];
	}

	raw_buffer schemaData;
	QFileInfo schemaFileInfo(QString::fromStdString(schema));
	FilePtr schemaFile(map(schemaFileInfo.absoluteFilePath(), schemaData));
	if (schemaFile.isNull())
		printUsage(stderr, 2, argc, argv);
	resolutionDir = schemaFileInfo.dir();
	qDebug() << "Looking for schemas in" << resolutionDir.absolutePath();

	QList<FilePtr> fileHandles;
	pjson::testc::Inputs jsonInput;

	QString inputQStr = QString::fromStdString(input);
	QFileInfo inputInfo(inputQStr);
	QDir inputsDir(inputInfo.dir());
	if (inputInfo.isDir()) {
		QStringList nameFilters;
		nameFilters += QFileInfo(QString::fromStdString(schema)).completeBaseName() + "*.json";
		QFileInfoList tests = inputsDir.entryInfoList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
		for (int i = 0; i < tests.size(); i++) {
			jsonInput.fileNames << tests.at(i).absoluteFilePath();
		}
	} else {
		jsonInput.fileNames += inputQStr;
	}

	for (int i = 0; i < jsonInput.fileNames.size(); i++) {
		raw_buffer fileData;
		FilePtr fileHandle(map(jsonInput.fileNames.at(i), fileData));
		if (fileHandle == NULL) {
			destroyFiles(fileHandles);
			printUsage(stderr, 3, argc, argv);
		}

		fileHandles += fileHandle;
		jsonInput.fileData += fileData;
	}

	if (jsonInput.fileData.size() == 0) {
		fprintf(stderr, "No json inputs found to validate against in %s\n", input.c_str());
		printUsage(stderr, 4, argc, argv);
	}

	Q_ASSERT(jsonInput.fileData.size() == jsonInput.fileNames.size());

	pjson::testc::TestSchema schemaTester(schemaData, jsonInput, pass);

	return QTest::qExec(&schemaTester, arguments);
}

