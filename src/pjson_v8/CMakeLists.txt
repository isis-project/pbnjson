if (NOT V8_FOUND)
	# internal problem since the parent should be doing a check before including
	message(FATAL_ERROR "Cannot build pbnjson v8 bindings without v8 project")
endif()

add_definitions(-DPJSONCXX_SHARED)
add_definitions(-DPJSON_SHARED)
add_definitions(-DPJSONCXX_EXPORT)
add_definitions(-DLIBRARY_NAME=pbnjson_v8)

include_directories(${API_HEADERS} ${V8_INCLUDE_DIRS})
add_library(pbnjson_v8 SHARED JV8Parser.cpp JV8Binding.cpp JV8LazyString.cpp)
target_link_libraries(pbnjson_v8 ${V8_LIBRARIES})

install(DIRECTORY ${API_HEADERS}/pbnjson/v8 DESTINATION include/pbnjson/cxx/ PATTERN "*.swp" EXCLUDE)
install(FILES ${API_HEADERS}/pbnjson_v8.hpp DESTINATION include/)
install(TARGETS pbnjson_v8 LIBRARY DESTINATION lib${LIB_SUFFIX}/)
