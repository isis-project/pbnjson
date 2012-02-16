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

#include "bench_yajl.h"

using namespace std;
using namespace benchmark::utils;

namespace json {
	namespace yajl {
#ifdef HAVE_YAJL
		int noop_null(void * ctx) { return 1; }
		int noop_boolean(void * ctx, int boolVal) { return 1; }
#if 0
		int noop_integer(void * ctx, long integerVal) { return 1; }
		int noop_double(void * ctx, double doubleVal) { return 1; }
#endif
		/** A callback which passes the string representation of the number
		 *  back to the client.  Will be used for all numbers when present */
		int noop_number(void * ctx, const char * numberVal,
							unsigned int numberLen) { return 1; }

		/** strings are returned as pointers into the JSON text when,
		 * possible, as a result, they are _not_ null padded */
		int noop_string(void * ctx, const unsigned char * stringVal,
							unsigned int stringLen) { return 1; }

		int noop_start_map(void * ctx) { return 1; }
		int noop_map_key(void * ctx, const unsigned char * key,
							 unsigned int stringLen) { return 1; }
		int noop_end_map(void * ctx) { return 1; }

		int noop_start_array(void * ctx) { return 1; }
		int noop_end_array(void * ctx) { return 1; }
		static yajl_callbacks *callbacks;
		static yajl_parser_config config;
#endif

		Benchmark::Benchmark(const string &input, Utf8Mode mode, CallbackType type)
			: json::Benchmark("yajl", input)
		{
#ifdef HAVE_YAJL
			static yajl_callbacks noCallbacks = { 0 };
			static yajl_callbacks noopCallbacks = {
				noop_null, // yajl_null
				noop_boolean, // yajl_boolean
				NULL, // yajl_integer
				NULL, // yajl_double
				noop_number, // yajl_number
				noop_string, // yajl_stirng
				noop_start_map, // yajl_start_map
				noop_map_key, // yajl_map_key
				noop_end_map, // yajl_end_map
				noop_start_array, // yajl_start_array
				noop_end_array, // yajl_end_array
			};
			config.allowComments = 0;
			config.checkUTF8 = mode == Utf8Validate;
			switch (type) {
				case json::yajl::CallbacksNone:
					callbacks = &noCallbacks;
					break;
				case json::yajl::CallbacksNoop:
					callbacks = &noopCallbacks;
					break;
			}
#endif /* HAVE_YAJL */
		}

		Benchmark::~Benchmark()
		{
#ifdef HAVE_YAJL
#endif /* HAVE_YAJL */
		}

#ifdef HAVE_YAJL
		raw_buffer Benchmark::init() throw(std::runtime_error)
		{
			raw_buffer inputBuffer = input();
			m_parser = yajl_alloc(callbacks, &config, NULL, NULL);
			yajl_status result = yajl_parse(m_parser, (const unsigned char *)inputBuffer.m_str, inputBuffer.m_len);
			yajl_free(m_parser);
			if (result != yajl_status_ok)
				throw runtime_error("Failed to parse json input");
			return inputBuffer;
		}

		double Benchmark::execute(size_t numIterations) throw(std::runtime_error)
		{
			Timer start, end;
			raw_buffer input = init();

			start.reset();
			while (numIterations--) {
				m_parser = yajl_alloc(callbacks, &config, NULL, NULL);
				yajl_parse(m_parser, (const unsigned char *)input.m_str, input.m_len);
				yajl_free(m_parser);
			}
			end.reset();

			return end - start;
		}

		size_t Benchmark::execute(double runtime) throw(std::runtime_error)
		{
			raw_buffer input = init();
			Timer end;
			double start = Timer::now();
			size_t numIterations = 0;

			for (; end - start < runtime; end.reset()) {
				m_parser = yajl_alloc(callbacks, &config, NULL, NULL);
				yajl_parse(m_parser, (const unsigned char *)input.m_str, input.m_len);
				yajl_free(m_parser);
				numIterations++;
			}

			return numIterations;
		}
#endif /* HAVE_YAJL */
	}
}
