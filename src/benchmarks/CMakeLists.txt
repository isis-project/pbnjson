find_package(Boost REQUIRED program_options)

# Helpful macro to make adding a json library into the framework 
# to benchmark much easier 
macro(add_json_engine library)
	string(TOUPPER "${library}" UPPER_LIBNAME)
	string(TOLOWER "${library}" LOWER_LIBNAME)
	find_package(${UPPER_LIBNAME})
	if (${UPPER_LIBNAME}_FOUND)
		string(TOUPPER "-DHAVE_${library}" LIB_MACRO)
		string(TOUPPER "${library}_INCLUDE_DIRS" HEADERS)
		string(TOLOWER "bench_${library}.cpp" SOURCE)
		string(TOUPPER "${library}_LIBRARIES" LIBS)
		
		message(STATUS "Including benchmark headers ${${UPPER_LIBNAME}_INCLUDE_DIRS}")
		include_directories(${${UPPER_LIBNAME}_INCLUDE_DIRS})
		add_definitions(-DHAVE_${UPPER_LIBNAME})
		list(APPEND BENCH_LIBRARIES ${${UPPER_LIBNAME}_LIBRARIES})
	else ()
		message(WARNING "Benchmarks: ${UPPER_LIBNAME} not found - benchmarks unavailable")
	endif ()
	# In any case, we need the source file to implement the functions
	list(APPEND BENCH_SOURCE "bench_${LOWER_LIBNAME}.cpp")
endmacro()

include_directories(${Boost_INCLUDE_DIRS} ${API_HEADERS})

# Libraries to link in to all benchmark executables
list(APPEND BENCH_LIBRARIES
	${Boost_LIBRARIES}
	rt
	pbnjson_c
	pbnjson_cpp
)
	
# Source files for benchmark framework
set(BENCH_SOURCE
	bench.cpp
	bench_json.cpp
#	bench_pbnjson.cpp
)

# JSON source files to used in benchmarking
list(APPEND TEST_SOURCE_FILES ohai.json)

foreach (i ${TEST_SOURCE_FILES})
	configure_file(${i} ${i} COPYONLY)
endforeach()

add_json_engine(yajl)
add_json_engine(cjson)
add_json_engine(mjson)

add_executable(bench_json ${BENCH_SOURCE})
target_link_libraries(bench_json ${BENCH_LIBRARIES})
