# Try to find mjson library
# This will define
# MJSON_FOUND - system has mjson
# MJSON_INCLUDE_DIRS - the mjson include directory and any dependancies
# MJSON_LIBRARIES - the mjson library and any dependancies

include(LibFindMacros)

# Include directories
find_path(MJSON_INCLUDE_DIR NAMES json.h)

# Find the library
find_library(MJSON_LIBRARY NAMES mjson)

# let LibFindMacros take care of the rest
set(MJSON_PROCESS_INCLUDES MJSON_INCLUDE_DIR)
set(MJSON_PROCESS_LIBS MJSON_LIBRARY)
libfind_process(MJSON)
