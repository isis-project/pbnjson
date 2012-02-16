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

#include "bench_pbnjson.h"

using namespace std;
using namespace boost;
using namespace benchmark::utils;

namespace json {
	namespace pbnjson {
		namespace c {
			template <typename T>
			Benchmark<T>::Benchmark(const std::string& inputFile, const std::string& schemaPath) throw (std::runtime_error)
				: json::Benchmark("pbnjson_c", inputFile)
			{
				jschema_ref inputSchema;
				if (schemaPath.empty()) {
					inputSchema = jschema_all();
				} else {
					inputSchema = jschema_parse_file(schemaPath.c_str(), NULL);
					if (inputSchema == NULL)
						throw runtime_error("Unable to generate schema from " + schemaPath);
				}
				jschema_info_init(&m_schema, inputSchema, NULL, NULL);
			}

			template <typename T>
			Benchmark<T>::~Benchmark()
			{
				jschema_release(&m_schema.m_schema);
			}

			template <typename T>
			inline bool parse(raw_buffer &input, JSchemaInfo &schemaInfo);

			template <>
			inline bool parse<SAX>(raw_buffer &input, JSchemaInfo &schemaInfo)
			{
				return jsax_parse(NULL, input, &schemaInfo);
			}

			template <>
			inline bool parse<DOM>(raw_buffer &input, JSchemaInfo &schemaInfo)
			{
				bool ok;
				jvalue_ref parsed = jdom_parse(input, DOMOPT_NOOPT, &schemaInfo);
				ok = !jis_null(parsed);
				j_release(&parsed);
				return ok;
			}

			template <typename T>
			raw_buffer Benchmark<T>::init() throw (std::runtime_error)
			{
				raw_buffer inputBuffer = input();
				Timer start, end;

				if (!parse<T>(inputBuffer, m_schema))
					throw runtime_error("Failed to parse input");

				return inputBuffer;
			}

			template <typename T>
			double Benchmark<T>::execute(size_t numIterations) throw (std::runtime_error)
			{
				Timer start, end;
				raw_buffer inputBuffer = init();

				start.reset();
				while(numIterations--) {
					parse<T>(inputBuffer, m_schema);
				}
				end.reset();

				return end - start;
			}

			template <typename T>
			size_t Benchmark<T>::execute(double runTime) throw (std::runtime_error)
			{
				raw_buffer inputBuffer = init();
				size_t numIterations = 0;

				double start = Timer::now();
				for (Timer end; end - start < runTime; end.reset()) {
					parse<T>(inputBuffer, m_schema);
					numIterations++;
				}

				return numIterations;
			}
		}

		namespace cpp {
			using namespace ::pbnjson;

			template <typename T>
			Benchmark<T>::Benchmark(const std::string& inputFile, const std::string& schemaPath) throw (std::runtime_error)
				: json::Benchmark("pbnjson_cpp", inputFile)
			{
				m_schema = new JSchemaFile(schemaPath);
				if (!m_schema->isInitialized())
					throw runtime_error("Failed to initialize schema " + schemaPath);
			}

			template <typename T>
			Benchmark<T>::~Benchmark()
			{
				delete m_schema;
			}

			class NoopSaxparser : public JParser {
			public:
				NoopSaxparser() : JParser(NULL) {}
				virtual bool jsonObjectOpen() { return true; }
				virtual bool jsonObjectKey(const std::string& key) { return true; }
				virtual bool jsonObjectClose() { return true; }
				virtual bool jsonArrayOpen() { return true; }
				virtual bool jsonArrayClose() { return true; }
				virtual bool jsonString(const std::string& s) { return true; }
				virtual bool jsonNumber(const std::string& n) { return true; }
				virtual bool jsonNumber(int64_t number) { return false; }
				virtual bool jsonNumber(double &number, ConversionResultFlags asFloat) { return false; }
				virtual bool jsonBoolean(bool truth) { return true; }
				virtual bool jsonNull() { return true; }
				virtual NumberType conversionToUse() const { return JNUM_CONV_RAW; }
			};

			template <class T>
			inline bool parse(const std::string& input, JSchema *schema);

			template <>
			inline bool parse<SAX>(const std::string& input, JSchema *schema)
			{
				NoopSaxparser parser;
				return parser.parse(input, *schema, NULL);
			}

			template <>
			inline bool parse<DOM>(const std::string& input, JSchema *schema)
			{
				JDomParser parser;
				bool ok;
				ok = parser.parse(input, *schema, NULL);
				JValue j = parser.getDom();
				return ok;
			}

			template <typename T>
			string Benchmark<T>::init() throw (std::runtime_error)
			{
				raw_buffer inputBuffer = input();
				string inputStr(inputBuffer.m_str, inputBuffer.m_len);
				Timer start, end;

				if (!parse<DOM>(inputStr, m_schema))
					throw runtime_error("Failed to parse input");

				return inputStr;
			}

			template <typename T>
			double Benchmark<T>::execute(size_t numIterations) throw (std::runtime_error)
			{
				Timer start, end;
				string input = init();

				start.reset();
				while(numIterations--) {
					parse<T>(input, m_schema);
				}
				end.reset();

				return end - start;
			}

			template <typename T>
			size_t Benchmark<T>::execute(double runtime) throw (std::runtime_error)
			{
				string input = init();
				size_t numIterations = 0;

				double start = Timer::now();
				for (Timer end; end - start < runtime; end.reset()) {
					parse<T>(input, m_schema);
					numIterations++;
				}

				return numIterations;
			}

		}
	}
}
