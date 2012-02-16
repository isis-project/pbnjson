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

#ifndef BENCH_JSON_H_
#define BENCH_JSON_H_

#include <string>
#include <pbnjson.h>

#include <stdexcept>

namespace benchmark {
	namespace utils {
		class MemoryMap {
		public:
			enum Options {
				MapReadOnly,
				MapReadWrite = 1,
				MapPersist = MapReadWrite | 2,
			};
			MemoryMap(const std::string &inputFile, Options mapType = MapReadOnly);
			~MemoryMap();

			template <typename T>
			const T* map() const
			{
				return static_cast<const T*>(m_map);
			}

			template <typename T>
			T* map()
			{
				return static_cast<T*>(m_map);
			}

			size_t size() const
			{
				return m_size;
			}

			operator raw_buffer()
			{
				raw_buffer result;
				result.m_str = this->map<char>();
				result.m_len = size();
				return result;
			}

		private:
			void *m_map;
			size_t m_size;
		};

		class Timer {
		public:
			operator double() const
			{
				return t.tv_sec + t.tv_nsec / 1000000000.0;
			}

			double reset()
			{
				clock_gettime(CLOCK_MONOTONIC, &t);
				return (double)*this;
			}

			Timer()
			{
				reset();
			}

			double operator-(double other) const
			{
				return ((double)*this) - other;
			}

			double operator-(const Timer& other) const
			{
				return ((double)*this) - (double)other;
			}

			static double now()
			{
				Timer t;
				return (double)t;
			}

		private:
			struct timespec t;
		};
	}
}

namespace json {
	class Benchmark {
	public:
		Benchmark();
		Benchmark(const std::string& engine, const std::string& inputFile) throw(std::runtime_error);
		virtual ~Benchmark();

		virtual double execute(size_t numIterations) throw(std::runtime_error);
		virtual size_t execute(double runTime) throw(std::runtime_error);

	protected:
		raw_buffer input() const
		{
			raw_buffer result;
			result.m_str = m_input->map<const char>();
			result.m_len = m_input->size();
			return result;
		}

	private:
		std::string m_engineName;
		benchmark::utils::MemoryMap *m_input;
	};
}

#endif /* BENCH_JSON_H_ */
