project(PalmsBetterNativeJSON)
cmake_minimum_required(VERSION 2.6)

if (APPLE)
	set(CMAKE_OSX_ARCHITECTURES i386 x86_64)
endif ()

# Project-wide information for the language bindings to find comon directories
set(SRC_BASE ${PROJECT_SOURCE_DIR})
set(BUILD_SRC_BASE ${CMAKE_BINARY_DIR})
set(API_HEADERS ${SRC_BASE}/api)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/" ${CMAKE_MODULE_PATH})

if (NOT DEFINED PBNJSON_MAJOR)
	set(PBNJSON_MAJOR 1)
endif (NOT DEFINED PBNJSON_MAJOR)

if (NOT DEFINED PBNJSON_MINOR)
	set(PBNJSON_MINOR 1)
endif (NOT DEFINED PBNJSON_MINOR)

if (NOT DEFINED PBNJSON_MICRO)
	set(PBNJSON_MICRO 0)
endif (NOT DEFINED PBNJSON_MICRO)

set(PBNJSON_VERSION ${PBNJSON_MAJOR}.${PBNJSON_MINOR}.${PBNJSON_MICRO})

# The information about the yajl version being used (include directories & whatnot)
set(YAJL_MAJOR 1)
set(YAJL_MINOR 0)
set(YAJL_MICRO 7)
set(YAJL_VERSION ${YAJL_MAJOR}.${YAJL_MINOR}.${YAJL_MICRO})

macro(PICK_ENGINE_NAME VARIABLE_NAME STATIC_VALUE SHARED_VALUE STATIC_ENGINE)
    if(${STATIC_ENGINE})
        set(${VARIABLE_NAME} ${STATIC_VALUE})
        message("${VARIABLE_NAME} is linking to static engine ${${VARIABLE_NAME}}")
    else(${STATIC_ENGINE})
        set(${VARIABLE_NAME} ${SHARED_VALUE})
        message("${VARIABLE_NAME} is linking to shared engine ${${VARIABLE_NAME}}")
    endif(${STATIC_ENGINE})
endmacro(PICK_ENGINE_NAME VARIABLE_NAME STATIC_VALUE SHARED_VALUE STATIC_ENGINE)

set(C_ENGINE yajl)
set(STATIC_C_ENGINE "FALSE")
set(CXX_ENGINE yajl)
set(STATIC_CXX_ENGINE "FALSE")

set(EXTERNAL_YAJL TRUE)

if (NOT EXTERNAL_YAJL)
	# the name of the library to link against - for now we link against the static version
	# since no one else should be using the engine directly

	# the x.y.z version of the engine
	set(C_ENGINE_VERSION ${YAJL_VERSION})
	# where the engine's cmake project is located
	set(C_ENGINE_DIR pjson_engine/${C_ENGINE})
	set(C_ENGINE_SRC_DIR ${SRC_BASE}/${C_ENGINE_DIR})
	# where the engine will build everything (e.g. include, bin, doc, etc)
	set(C_ENGINE_INSTALLDIR ${BUILD_SRC_BASE}/${C_ENGINE_DIR}/${C_ENGINE}-${C_ENGINE_VERSION})
	# the include directory for the engine api
	set(C_ENGINE_INCDIR ${C_ENGINE_INSTALLDIR}/include)
	message("include directory is ${C_ENGINE_INCDIR}")
	# the library search directory for the engine
	set(C_ENGINE_LIBDIR ${C_ENGINE_INSTALLDIR}/lib)
endif()
PICK_ENGINE_NAME(C_ENGINE_LIBNAME ${C_ENGINE}_s ${C_ENGINE} STATIC_C_ENGINE)

# the C++ equivalent of the above
if (NOT EXTERNAL_YAJL)
	set(CXX_ENGINE_VERSION ${YAJL_VERSION})
	set(CXX_ENGINE_DIR pjson_engine/${CXX_ENGINE})
	set(CXX_ENGINE_SRC_DIR ${SRC_BASE}/${CXX_ENGINE_DIR})
	set(CXX_ENGINE_INSTALLDIR ${BUILD_SRC_BASE}/${CXX_ENGINE_DIR}/${CXX_ENGINE}-${CXX_ENGINE_VERSION})
	set(CXX_ENGINE_INCDIR ${CXX_ENGINE_INSTALLDIR}/include)
	set(CXX_ENGINE_LIBDIR ${CXX_ENGINE_INSTALLDIR}/lib)
	add_subdirectory(pjson_engine/${C_ENGINE})
endif ()
PICK_ENGINE_NAME(CXX_ENGINE_LIBNAME ${CXX_ENGINE}_s ${CXX_ENGINE} STATIC_CXX_ENGINE)

########## CONFIGURE THE COMPILERS FOR THE LIBRARIES ############################
message(STATUS "C engine's include directory is ${C_ENGINE_INCDIR}")
message(STATUS "C++ engine's include directory is ${CXX_ENGINE_INCDIR}")

add_definitions(-D_GNU_SOURCE)
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
set(WITH_DEBUG_POINTERS TRUE CACHE BOOL "In debug builds, perform extra checks on pointer manipulations")
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
	set(WITH_DEBUG_POINTERS FALSE CACHE BOOL "" FORCE)
endif()
if (WITH_DEBUG_POINTERS)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG_FREED_POINTERS=1")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG_FREED_POINTERS=1")
endif ()
if(CMAKE_COMPILER_IS_GNUCC)
	# std=c99: The C library will be C99 compliant (necessary for some stuff).
	# fmessage-length=0: Make sure that errors & messages are printed on 1 line
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fmessage-length=0 -Wall -Werror -pipe")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DDEBUG -D_DEBUG -DPJSON_LOG_INFO -DPJSON_LOG_STDOUT")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG -g")
else(CMAKE_COMPILER_IS_GNUCC)
	# don't take out this message - this is the default fall-through case when the
	# compiler cannot be determined.  instead, add compiler detection as necessary
	message("WARNING - not using GCC for compilation - please provide flags to enable C99")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_BSD_SOURCE")
endif(CMAKE_COMPILER_IS_GNUCC)

if(CMAKE_COMPILER_IS_GNUCXX)
	# fmessage-length=0: Make sure that errors & messages are printed on 1 line
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmessage-length=0 -Wall -Werror -pipe")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DDEBUG -D_DEBUG -DPJSON_LOG_INFO -DPJSON_LOG_STDOUT")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -g")
else(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_BSD_SOURCE")
endif(CMAKE_COMPILER_IS_GNUCXX)
########## CONFIGURE THE COMPILERS FOR THE LIBRARIES ############################

set(WITH_DOCS ${DOXYGEN_FOUND} CACHE BOOL "Generate documentation when building")
if (DEFINED DONT_PREPARE_DOCS)
	# Variable compatibility with existing build scripts
	# Deprecated in favour of WITH_DOCS
	set(WITH_DOCS NOT ${DONT_PREPARE_DOCS} CACHE BOOL "Generate documentation when building" FORCE)
endif()
if (WITH_DOCS)
	INCLUDE(PBNJSONDoc.cmake)
else()
	message(STATUS "Skipping document generation")
endif ()

# build the JSON engine for the C library

# if the C++ JSON engine being used is different from the C one, then build it too
# CMake doesn't seem to be able to handle the same directory being added twice
if (DEFINED CXX_ENGINE AND NOT ${C_ENGINE} STREQUAL ${CXX_ENGINE})
	add_subdirectory(pjson_engine/${CXX_ENGINE})
endif (DEFINED CXX_ENGINE AND NOT ${C_ENGINE} STREQUAL ${CXX_ENGINE} )

set(WITH_STATIC TRUE CACHE BOOL "Build with static pbnjson library")
set(WITH_SCHEMA TRUE CACHE BOOL "Build with schema support.  If built without, no input will fail schema validation.")
if (WITH_SCHEMA)
	set(WITH_PCRE TRUE CACHE BOOL "Build with PCRE support in schemas")
else ()
	set(WITH_PCRE FALSE CACHE BOOL "Build with PCRE support in schemas" FORCE)
endif ()

if (WITH_PCRE AND NOT WITH_SCHEMA)
	message(FATAL_ERROR "Conflicting option: set WITH_PCRE but schemas aren't enabled")
endif()

if (WITH_PCRE)
	if (EXISTS pjson_engine/pcre)
		add_subdirectory(pjson_engine/pcre)
		set(PCRE_LIBRARIES "pcre" "pcreposix")
		set(LOCAL_PCRE true)
	endif (EXISTS pjson_engine/pcre)
endif()

# Variable compatibility with existing build scripts
# Deprecated in favour of WITH_TESTS
set(WITH_TESTS FALSE CACHE BOOL "Control building of unit tests")
if (DEFINED NO_TESTS)
    set(WITH_TESTS NOT ${NO_TESTS} CACHE BOOL "Control building of unit tests" FORCE)
endif()

set(WITH_VERBOSE_DEBUG FALSE CACHE BOOL "Enable verbose debug logging")
set(WITH_VERBOSE_TRACE FALSE CACHE BOOL "Enable tracing debug logging")

if (DEFINED WITH_V8)
	if (WITH_V8)
		find_package(V8 REQUIRED)
	endif ()
else ()
	find_package(V8 QUIET)
endif ()
set(WITH_V8 ${V8_FOUND} CACHE BOOL "Build v8 javascript bindings")

# build the language bindings
add_subdirectory(pjson_c)
add_subdirectory(pjson_cxx)
if (V8_FOUND)
	add_subdirectory(pjson_v8)
endif()

if (WITH_TESTS)
    set(WITH_QTCREATOR FALSE CACHE BOOL "Enable better Qt Creator integration")
    enable_testing()
    add_subdirectory(test)
else()
	message(STATUS "Skipping all unit tests")
endif()

set(WITH_BENCHMARKS ${WITH_TESTS} CACHE BOOL "Build benchmarks to compare various implementations")
if (WITH_BENCHMARKS)
	add_subdirectory(benchmarks)
else()
	message(STATUS "Skipping benchmarks")
endif()

add_custom_target (deb 
	dpkg-buildpackage "-rfakeroot")


