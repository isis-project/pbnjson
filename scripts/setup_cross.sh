#!/bin/bash

# Help:
#   $0 [DEVICE] [HOME_CROSS] [HOME_BUILD]
#     DEVICE - mandatory on command line or in environment
#     HOME_CROSS - mandatory on command line or in environment
#     HOME_BUILD - optional.  found relative to HOME_CROSS if not specified

function print_help()
{
	echo "$0 <DEVICE> <HOME_CROSS> [HOME_BUILD]
	DEVICE - The target (e.g. castle).  Refer to src/CMake/DeviceSupport for valid targets
	HOME_CROSS - The bin directory of the cross compiler in your OE build
	HOME_BUILD - Optional directory of the OE build directory in case it's separate from the compiler

	Any of the parameters can be omitted if they appear in the environment (environment always takes precedance)."
}

function die()
{
	local CODE=1
	if [ $# -gt 0 ]; then
		if [ $1 -eq $1 > /dev/null 2>&1 ]; then
			CODE=$1
			shift
		fi
	fi
	echo "$*" >&2
	print_help >&2
	exit $CODE
}

if [ -z "$DEVICE" ]; then
	if [ $# -lt 1 ]; then
		die 1 "DEVICE not specified in environment or command line"
	fi
	DEVICE=$1
	shift
fi

if [ -z "$HOME_CROSS" ]; then
	if [ $# -lt 1 ]; then
		die 2 "HOME_CROSS not specified in environment or command line"
	fi
	HOME_CROSS=$1
	if [ ! -d "$HOME_CROSS" ]; then
		die 3 "HOME_CROSS must be specified as the parameter after the DEVICE (or in the environment)"
	fi
	shift
fi

if [ -z "$HOME_BUILD" ]; then
	if [ $# -gt 1 ]; then
		HOME_BUILD=$1
		shift
	fi
fi

SCRIPT_BASE=$(dirname $0)
SCRIPT_NAME=$(basename $0 | sed 's~setup_cross\([^.]*\)\.sh~\1~')
case $SCRIPT_NAME in
	_debug) BUILD_TYPE=debug;;
	_release) BUILD_TYPE=release;;
	*)
		die 4 "Unsupported build type $SCRIPT_NAME"
		exit 1;;
esac

export DEVICE HOME_BUILD HOME_CROSS BUILD_TYPE

IDE=${IDE:-make} ${SCRIPT_BASE}/setup.sh
