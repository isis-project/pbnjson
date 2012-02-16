find_package(Valgrind)

# Variable compatibility with existing build scripts
# Deprecated in favour of setting WITH_VALGRIND to 'memcheck'
if (DEFINED MEMCHK)
    set(WITH_VALGRIND "memcheck" CACHE STRING "When running unit tests, the tool to use when running valgrind (memcheck, callgrind, etc)")
else ()
    set(WITH_VALGRIND "" CACHE STRING "When running unit tests, the tool to use when running valgrind (memcheck, callgrind, etc)")
endif ()

set(VALGRIND_TOOL "${WITH_VALGRIND}")

set(WITH_VALGRIND_memcheck_OPTIONS --leak-check=full --error-exitcode=127 CACHE STRING "The options to pass when running memcheck")
set(WITH_VALGRIND_callgrind_OPTIONS "" CACHE STRING "The options to pass when running callgrind")

if (VALGRIND_FOUND AND VALGRIND_TOOL)
	# G_SLICE=always-malloc is needed for testing w/ valgrind
	# properly (otherwise tests may provide false positives due to
	# Qt using Glib & the slice allocator never freeing memory)
	# Setting test environment variables from CMake are only supported
	# as of 2.8
	if (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION LESS 8)
		macro(conf_valgrind_env TEST_NAME)
			set(SUPRESS_GSLICE_WARNING $ENV{SUPRESS_GSLICE_WARNING})
			if (NOT SUPRESS_GSLICE_WARNING)
				message(WARNING "CMake version is too old - you need to manually set G_SLICE=always-malloc when running the tests under valgrind (set SUPRESS_GSLICE_WARNING env variable to suppress this warning)")
			endif ()
		endmacro(conf_valgrind_env)
	else ()
		macro(conf_valgrind_env TEST_NAME)
			set_property(TEST '${${TEST_NAME}}' APPEND PROPERTY ENVIRONMENT
				"G_SLICE=always-malloc"
			)
		endmacro(conf_valgrind_env)
	endif ()
endif ()

# Locate Qt for testing
# Qt4 preamble
find_package(Qt4 4.5 COMPONENTS QtCore QtTestLib)

# You'd think that if I asked a minimum Qt version, it wouldn't
# set QT_FOUND if that minimum version wasn't found.
# This is probably a bug in CMake or the FindQt4.cmake script.
# In the future, they might define this variable, so let's make this
# forward compatible
if (NOT QT_VERSION)
	set(QT_VERSION "${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")
	message (WARNING "QT_VERSION not found - manually constructed as ${QT_VERSION}")
endif()

if ("4.5.0" VERSION_GREATER "${QT_VERSION}")
	message(STATUS "Found Qt version ${QT_VERSION} is too old for unit tests - pbnjson tests will not be built")
	set(QT_FOUND FALSE)
endif()

if (QT_FOUND)

	set(QT_USE_QTTEST true)
	set(QT_DONT_USE_QTCORE false)
	set(QT_DONT_USE_QTGUI true)
	set(QT_USE_QTXML true)

	include(${QT_USE_FILE})
	if (NOT QT_LIBRARIES)
	    if (APPLE)
		list(APPEND QT_LIBRARIES ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY})
		FIND_PATH(QT_INCLUDE_DIR QByteArray)
		message(STATUS "Seems there's a problem with the FindQt4.cmake in CMake for Apple - QT_LIBRARIES not set.  Manually setting it to ${QT_LIBRARIES}")
	    else (APPLE)
		message(FATAL_ERROR "Qt libraries found, but QT_LIBRARIES variable not set by FindQt4.cmake - tests will not compile")
	    endif (APPLE)
	elseif (NOT QT_QTTEST_FOUND)
		message(FATAL_ERROR "Qt4 found, but QtTest component wasn't - tests will not compile")
	elseif (NOT_QT_QTCORE_FOUND)
		message(FATAL_ERROR "Qt4 found, but QtCore component wasn't - tests will not compile")
	endif (NOT QT_LIBRARIES)

	######################### HELPER FUNCTIONS ############################    
	# EXE_NAME - the name of the executable
	# ... - A list of headers to MOC
	# defines the variable ${EXE_NAME}_qthdrs to the given list
	macro(qt_hdrs EXE_NAME)
	    set(${EXE_NAME}_qthdrs ${ARGN})
	endmacro(qt_hdrs EXE_NAME )
else ()
	macro(qt_hdrs EXE_NAME)
	endmacro(qt_hdrs EXE_NAME)
endif ()

# EXE_NAME - the name of the executable
# ... - A list of source files to include in the executable
# defines the variable ${EXE_NAME}_src to the given list
macro(src EXE_NAME)
    set(${EXE_NAME}_src ${ARGN})
endmacro(src EXE_NAME)

# Variable compatibility with existing build scripts
# Deprecated in favour of setting WITH_PERFORMANCE_TESTS
if (DEFINED ADD_PERFORMANCE_TEST)
    set(WITH_PERFORMANCE_TESTS ${ADD_PERFORMANCE_TEST} CACHE BOOL "Include performance unit tests")
else ()
    set(WITH_PERFORMANCE_TESTS FALSE CACHE BOOL "Include performance unit tests")
endif ()

macro (conf_valgrind_prefix)
    if (NOT VALGRIND_FOUND AND VALGRIND_TOOL)
        message(FATAL_ERROR "Requesting running tests under valgrind, but valgrind not found")
    elseif (VALGRIND_TOOL)
        if (NOT VALGRIND_TOOL STREQUAL "callgrind" AND NOT VALGRIND_TOOL STREQUAL "memcheck" AND NOT VALGRIND_TOOL STREQUAL "cachegrind" AND NOT VALGRIND_TOOL STREQUAL "massif" AND NOT VALGRIND_TOOL STREQUAL "helgrind")
            message(WARNING "Valgrind tool ${VALGRIND_TOOL} not recognized")
        elseif (NOT DEFINED WITH_VALGRIND_${VALGRIND_TOOL}_OPTIONS)
            message(WARNING "Valgrind tool ${VALGRIND_TOOL} doesn't have any options specified")
        endif()

        set(VALGRIND_OPTIONS "${WITH_VALGRIND_${VALGRIND_TOOL}_OPTIONS}")
        set(cmd_prefix valgrind --tool=${VALGRIND_TOOL} ${VALGRIND_OPTIONS})
    else ()
        set(cmd_prefix )
    endif ()
endmacro (conf_valgrind_prefix)

if (QT_FOUND)
	# EXE_NAME - the name of the executable
	#            make sure you use qt_hdrs & src macros so that variable naming is consistent
	# TEST_NAME - the friendly name of the test
	# ... - An optional list of command line arguments to pass to the executable
	function(add_qt_test EXE_NAME TEST_NAME)
		conf_valgrind_prefix()

		if (NOT TARGET ${EXE_NAME})
			# Specify MOC
			qt4_wrap_cpp(${EXE_NAME}_mocsrc ${${EXE_NAME}_qthdrs})
			
			# Create executable
			add_executable(${EXE_NAME} ${${EXE_NAME}_src} ${${EXE_NAME}_mocsrc})
			
			# Libraries to link against
			target_link_libraries(${EXE_NAME}
				${QT_LIBRARIES}
				${TEST_LIBRARIES}
			)
		endif (NOT TARGET ${EXE_NAME})

		if (NOT DEFINED ${EXE_NAME}_test_list)
			if (VALGRIND_TOOL)
				set(TEST_NAME "Valgrind ${VALGRIND_TOOL}: ${TEST_NAME}")
			endif()
			string(REPLACE " " "\\ " TEST_NAME ${TEST_NAME}) 
			add_test('${TEST_NAME}' ${cmd_prefix} ${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME} ${ARGN})
			conf_valgrind_env(TEST_NAME)
		else (NOT DEFINED ${EXE_NAME}_test_list)
			foreach (test_name ${${EXE_NAME}_test_list})
				set(tmp_test_name "${TEST_NAME} : ${test_name}")
				if (VALGRIND_TOOL)
					set(tmp_test_name "Valgrind ${VALGRIND_TOOL}: ${tmp_test_name}")
				endif()
				string(REPLACE " " "\\ " tmp_test_name ${tmp_test_name}) 
				add_test('${tmp_test_name}' ${cmd_prefix} ${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME} ${test_name})
				conf_valgrind_env(tmp_test_name)
			endforeach (test_name)
		endif (NOT DEFINED ${EXE_NAME}_test_list)
	endfunction(add_qt_test EXE_NAME TEST_NAME)

	function(add_schema_test_ex EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME IS_VALID)
		if (IS_VALID)
			set(RELATIVE_PATH "${SCHEMA_PATH}/input_valid")
			# the empty prefix is so that test names line up nicely on output
			set(TEST_NAME "        ${TEST_NAME}")
			set(PASS_VALUE 1)
		else (IS_VALID)
			set(RELATIVE_PATH "${SCHEMA_PATH}/input_invalid")
			SET(TEST_NAME "Invalid ${TEST_NAME}")
			set(PASS_VALUE 0)
		endif (IS_VALID)
		file(GLOB test_files RELATIVE "${RELATIVE_PATH}" "${RELATIVE_PATH}/${SCHEMA_NAME}.*.json")
		if (test_files)
			foreach (test_file ${test_files})		
				set(${EXE_NAME}_input ${test_file}_${PASS_VALUE})
				add_qt_test("${EXE_NAME}" "Schema API : ${TEST_NAME} w/ input file ${test_file}" -schema "${SCHEMA_PATH}/${SCHEMA_NAME}.schema" -input "${RELATIVE_PATH}/${test_file}"  -pass ${PASS_VALUE})
			endforeach(test_file)		
		else ()
			message(SEND_ERROR "${SCHEMA_NAME} has no test cases (IS_VALID = ${IS_VALID}): RELATIVE_PATH = '${RELATIVE_PATH}', test_files = '${test_files}'")
		endif ()
	endfunction(add_schema_test_ex EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME IS_VALID)

	function(add_schema_test EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME)
		add_schema_test_ex("${EXE_NAME}" "${TEST_NAME}" "${SCHEMA_PATH}" "${SCHEMA_NAME}" true)
		add_schema_test_ex("${EXE_NAME}" "${TEST_NAME}" "${SCHEMA_PATH}" "${SCHEMA_NAME}" false)
	endfunction(add_schema_test EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME)

else()
	macro(add_qt_test)
	endmacro(add_qt_test)

	macro(add_schema_test_ex EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME IS_VALID)
	endmacro(add_schema_test_ex EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME IS_VALID)

	macro(add_schema_test EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME)
	endmacro(add_schema_test EXE_NAME TEST_NAME SCHEMA_PATH SCHEMA_NAME)

endif ()

# EXE_NAME - the name of the executable
#            make sure you use qt_hdrs & src macros so that variable naming is consistent
# TEST_NAME - the friendly name of the test
# ... - An optional list of command line arguments to pass to the executable
function(add_regular_test EXE_NAME TEST_NAME)
    conf_valgrind_prefix()

    if (NOT TARGET ${EXE_NAME})
	# Create executable
	add_executable(${EXE_NAME} ${${EXE_NAME}_src})
	
	# Libraries to link against
	target_link_libraries(${EXE_NAME}
		${TEST_LIBRARIES}
	)
    endif (NOT TARGET ${EXE_NAME})

    if (NOT DEFINED ${EXE_NAME}_test_list)
	if (VALGRIND_TOOL)
		set(TEST_NAME "Valgrind: ${TEST_NAME}")
	endif()
	string(REPLACE " " "\\ " TEST_NAME ${TEST_NAME}) 
	add_test('${TEST_NAME}' ${cmd_prefix} ${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME} ${ARGN})
	conf_valgrind_env(TEST_NAME)
    else (NOT DEFINED ${EXE_NAME}_test_list)
	foreach (test_name ${${EXE_NAME}_test_list})
		set(tmp_test_name "${TEST_NAME} : ${test_name}")
		if (VALGRIND_TOOL)
			set(tmp_test_name "Valgrind: ${tmp_test_name}")
		endif()
		string(REPLACE " " "\\ " tmp_test_name ${tmp_test_name}) 
		add_test('${tmp_test_name}' ${cmd_prefix} ${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME} ${test_name})
		#set_property(TEST '${tmp_test_name}' APPEND PROPERTY ENVIRONMENT G_SLICE=always-malloc)
		conf_valgrind_env(tmp_test_name)
	endforeach (test_name)
    endif (NOT DEFINED ${EXE_NAME}_test_list)
endfunction(add_regular_test EXE_NAME TEST_NAME)

