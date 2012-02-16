# Try to find V8 Javascript engine
# This will define
# V8_FOUND - system has v8
# V8_INCLUDE_DIRS - the v8 include directory and any dependancies
# V8_LIBRARIES - the v8 library and any dependancies

include(LibFindMacros)

# Include directories
find_path(V8_INCLUDE_DIR NAMES v8.h)

# Find the library
find_library(V8_LIBRARY NAMES v8)

# let LibFindMacros take care of the rest
set(V8_PROCESS_INCLUDES V8_INCLUDE_DIR)
set(V8_PROCESS_LIBS V8_LIBRARY)
libfind_process(V8)
