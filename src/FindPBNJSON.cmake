# Try to find pbnjson library
# This will define
# PBNJSON_FOUND - system has pbnjson
# PBNJSON_INCLUDE_DIRS - include directories necessary to compile w/ pbnjson
# PBNJSON_LIBRARIES - libraries necessary to link to to get pbnjson
#
# PBNJSON has the following components (e.g. find_package(PBNJSON REQUIRED v8):
#     c - The underlying C library that implements the abstraction
#     cpp - The C++ abstraction library
#     v8 - The v8 bindings
include (LibFindMacros)

function(_PBNJSON_CONF _target_var _var_name _msg)
	if (${_var_name})
		list(APPEND ${_target_var} ${${_var_name}})
	elseif (PBNJSON_FIND_REQUIRED)
		message(FATAL_ERROR ${_msg})
	endif ()
endfunction()

# PBNJSON library dependancies - handles FATAL_ERROR logic for us
function(_PBNJSON_LIBRARY_ _name)
	set(_libname pbnjson_${_name})
	find_library(_PBNJSON_LIBRARY_${_name} ${_libname})
	_PBNJSON_CONF(PBNJSON_PROCESS_LIBS _PBNJSON_LIBRARY_${_name} "Could not find ${_name} component : Missing library ${_libname}")
endfunction()

function(_PBNJSON_INCLUDE_DIR_ _name _header)
	find_path(_PBNJSON_HEADER_${_name} NAMES ${_header})
	_PBNJSON_CONF(PBNJSON_PROCESS_INCLUDES _PBNJSON_HEADER_${_name} "Could not find ${_name} component : Missing header ${_header}")
endfunction()

# default to C library
if (NOT PBNJSON_FIND_COMPONENTS)
	set(PBNJSON_FIND_COMPONENTS c)
endif()

foreach(__PBNJSON_COMPONENT ${PBNJSON_FIND_COMPONENTS})
	if(__PBNJSON_COMPONENT STREQUAL "v8")
		libfind_package(PBNJSON V8)
		set(_component_hdr "pbnjson_v8.hpp")
	elseif(__PBNJSON_COMPONENT STREQUAL "c")
		set(_component_hdr "pbnjson.h")
	elseif(__PBNJSON_COMPONENT STREQUAL "cpp")
		set(_component_hdr "pbnjson.hpp")
	endif ()

	_PBNJSON_LIBRARY_(${__PBNJSON_COMPONENT})
	_PBNJSON_INCLUDE_DIR_(${__PBNJSON_COMPONENT} ${_component_hdr})
endforeach()

# LibFindMacros takes care of the rest
libfind_process(PBNJSON)
