if (NOT QT_FOUND)
    message(STATUS "Qt not found - skipping building many tests for C++ api")
endif ()

src(smoketest_mem
    SmokeTestMemLeak.cpp
)

include_directories(
    ${API_HEADERS}
)

set(TEST_LIBRARIES pbnjson_cpp)
######################### THE UNIT TESTS ############################
# TestDOM test
qt_hdrs(test_cppdom
    TestDOM.h
)

src(test_cppdom
    TestDOM.cpp
)

qt_hdrs(test_cppsax
    TestSAX.h
)

src(test_cppsax
    TestSAX.cpp
)

src(test_yajl
    TestYAJL.cpp
)

qt_hdrs(test_cppschema
	TestSchema.h
)

src(test_cppschema
	TestSchema.cpp
)

qt_hdrs(test_regressions
	TestRegression.h
)

src(test_regressions
	TestRegression.cpp
)

set(test_regressions_test_list
	testNOV99444
)

include_directories(${C_ENGINE_INCDIR})

set(test_cppdom_test_list
	testParserObjectSimple
	testParserObjectComplex
	testParserArray
	testParserComplex
	testObjectSimple
	testObjectComplicated
	testObjectIterator
	testObjectPut
	testArraySimple
	testArrayComplicated
	testStringSimple
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

add_qt_test(test_cppdom "C++ DOM binding")
add_regular_test(smoketest_mem "C++ Smoke Test of Memory Usage")
#add_qt_test(test_cppsax "pjson C++ binding SAX test")
#add_qt_test(test_num_conversion "Numeric conversion Unit Tests")
add_schema_test(test_cppschema "C++ Number sanity check" ${CMAKE_CURRENT_SOURCE_DIR}/../schemas/sanity/ SimpleNumber)
add_schema_test(test_cppschema "C++ Contact" ${CMAKE_CURRENT_SOURCE_DIR}/../schemas/contact/ Contact)
add_qt_test(test_regressions "Regression testing")
