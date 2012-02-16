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

static QDir resolutionDir;

class SimpleResolver : public pbnjson::JResolver {
public:
	pbnjson::JSchema resolve(const ResolutionRequest &request, JSchemaResolutionResult &resolutionResult)
	{
		pbnjson::JSchema parent(request.schema());
		QString resourceName = QString::fromStdString(request.resource());
		QFileInfo lookupPath(resolutionDir, resourceName + ".schema");
		if (!lookupPath.isFile() || !lookupPath.isReadable()) {
			qWarning() << "Failed to resolve" << resourceName << ".  Resolved path" << lookupPath.absoluteFilePath() << " isn't a file";
			resolutionResult = SCHEMA_NOT_FOUND;
			return pbnjson::JSchema::NullSchema();
		}
		QString filePath = lookupPath.absoluteFilePath();
		QByteArray filePathUtf8 = filePath.toUtf8();
		std::string filePathStr = filePathUtf8.data();
		pbnjson::JSchemaFile resolved(filePathStr);
		if (!resolved.isInitialized()) {
			qWarning() << "Failed to open" << resourceName << "(" << lookupPath.absoluteFilePath() << ")";
			resolutionResult = SCHEMA_IO_ERROR;
			return pbnjson::JSchema::NullSchema();
		}

		resolutionResult = SCHEMA_RESOLVED;
		return resolved;
	}
};

namespace pjson {

namespace testcxx {

TestSchema::TestSchema(const pj::JSchema& schema, const Inputs& inputs, bool shouldValidate)
	: m_fileNames(inputs.fileNames),
	  m_inputs(inputs.fileData),
	  m_mustPass(shouldValidate),
	  m_schemaDOM(schema)
{
}

TestSchema::~TestSchema() {
	// TODO Auto-generated destructor stub
}

void TestSchema::initTestCase()
{
	QCOMPARE(m_fileNames.size(), m_inputs.size());
	QVERIFY(m_schemaDOM.isInitialized());
}

void TestSchema::init()
{
}

void TestSchema::cleanup()
{
}

void TestSchema::cleanupTestCase()
{
}

void TestSchema::validateInput_data()
{
	QTest::addColumn<bool>("testDOM");
	QTest::addColumn<QByteArray>("testInput");

	QByteArray input;

	for (int i = 0; i < m_inputs.size(); i++) {
		QStringList testTitleBuilder;
		QString testTitle;

		testTitleBuilder << "" << "input file" << m_fileNames.at(i);

#if 0
		testTitleBuilder[0] = "SAX";
		testTitle = testTitleBuilder.join(" ");
		m_dataNames += testTitle.toUtf8();
		QTest::newRow(m_dataNames.last().constData()) << false << m_inputs.at(i);
#endif

		testTitleBuilder[0] = "DOM";
		testTitle = testTitleBuilder.join(" ");
		m_dataNames += testTitle.toUtf8();
		QTest::newRow(m_dataNames.last().constData()) << true << m_inputs.at(i);
	}
}

void TestSchema::validateInput()
{
	QFETCH(QByteArray, testInput);
	QFETCH(bool, testDOM);

	const char *dataName = QTest::currentDataTag();
	QByteArray failureInfo(dataName);
	failureInfo += " (must validate... ";
	failureInfo += (m_mustPass ? "yes" : "no");
	failureInfo += "): '" + testInput;

	if (!testDOM) {
#if 0
		// from C unit test
		QVERIFY2(jsax_parse_ex(NULL, testInput, &schemaInfo, NULL, true) == m_mustPass, failureInfo.constData());
#else
		QVERIFY2(false == true, "C++ SAX schema support untested");
#endif
	} else {
		std::string testInputStr(testInput.data(), testInput.size());

		SimpleResolver resolver;
		pj::JDomParser parser(&resolver);
		bool parsed = parser.parse(testInputStr, m_schemaDOM);

#if BYPASS_SCHEMA
		if (!m_mustPass) {
			QEXPECT_FAIL("", "Schemas disabled - verifying schema rejects invalid input not available", Continue);
		}
#endif

		QVERIFY2(parsed == m_mustPass, failureInfo.constData());
		QVERIFY2(!parsed == parser.getDom().isNull(), failureInfo.constData());
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
	Q_ASSERT(file.isNull());
}

static void destroyFiles(QList<FilePtr> &fileHandles)
{
	while (!fileHandles.isEmpty())
		destroyFile(fileHandles.takeLast());
	fileHandles.clear();
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

	pbnjson::JSchemaFile schemaFile(schema);
	QFileInfo schemaFileInfo(QString::fromStdString(schema));

	if (!schemaFile.isInitialized())
		printUsage(stderr, 2, argc, argv);
	resolutionDir = schemaFileInfo.dir();
	qDebug() << "Looking for schemas in" << resolutionDir.absolutePath();

	QList<FilePtr> fileHandles;
	pjson::testcxx::Inputs jsonInput;

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
		jsonInput.fileData += QByteArray::fromRawData(fileData.m_str, fileData.m_len);
	}

	if (jsonInput.fileData.size() == 0) {
		fprintf(stderr, "No json inputs found to validate against in %s\n", input.c_str());
		printUsage(stderr, 4, argc, argv);
	}

	Q_ASSERT(jsonInput.fileData.size() == jsonInput.fileNames.size());

	pjson::testcxx::TestSchema schemaTester(schemaFile, jsonInput, pass);

	return QTest::qExec(&schemaTester, arguments);
}

