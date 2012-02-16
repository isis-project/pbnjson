# Try to find V8 Javascript engine
# This will define
# V8_Juice_FOUND - system has v8
# V8_Juice_INCLUDE_DIRS - the v8 include directory and any dependancies
# V8_Juice_LIBRARIES - the v8 library and any dependancies

include(LibFindMacros)

# Dependencies
libfind_package(V8_Juice V8)

# Include directories
find_path(V8_Juice_INCLUDE_DIR NAMES v8/juice/juice.h)

# Find the library
#find_library(V8_LIBRARY NAMES v8)

# let LibFindMacros take care of the rest
set(V8_Juice_PROCESS_INCLUDES V8_Juice_INCLUDE_DIR V8_INCLUDE_DIRS)
#set(V8_Juice_PROCESS_LIBS V8_Juice_LIBRARY V8_LIBRARIES)
set(V8_Juice_PROCESS_LIBS V8_LIBRARIES)
libfind_process(V8_Juice)
