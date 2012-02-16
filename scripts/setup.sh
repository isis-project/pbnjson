#!/bin/bash

if [ -n "$IDE" ]; then
	case "$IDE" in
		eclipse) IDE="Eclipse CDT4 - Unix Makefiles";;
		unix) IDE="Unix Makefiles";;
		make) IDE="Unix Makefiles";;
		makefile) IDE="Unix Makefiles";;
		makefiles) IDE="Unix Makefiles";;
		xcode) IDE="Xcode";;
		kdevelop3) IDE="KDevelop3";;
		kdevelop) IDE="KDevelop3";; # latest kdevelop
		*) echo "Targetting CMake generator '$IDE'";;
	esac
else
	echo "No IDE specified.
eclipse, unix, make, xcode, or kdevelop allowed, or the actual CMake Generator string" >&2
	exit 1
fi

: ${BUILD_TYPE:=release}
: ${SCRIPT_NAME:=setup_native.sh}

if ! which dirname > /dev/null 2>&1; then
	echo "'dirname' is not on the path" >&2
	exit 1
fi

SCRIPT_DIR=$(dirname $0)
if [ ! -f "$SCRIPT_DIR/${SCRIPT_NAME}" ]; then
	echo "Found script directory '$BUILD_DIR' which doesn't seem valid" >&2
	exit 1
fi
pushd "$SCRIPT_DIR"
SCRIPT_DIR="$PWD"
popd

if [ -z "$PROJ_DIR" ]; then
	pushd "$SCRIPT_DIR/../"
	PROJ_DIR="$PWD"
	popd
fi

: ${SRC_DIR:="$PROJ_DIR/src"}

if [ ! -f "${SRC_DIR}/CMakeLists.txt" ]; then
	echo "Found source directory '${SRC_DIR}' which doesn't seem valid.  Try setting PROJ_DIR (default SCRIPT_DIR/../) or SRC_DIR (default PROJ_DIR/src) manually" >&2
	exit 1
fi

unset TOOLCHAIN
unset HOME_VARS
TOOLCHAIN_FILE="${SRC_DIR}/CMake/DeviceSupport/Toolchain-${DEVICE}.cmake"
if [ -n "$DEVICE" -a -f "${TOOLCHAIN_FILE}" ]; then
	if [ ! -d "$HOME_CROSS" ]; then
		echo "Error: Must set HOME_CROSS to the bin directory containing the compiler (instead got $HOME_CROSS)" >&2
		exit 1
	fi
	if [ ! -d "$HOME_BUILD" ]; then
		if [ -n "$HOME_BUILD" ]; then
			echo "HOME_BUILD $HOME_BUILD is not valid" >&2
			exit 1
		fi
		pushd "$HOME_CROSS/../../"
		HOME_BUILD="$PWD"
		popd
		echo "Warn: HOME_BUILD not set - picking up relative to HOME_CROSS: $HOME_BUILD" >&2
	fi
	TOOLCHAIN='-DCMAKE_TOOLCHAIN_FILE:FILEPATH="$TOOLCHAIN_FILE"'
	HOME_VARS='-DHOME_CROSS:FILEPATH="$HOME_CROSS" -DHOME_BUILD:FILEPATH="$HOME_BUILD"'
	echo "Using cross compiler:
	TOOLCHAIN=$TOOLCHAIN
	HOME_CROSS=$HOME_CROSS
	HOME_BUILD=$HOME_BUILD"
elif [ -n "$DEVICE" ]; then
	echo "Error: Device set to $DEVICE but no toolchain support" >&2
	exit 1
else
	DEVICE=native
fi

: ${BUILD_DIR:="${PROJ_DIR}/build"}
if ! mkdir -p "$BUILD_DIR"; then
	echo "Unable to create $BUILD_DIR" >&2
	exit 1
fi

TARGET_DIR="${BUILD_DIR}/${BUILD_PREFIX:-$DEVICE}-$BUILD_TYPE"
if [ -d "${TARGET_DIR}" ]; then
	echo "WARNING: '$TARGET_DIR' already exists" >&2
elif ! mkdir "$TARGET_DIR"; then
	echo "Unable to create '$TARGET_DIR'" >&2
	exit 1
fi

pushd "$PROJ_DIR"
git submodule init
git submodule update 
pushd "${SRC_DIR}/pjson_engine/yajl"
if ! git reset --hard HEAD; then
	echo "Failed to undo changes" >&2
	exit 1
fi
popd
if [ -d ${SCRIPT_DIR}/patches ]; then
	for patch in ${SCRIPT_DIR}/patches/*; do
		printf "Applying %s" "$patch"
		if ! patch --strip=1 --forward --input="$patch" --directory="${SRC_DIR}/pjson_engine/yajl" --unified --silent; then
			echo " !!!! ERROR" >&2
			exit 2
		fi
		echo "..."
	done
fi
popd

if [ -n "$CJSON_INSTALL_DIR" ]; then
	CJSON_INSTALL_DIR="'-DCJSON_INSTALL_DIR=$CJSON_INSTALL_DIR'"
fi

pushd "$TARGET_DIR"
eval echo "Invoking CMake:" cmake -G "'$IDE'" -DCMAKE_BUILD_TYPE='"${BUILD_TYPE}"' \$CMAKE_ADDITIONAL_FLAGS $TOOLCHAIN $HOME_VARS "'$SRC_DIR'" "$@" $CJSON_INSTALL_DIR
eval cmake -G "'$IDE'" -DCMAKE_BUILD_TYPE='"${BUILD_TYPE}"' \$CMAKE_ADDITIONAL_FLAGS $TOOLCHAIN $HOME_VARS "'$SRC_DIR'" "$@" $CJSON_INSTALL_DIR
popd
