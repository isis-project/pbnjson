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

#include "bench_json.h"

#include <exception>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <limits>

using namespace std;

namespace benchmark {
	namespace utils {
		MemoryMap::MemoryMap(const string &inputFile, Options mapType)
		{
			int fopts;
			int mopts;
			int mProtOpts;
			int fd;
			struct stat fileInfo;

			switch (mapType) {
				case MapPersist:
					fopts = O_RDWR;
					mopts = MAP_SHARED;
					mProtOpts = PROT_READ | PROT_WRITE;
					break;
				case MapReadWrite:
					fopts = O_RDWR;
					mopts = MAP_PRIVATE;
					mProtOpts = PROT_READ | PROT_WRITE;
					break;
				case MapReadOnly:
					fopts = O_RDONLY;
					mopts = MAP_PRIVATE;
					mProtOpts = PROT_READ;
					break;
				default:
					throw logic_error("Invalid option selected");
			}

			errno = 0;

			fd = open(inputFile.c_str(), fopts);

			if (fd == -1) {
				throw runtime_error("Failed to open input file" + inputFile);
			}

			if (0 != fstat(fd, &fileInfo)) {
				close(fd);
				throw runtime_error("Failed to get file info for " + inputFile);
			}
			m_size = fileInfo.st_size;

			m_map = mmap(NULL, m_size, mProtOpts, mopts, fd, 0);
			close(fd);
			if (m_map == NULL || m_map == MAP_FAILED) {
				throw runtime_error("Failed to memory map file " + inputFile);
			}
		}

		MemoryMap::~MemoryMap()
		{
			if (0 != munmap(m_map, m_size)) {
				throw runtime_error("Failed to unmap memory file");
			}
		}
	}
}

namespace json {
	using namespace benchmark::utils;

	Benchmark::Benchmark()
		: m_engineName("uninitialized")
	{
	}

	Benchmark::Benchmark(const std::string& engine, const std::string& inputFile) throw(std::runtime_error)
		: m_engineName(engine), m_input(new MemoryMap(inputFile))
	{
		if (m_input->size() > numeric_limits<unsigned int>::max())
			throw runtime_error("Input file is too big");
	}

	Benchmark::~Benchmark()
	{
		delete m_input;
	}

	double Benchmark::execute(size_t numIterations) throw(std::runtime_error)
	{
		return execute(0.0);
	}

	size_t Benchmark::execute(double runTime) throw(std::runtime_error)
	{
		throw runtime_error(m_engineName + " is not a supported engine");
	}
}

