include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckCXXSourceCompiles)
include(CheckFunctionExists)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_include_files("malloc.h" HAVE_MALLOC_H)
check_include_files("syslog.h" HAVE_SYSLOG_H)
check_include_files("regex.h" HAVE_POSIX_REGEXP)
check_symbol_exists("vsyslog" "stdarg.h;syslog.h" HAVE_VSYSLOG)
check_symbol_exists("vfprintf" "stdarg.h;stdio.h" HAVE_VFPRINTF)
check_symbol_exists("__assert_fail" "assert.h" HAVE_ASSERT_FAIL)
check_function_exists(strnlen HAVE_STRNLEN)
check_function_exists(isatty HAVE_ISATTY)

# Check for GCC atomics	
check_cxx_source_compiles(	
	"int main() {
		int f;
		__sync_fetch_and_add(&f, 1);
		__sync_fetch_and_sub(&f, 1);
		__sync_add_and_fetch(&f, 1);
		__sync_sub_and_fetch(&f, 1);
		__sync_bool_compare_and_swap(&f, 1, 0);
		return 0;
	}"
	HAVE_GCC_ATOMICS)

if (WITH_PCRE)
	if (NOT LOCAL_PCRE)
	    find_package(PCRE)
	    if (PCRE_FOUND)
		set(HAVE_PCRE 1)
	    else (PCRE_FOUND)
		set(PCRE_LIBRARIES "")
	    endif (PCRE_FOUND)
	endif (NOT LOCAL_PCRE)
endif()

STRING(TOUPPER ${C_ENGINE} C_ENGINE_PKG)
if (EXTERNAL_${C_ENGINE_PKG})
	set(${C_ENGINE_PKG}_STATIC ${STATIC_C_ENGINE})
	find_package(${C_ENGINE_PKG} REQUIRED)
	set(C_ENGINE_INCDIR ${${C_ENGINE_PKG}_INCLUDE_DIRS})
	set(C_ENGINE_LIBNAME ${${C_ENGINE_PKG}_LIBRARIES})
endif()
message(STATUS "pbnjson_c: ${C_ENGINE} include directory ${C_ENGINE_INCDIR}, library ${C_ENGINE_LIBNAME}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sys_malloc.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/sys_malloc.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pjson_syslog.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/pjson_syslog.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/regexp.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/regexp.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/strnlen.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/strnlen.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/isatty.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/isatty.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/assert_compat.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/assert_compat.h)

set(SHARED_SOURCE
    jgen_stream.c 
    jobject.c
    jschema.c
    jvalue/value.c
    jvalue/object.c
    jvalue/array.c
    jvalue/number.c
    jvalue/num_conversion.c
    jparse_stream.c
    debugging.c
    )

set(STATIC_SOURCE ${SHARED_SOURCE})
    
add_library(pbnjson_c SHARED ${SHARED_SOURCE})
target_link_libraries(pbnjson_c ${C_ENGINE_LIBNAME} ${PCRE_LIBRARIES})
set_target_properties(pbnjson_c PROPERTIES DEFINE_SYMBOL PJSON_SHARED)

if (WITH_STATIC)
	add_library(pbnjson_c_s STATIC ${STATIC_SOURCE})
	target_link_libraries(pbnjson_c_s ${C_ENGINE_LIBNAME} ${PCRE_LIBRARIES})
endif ()

include_directories(${API_HEADERS} ${API_HEADERS}/pbnjson ${API_HEADERS}/pbnjson/c ${C_ENGINE_INCDIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
if (C_ENGINE_LIBDIR)
	link_directories(${C_ENGINE_LIBDIR})
endif ()

if(WITH_VERBOSE_DEBUG)
	add_definitions(-DPJSON_LOG_DBG=1)
endif ()
if(WITH_VERBOSE_TRACE)
	add_definitions(-DPJSON_LOG_TRACE=1)
	add_definitions(-DPJSON_SCHEMA_TRACE=3)
endif ()
add_definitions(-DLIBRARY_NAME=pbnjson_c)
add_definitions(-DPJSON_EXPORT)

if (NOT WITH_SCHEMA)
	message(STATUS "Compiling without schema support: WITH_SCHEMA=${WITH_SCHEMA}")
	add_definitions(-DBYPASS_SCHEMA=1)
else()
	message(STATUS "Compiling with schema support: WITH_SCHEMA=${WITH_SCHEMA}")
	add_definitions(-DBYPASS_SCHEMA=0)
endif()

install(DIRECTORY ${API_HEADERS}/pbnjson/c/ DESTINATION include/pbnjson/c/ PATTERN "*.swp" EXCLUDE)
install(FILES ${API_HEADERS}/pbnjson.h ${API_HEADERS}/pbnjson_experimental.h DESTINATION include/)
install(TARGETS pbnjson_c LIBRARY DESTINATION lib${LIB_SUFFIX}/)
