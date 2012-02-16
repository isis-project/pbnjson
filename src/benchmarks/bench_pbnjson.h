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

#ifndef BENCH_PBNJSON_H_
#define BENCH_PBNJSON_H_

#include "bench_json.h"
#include <pbnjson.h>
#include <pbnjson.hpp>

namespace json {
	namespace pbnjson {
		class SAX {};
		class DOM {};

		namespace c {
			template <typename M>
			class Benchmark : public json::Benchmark {
			public:
				Benchmark(const std::string& inputFile, const std::string& schemaPath) throw (std::runtime_error);
				~Benchmark();

				double execute(size_t numIterations) throw (std::runtime_error);
				size_t execute(double runtime) throw (std::runtime_error);

			private:
				raw_buffer init() throw (std::runtime_error);

				JSchemaInfo m_schema;
			};
		}

		namespace cpp {
			template <typename T>
			class Benchmark : public json::Benchmark {
			public:
				Benchmark(const std::string& inputFile, const std::string& schemaPath) throw (std::runtime_error);
				~Benchmark();

				double execute(size_t numIterations) throw (std::runtime_error);
				size_t execute(double runtime) throw (std::runtime_error);

			private:
				std::string init() throw (std::runtime_error);

				::pbnjson::JSchema *m_schema;
			};
		}
	}
}

#include "bench_pbnjson.cpp"

#endif /* BENCH_PBNJSON_H_ */
