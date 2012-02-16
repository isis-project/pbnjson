# Try to find pbnjson library
# This will define
# CJSON_FOUND - system has cjson 
# CJSON_INCLUDE_DIRS - include directories necessary to compile w/ cjson
# CJSON_LIBRARIES - libraries necessary to link to to get cjson
include (LibFindMacros)

# Include directories
find_path(CJSON_INCLUDE_DIR NAMES cjson/json.h)

# Find the library
if (CJSON_DEBUG)
	message("Looking for CJSON debug")
	find_library(CJSON_LIBRARY NAMES cjson-d)
elseif (CJSON_STATIC)
	message("Looking for CJSON static")
	find_library(CJSON_LIBRARY NAMES cjson_s)
else ()
	message("Looking for CJSON library")
	find_library(CJSON_LIBRARY NAMES cjson)
endif ()

# let LibFindMacros take care of the rest
set(CJSON_PROCESS_INCLUDES CJSON_INCLUDE_DIR)
set(CJSON_PROCESS_LIBS CJSON_LIBRARY)

# LibFindMacros takes care of the rest
libfind_process(CJSON)
