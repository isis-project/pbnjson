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

#include <JSchemaFile.h>

#include <cstdio>
#include <cassert>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../pjson_c/liblog.h"

#include <pbnjson.h>

namespace pbnjson {

JSchemaFile::MMapResource::MMapResource(Map &schemaFile, jschema_ref schema)
	: JSchema::Resource(new Map(schemaFile), schema, JSchema::Resource::TakeSchema)
{
}

JSchemaFile::MMapResource::~MMapResource()
{
	Map *schemaMap = static_cast<Map *>(data());
	munmap(schemaMap->data, schemaMap->size);
	delete schemaMap;
}

JSchema::Resource* JSchemaFile::createSchemaMap(const std::string &path)
{
	Resource *result;
	int fd = open(path.c_str(), O_RDONLY);
	result = createSchemaMap(fd);
	close(fd);
	return result;
}

JSchema::Resource* JSchemaFile::createSchemaMap(int fd)
{
	Map map;
	if (!initSchemaMap(fd, map))
		return NULL;

	raw_buffer schemaContents;
	schemaContents.m_str = (char *)map.data;
	schemaContents.m_len = map.size;

	jschema_ref parsed = jschema_parse(schemaContents, JSCHEMA_INPUT_OUTLIVES_DOM | JSCHEMA_DOM_INPUT_NOCHANGE, NULL);

	return new MMapResource(map, parsed);
}

bool JSchemaFile::initSchemaMap(int fd, Map &map)
{
	if (fd < 0) {
		// TODO: Log error
		return false;
	}

	struct stat finfo;
	if (-1 == fstat(fd, &finfo)) {
		// TODO: Log error
		return false;
	}

	if (finfo.st_size <= 0 || !S_ISREG(finfo.st_mode)) {
		// TODO: Log error
		return false;
	}

	void *data = mmap(NULL, finfo.st_size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
	if (MAP_FAILED == data) {
		// TODO: Log error
		return false;
	}

	// let tell the kernel to be very conservative with the memory
	// map until it's actually used
	madvise(data, finfo.st_size, MADV_SEQUENTIAL | MADV_DONTNEED);

	map.data = data;
	map.size = finfo.st_size;
	return true;
}

JSchemaFile::JSchemaFile(FILE *f)
	: JSchema(createSchemaMap(fileno(f)))
{
}

JSchemaFile::JSchemaFile(int fd)
	: JSchema(createSchemaMap(fd))
{
}

JSchemaFile::JSchemaFile(const std::string& path)
	: JSchema(createSchemaMap(path))
{
}

JSchemaFile::JSchemaFile(const JSchemaFile& other)
	: JSchema(other)
{
}

JSchemaFile::~JSchemaFile()
{
}

}

