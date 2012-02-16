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

#ifndef BENCH_YAJL_H_
#define BENCH_YAJL_H_

#include "bench_json.h"
#ifdef HAVE_YAJL
#include <yajl/yajl_parse.h>
#endif

namespace json {
	namespace yajl {
		enum Utf8Mode {
			Utf8DontValidate,
			Utf8Validate,
		};

		enum CallbackType {
			CallbacksNone,
			CallbacksNoop,
		};

		class Benchmark : public json::Benchmark {
		public:
			Benchmark(const std::string &input, Utf8Mode mode, CallbackType type);
			~Benchmark();

#ifdef HAVE_YAJL
			double execute(size_t numIterations) throw(std::runtime_error);
			size_t execute(double runtime) throw(std::runtime_error);
#endif

		private:
#ifdef HAVE_YAJL
			raw_buffer init() throw(std::runtime_error);
#endif

#ifdef HAVE_YAJL
			yajl_handle m_parser;
#endif
		};
	}
}

#endif /* BENCH_YAJL_H_ */
