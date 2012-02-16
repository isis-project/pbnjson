STRING(TOUPPER ${CXX_ENGINE} CXX_ENGINE_PKG)
if (EXTERNAL_${CXX_ENGINE_PKG})
	set(${CXX_ENGINE_PKG}_STATIC ${STATIC_CXX_ENGINE})
	find_package(${CXX_ENGINE_PKG} REQUIRED)
	set(CXX_ENGINE_INCDIR ${${CXX_ENGINE_PKG}_INCLUDE_DIR})
	set(CXX_ENGINE_LIBNAME ${${CXX_ENGINE_PKG}_LIBRARY})
endif()
message(STATUS "pbnjson_cxx: ${CXX_ENGINE} include directory ${CXX_ENGINE_INCDIR}, library ${CXX_ENGINE_LIBNAME}")

set(SHARED_SOURCE
    JValue.cpp
    JOutputStream.cpp
    JParser.cpp
    JDomParser.cpp
    JGenerator.cpp
    JSchema.cpp
    JSchemaFile.cpp
    JSchemaFragment.cpp
    JResolver.cpp
    ../pjson_c/debugging.c
    )
    
if(WITH_VERBOSE_DEBUG)
	add_definitions(-DPJSON_LOG_DBG=1)
endif ()
if(WITH_VERBOSE_TRACE)
	add_definitions(-DPJSON_LOG_TRACE=1)
	add_definitions(-DPJSON_SCHEMA_TRACE=3)
endif ()
add_definitions(-DPJSONCXX_SHARED)
add_definitions(-DPJSON_SHARED)
add_definitions(-DPJSONCXX_EXPORT)
add_definitions(-DLIBRARY_NAME=pbnjson_cpp)
    
add_library(pbnjson_cpp SHARED ${SHARED_SOURCE})
target_link_libraries(pbnjson_cpp pbnjson_c ${CXX_ENGINE_LIBNAME})

include_directories(${API_HEADERS}
	${API_HEADERS}/pbnjson
	${API_HEADERS}/pbnjson/c
	${API_HEADERS}/pbnjson/cxx
	${CXX_ENGINE_INCDIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/../pjson_c
	${CMAKE_CURRENT_SOURCE_DIR})

if (CXX_ENGINE_LIBDIR)
	link_directories(${CXX_ENGINE_LIBDIR})
endif()

install(DIRECTORY ${API_HEADERS}/pbnjson/cxx/ DESTINATION include/pbnjson/cxx/ PATTERN "*.swp" EXCLUDE)
install(FILES ${API_HEADERS}/pbnjson.hpp DESTINATION include/)
install(TARGETS pbnjson_cpp LIBRARY DESTINATION lib${LIB_SUFFIX}/)
