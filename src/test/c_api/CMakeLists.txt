if (NOT QT_FOUND)
    message(WARNING "Qt not found - skipping building many tests for C api")
endif ()

include_directories(
    ${API_HEADERS}
)

# Find CJSON for comparison benchmarks
find_package(CJSON)
if (NOT CJSON_FOUND)
    set(CJSON_INCLUDE_DIR "")
    set(CJSON_LIBRARY "")
    set(HAVE_CJSON false)
endif (NOT CJSON_FOUND)

find_package(YAJL)
if (NOT YAJL_FOUND)
    set (YAJL_INCLUDE_DIRS "")
    set (YAJL_LIBRARIES "")
    set (FOUND_YAJL false)
endif (NOT YAJL_FOUND)

######################### TEST CONFIGURATION ########################
set(TEST_LIBRARIES pbnjson_c ${CJSON_LIBRARY} ${YAJL_LIBRARIES})

include(CheckSymbolExists)
if (CJSON_FOUND)
	check_symbol_exists("json_object_new_object" "json_object.h" HAVE_CJSON)
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cjson.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/cjson.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/yajl.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/yajl.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${YAJL_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})

list(APPEND TEST_PARSE_FILES
	file_parse_test
)

foreach (i ${TEST_PARSE_FILES})
	configure_file(${i}.json ${i}.json COPYONLY)
	configure_file(${i}.schema ${i}.schema COPYONLY)
endforeach()



######################### THE UNIT TESTS ############################
# TestDOM test
qt_hdrs(test_dom
    TestDOM.h
)

src(test_dom
    TestDOM.cpp
)

qt_hdrs(test_parse
	TestParse.h
)

src(test_parse
	TestParse.cpp)

qt_hdrs(test_sax
    TestSAX.h
)

src(test_sax
    TestSAX.cpp
    JSXMLConverter.cpp
)

qt_hdrs(test_yajl
    TestYAJL.h
)
  
src(test_yajl
    TestYAJL.cpp
)

qt_hdrs(test_perf
    TestPerformance.h
)

src(test_perf
    TestPerformance.cpp
)

qt_hdrs(test_schema
    TestSchema.h
)

src(test_schema
    TestSchema.cpp
)
qt_hdrs(test_schema2
	TestSchemaSanity.h
)

src(test_schema2
	TestSchemaSanity.cpp
)

src(smoketest_mem_badinput
    SmokeTestMemLeakBadInput.cpp
)

include_directories(${C_ENGINE_INCDIR})

set(test_dom_test_list
	testObjectSimple
	testObjectComplicated
	testObjectPut
	testArraySimple
	testArrayComplicated
	testStringSimple
	testStringDealloc
	testInteger32Simple
	testInteger320
	testInteger32Limits
	testInteger64Simple
	testInteger640
	testInteger64Limits
	testDouble0
	testDoubleInfinities
	testDoubleNaN
	testDoubleFromInteger
	testBoolean
)

set(test_parse_test_list
	testParseDoubleAccuracy
	testParseFile
)

set(test_sax_test_list
	testGenerator
	testParser
)

set(test_schema2_test_list
	testSimpleSchema
	testSchemaReuse
)

add_qt_test(test_yajl "YAJL sanity")
add_qt_test(test_dom "DOM API")
add_qt_test(test_parse "Parser logic")
add_qt_test(test_sax "SAX API")
add_qt_test(test_schema2 "Schema sanity checks")
add_regular_test(smoketest_mem_badinput "C memory leak smoke test for parsing invalid input")

if (WITH_PERFORMANCE_TESTS)
    if (WITH_VALGRIND)
        set(_OLD_VALGRIND_TOOL ${VALGRIND_TOOL})
        set(VALGRIND_TOOL "callgrind")
        add_qt_test(test_perf "Performance test" -callgrind)
        set(VALGRIND_TOOL ${_OLD_VALGRIND_TOOL})
    else ()
        add_qt_test(test_perf "Performance test")
    endif ()
endif ()

add_schema_test(test_schema "Number sanity check" ${CMAKE_CURRENT_SOURCE_DIR}/../schemas/sanity/ SimpleNumber)
add_schema_test(test_schema "Contact" ${CMAKE_CURRENT_SOURCE_DIR}/../schemas/contact/ Contact)

#add_qt_test(test_schema "pjson-Schema Test Return Value" returnValue.schema returnValue1.good returnValue2.good returnValue3.good returnValue4.good)
#add_qt_test(test_schema "pjson-Schema Test Invalid Return Value" returnValue.schema returnValue1.bad returnValue2.bad returnValue3.bad returnValue4.bad)
#add_qt_test(test_num_conversion "Numeric conversion Unit Tests")
