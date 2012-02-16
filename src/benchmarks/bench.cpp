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

#include <string>
#include <iostream>
#include <boost/program_options.hpp>

#include "bench_yajl.h"
#include "bench_pbnjson.h"
#include "bench_mjson.h"
#include "bench_cjson.h"

#include <pbnjson.h>	// for raw_buffer definition

using namespace std;
namespace po = boost::program_options;

enum ExitCodes {
	EXIT_OK,
	EXIT_ERRARGS_ENGINE,    	/* the engine is invalid */
	EXIT_ERRARGS_SCHEMA,    	/* the schema to use in the test is invalid (or not a valid option) */
	EXIT_ERRARGS_LENGTH,    	/* the amount of time to run the test for is invalid (or not a valid option) */
	EXIT_ERRARGS_UTF8,      	/* invalid utf8 validation option selected (or not a valid option) */
	EXIT_ERRARGS_INPUT,     	/* invalid input file */
	EXIT_ERRARGS_PARSE_TYPE,	/* invalid parse type for pbnjson library */
	EXIT_RUN_ERROR,         	/* error running the benchmark */
	EXIT_NUM_EXITCODES,
};

#define OPT_HELP "help"
#define OPT_ENGINE "engine"
#define OPT_SCHEMA "schema"
#define OPT_UTF8_VALIDATION "utf8"
#define OPT_TEST_ITERATIONS "iterations"
#define OPT_TEST_RUN_TIME "duration"
#define OPT_INPUT "input"
#define OPT_SAX "sax"
#define OPT_DOM "dom"
#define OPT_ENGINES "engines"
#define OPT_NOOP_CALLBACKS "sax-noop"
#define OPT_STATISTICS "json-info"

#define ENGINE_YAJL "yajl"
#define ENGINE_PBNJSON_C "pbnjson_c"
#define ENGINE_PBNJSON_CPP "pbnjson_cpp"
#define ENGINE_CJSON "cjson"
#define ENGINE_MJSON "mjson"

struct JSONStats {
	size_t numObjects;
	size_t numKeys;
	size_t keySize;
	size_t numArrays;
	size_t numElements;
	size_t numStrings;
	size_t stringSize;
	size_t numNumbers;
	size_t numBooleans;
	size_t numNulls;
};

static void statistics(pbnjson::JValue json, JSONStats &stats)
{
	if (json.isObject()) {
		stats.numObjects++;
		pbnjson::JValue::ObjectIterator i;
		for (i = json.begin(); i != json.end(); i++) {
			stats.numKeys ++;
			stats.keySize += (*i).first.asString().length();
			pbnjson::JValue child = (*i).second;
			statistics(child, stats);
		}
	} else if (json.isArray()) {
		stats.numArrays++;
		stats.numElements += json.arraySize();
		for (ssize_t i = 0; i < json.arraySize(); i++) {
			statistics(json, stats);
		}
	} else if (json.isString()) {
		stats.numStrings++;
		stats.stringSize += json.asString().length();
	} else if (json.isNumber()) {
		stats.numNumbers++;
	} else if (json.isBoolean()) {
		stats.numBooleans++;
	} else if (json.isNull()) {
		stats.numNulls++;
	}
}

int main(int argc, char **argv)
{
	string jsonEngine;
	string jsonInput;
	string schemaPath;
	bool utf8Validation = false;
	bool sax = false;
	size_t iterations;
	double runLength;
	json::Benchmark *benchmark;

	po::options_description desc("Allowed options");
	desc.add_options()
		(OPT_HELP, "print this help message")
		(OPT_ENGINE, po::value<string>(&jsonEngine), "the engine to use (one of " ENGINE_YAJL ", " ENGINE_CJSON ", " ENGINE_MJSON ", " ENGINE_PBNJSON_C ", " ENGINE_PBNJSON_CPP ")")
		(OPT_SCHEMA, po::value<string>(&schemaPath), "the path to the schema to use (only valid with pbnjson engines)")
		(OPT_UTF8_VALIDATION, po::value<bool>(&utf8Validation), "whether or not to do utf8 validation on the input (currently only valid with yajl)")
		(OPT_TEST_ITERATIONS, po::value<size_t>(&iterations), "how many times to run the test")
		(OPT_TEST_RUN_TIME, po::value<double>(&runLength), "how long to run the test for (in seconds)")
		(OPT_INPUT, po::value<string>(&jsonInput), "the JSON input file to use for benchmarking")
		(OPT_SAX, "use the sax parser in the pbnjson library")
		(OPT_DOM, "use the dom parser in the pbnjson library")
		(OPT_ENGINES, "list the engines supported for benchmarking")
		(OPT_NOOP_CALLBACKS, "use noop callbacks")
		(OPT_STATISTICS, "print statistics about the input json")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count(OPT_STATISTICS)) {
		benchmark::utils::MemoryMap inputData(jsonInput, benchmark::utils::MemoryMap::MapReadOnly);
		pbnjson::JDomParser parser;
		pbnjson::JSchemaFragment anySchema("{}");
		string input(inputData.map<char>(), inputData.size());
		if (!parser.parse(input, anySchema)) {
			cerr << "Unable to parse " << jsonInput << "\n";
			return EXIT_RUN_ERROR;
		}
		JSONStats stats = { 0 };
		pbnjson::JValue parsed = parser.getDom();
		statistics(parsed, stats);
		cout << stats.numObjects << " objects w/ " <<
			stats.numKeys / (double) stats.numObjects << " keys/object (" <<
			stats.keySize / (double) stats.numKeys << " bytes/key)\n";
		cout << stats.numArrays << " arrays w/ " << stats.numArrays / (double) stats.numElements << " elements/array\n";
		cout << stats.numStrings << " strings w/ " << stats.stringSize / (double) stats.numStrings << "\n";
		cout << stats.numNumbers << " numbers\n";
		cout << stats.numBooleans << " booleans\n";
		cout << stats.numNulls << " nulls\n";
		return 0;
	}

	if (!vm.count(OPT_ENGINE)) {
		cerr << "Need to specify the engine to benchmark\n";
		cerr << desc << "\n";
		return EXIT_ERRARGS_ENGINE;
	}

	if (vm.count(OPT_HELP)) {
		cout << desc << "\n";
		return 0;
	}

	if (vm.count(OPT_ENGINES)) {
		cout << "The following backend json libraries are supported for benchmarking:\n";
		cout << "\tpbnjson_c\n";
		cout << "\tpbnjson_cpp\n";
#ifdef HAVE_YAJL
		cout << "	yajl\n";
#endif
#ifdef HAVE_CJSON
		cout << "	cjson\n";
#endif
#ifdef HAVE_MJSON
		cout << "	mjson\n";
#endif
		return 0;
	}

	if (vm.count(OPT_SCHEMA)) {
		if (jsonEngine != ENGINE_PBNJSON_C && jsonEngine != ENGINE_PBNJSON_CPP) {
			cerr << "Schema specification not valid with requested engine '" << jsonEngine << "'\n";
			cerr << desc << "\n";
			return EXIT_ERRARGS_SCHEMA;
		}

		jschema_ref parsedSchema = jschema_parse_file(schemaPath.c_str(), NULL);
		if (parsedSchema == NULL) {
			cerr << "Schema " << schemaPath << " isn't valid\n";
			return EXIT_ERRARGS_SCHEMA;
		}
	}

	if (vm.count(OPT_UTF8_VALIDATION)) {
		if (jsonEngine != ENGINE_YAJL) {
			cerr << "Utf8 validation currently only supported by yajl engine\n";
			return EXIT_ERRARGS_UTF8;
		}
	}

	if (!(vm.count(OPT_TEST_ITERATIONS) ^ vm.count(OPT_TEST_RUN_TIME))) {
		cerr << "Conflicting options selected - please specify either " << OPT_TEST_ITERATIONS << " or " << OPT_TEST_RUN_TIME << "\n";
		cerr << desc << "\n";
		return EXIT_ERRARGS_LENGTH;
	}

	if (!vm.count(OPT_INPUT)) {
		cerr << "Need to specify an input file\n";
		cerr << desc << "\n";
		return EXIT_ERRARGS_INPUT;
	}

	if (jsonEngine == ENGINE_PBNJSON_C || jsonEngine == ENGINE_PBNJSON_CPP) {
		if (!vm.count(OPT_SAX) && !vm.count(OPT_DOM)) {
			cerr << "Need to specify sax or dom when running\n";
			cerr << desc << "\n";
			return EXIT_ERRARGS_PARSE_TYPE;
		}
		if (vm.count(OPT_SAX) && vm.count(OPT_DOM)) {
			cerr << "Can't specify both sax & dom\n";
			cerr << desc << "\n";
			return EXIT_ERRARGS_PARSE_TYPE;
		}
		sax = vm.count(OPT_SAX) != 0;
	}

	try {
		if (jsonEngine == ENGINE_YAJL) {
			json::yajl::CallbackType ct;
			if (vm.count(OPT_NOOP_CALLBACKS))
				ct = json::yajl::CallbacksNoop;
			else
				ct = json::yajl::CallbacksNone;
			json::yajl::Utf8Mode utf8 = utf8Validation ? json::yajl::Utf8Validate : json::yajl::Utf8DontValidate;
			benchmark = new json::yajl::Benchmark(jsonInput, utf8, ct);
		} else if (jsonEngine == ENGINE_PBNJSON_C) {
			if (sax)
				benchmark = new json::pbnjson::c::Benchmark<json::pbnjson::SAX>(jsonInput, schemaPath);
			else
				benchmark = new json::pbnjson::c::Benchmark<json::pbnjson::DOM>(jsonInput, schemaPath);
		} else if (jsonEngine == ENGINE_PBNJSON_CPP) {
			if (sax)
				benchmark = new json::pbnjson::cpp::Benchmark<json::pbnjson::SAX>(jsonInput, schemaPath);
			else
				benchmark = new json::pbnjson::cpp::Benchmark<json::pbnjson::DOM>(jsonInput, schemaPath);
		} else if (jsonEngine == ENGINE_MJSON) {
			benchmark = new json::mjson::Benchmark(jsonInput);
		} else if (jsonEngine == ENGINE_CJSON) {
			benchmark = new json::cjson::Benchmark(jsonInput);
		} else
			benchmark = new json::Benchmark(jsonEngine, jsonInput);

		if (vm.count(OPT_TEST_ITERATIONS)) {
			double runtime = benchmark->execute(iterations);
			cerr << runtime << "\n";
		} else {
			size_t iterations = benchmark->execute(runLength);
			cout << iterations << "\n";
		}
	} catch(const runtime_error& e) {
		cerr << "Exception running test for " << jsonEngine << ": " << e.what() << "\n";
		return EXIT_RUN_ERROR;
	} catch(...) {
		cerr << "Unhandled exception\n";
		return EXIT_RUN_ERROR;
	}

	return 0;
}

