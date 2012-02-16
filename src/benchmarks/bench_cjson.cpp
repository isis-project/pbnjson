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

#include "bench_cjson.h"

#include <exception>

using namespace std;
using namespace benchmark::utils;

namespace json {
	namespace cjson {
		Benchmark::Benchmark(const string &input) throw(std::runtime_error)
			: json::Benchmark("cjson", input)
		{
		}

		Benchmark::~Benchmark()
		{
		}

#ifdef HAVE_CJSON
		string Benchmark::init() throw(std::runtime_error)
		{
			shared_ptr<MemoryMap> inputMMap = input();
			raw_buffer inputBuffer = (raw_buffer)*inputMMap;
			string inputStr(inputBuffer.m_str, inputBuffer.m_len);
			json_t *parsed = json_parse_document(inputStr.c_str());
			if (!parsed)
				throw runtime_error("Failed to parse json input");
			json_free_value(&parsed);
			return inputStr;
		}

		double Benchmark::execute(size_t numIterations) throw(std::runtime_error)
		{
			Timer start, end;
			json_t *parsed;
			string inputStr = init();

			start.reset();
			while (numIterations--) {
				parsed = json_parse_document(inputStr.c_str());
				json_free_value(&parsed);
			}
			end.reset();

			return end - start;
		}

		size_t Benchmark::execute(double runtime) throw(std::runtime_error)
		{
			string inputStr = init();
			Timer end;
			double start = Timer::now();
			size_t numIterations = 0;
			json_t *parsed;

			for (; end - start < runtime; end.reset()) {
				parsed = json_parse_document(inputStr.c_str());
				json_free_value(&parsed);
				numIterations++;
			}

			return numIterations;
		}
#endif /* HAVE_MJSON */
	}
}
