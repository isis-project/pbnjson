# Try to find pbnjson library
# This will define
# YAJL_FOUND - system has yajl
# YAJL_INCLUDE_DIRS - include directories necessary to compile w/ yajl
# YAJL_LIBRARIES - libraries necessary to link to to get yajl
include (LibFindMacros)

set(YAJL_INCLUDE_DIR $ENV{MY_YAJL_INCLUDE_PATH})
set(YAJL_LIBRARY $ENV{MY_YAJL_LIB_PATH})

# Include directories
find_path(YAJL_INCLUDE_DIR NAMES yajl/yajl_parse.h yajl/yajl_gen.h yajl/yajl_common.h)

# Find the library
if (YAJL_STATIC)
	find_library(YAJL_LIBRARY NAMES yajl_s)
else ()
	find_library(YAJL_LIBRARY NAMES yajl)
endif ()

# let LibFindMacros take care of the rest
set(YAJL_PROCESS_INCLUDES YAJL_INCLUDE_DIR)
set(YAJL_PROCESS_LIBS YAJL_LIBRARY)

# LibFindMacros takes care of the rest
libfind_process(YAJL)
