#!/bin/bash

pushd $(dirname $0)
cd ../
PROJ_DIR=$PWD
popd

BUILD_DIR=${PROJ_DIR}/build
SCRIPT_DIR=${PROJ_DIR}/scripts-bisect
if [ ! -d $SCRIPT_DIR ]; then
	: ${SCRIPT_BRANCH:=360a5b344761abed850c65b8eab41f58d3591315}
	echo "Getting scripts from ${SCRIPT_BRANCH}"
	pushd ${PROJ_DIR} || exit 130
	mv scripts{,.tmp} || exit 131
	git checkout ${SCRIPT_BRANCH} -- scripts || exit 132
	mv scripts{,-bisect} || exit 133
	mv scripts{.tmp,} || exit 134
	popd
fi

ERR=2
function run_tests() {
	local build_dir=$1
	local test_dir=$2
	shift 2
	local test_name=
	for test_name in "$@"; do
		printf "Testing $test_name... "
		printf "\n\n!!!!!!!!!!!!! NEW RUN !!!!!!!!!!!!!!!!!!\n" >> ${build_dir}/${test_name}.log
		if ! ${build_dir}/test/$test_dir/$test_name >> ${build_dir}/${test_name}.log 2>&1; then
			echo "Problem" >&2
			return $ERR
		else
			echo $OK
		fi
		ERR=$((ERR+1))
		if [ $ERR -eq 125 ]; then
			ERR=$((ERR + 1))
		elif [ $ERR -gt 127 ]; then
			ERR=2
		fi
	done

	return 0
}

function run_all_tests() {
	local CONF_SCRIPT=$1
	local BUILD=$2

	echo "Configuring...."
	${CONF_SCRIPT} || return 125
	pushd ${BUILD} || exit 140

	echo "Building..."
	make -j3 || exit 125

	echo "==============Testing C API==============="
	run_tests ${BUILD} c_api test_dom test_sax test_perf test_yajl || return $?

	echo "==============Testing CXX API==============="
	run_tests ${BUILD} cxx_api test_cppdom || return $?

	return 0
}

function test_build() {
	echo "+++++++++++++++++++ TESTING DEBUG ++++++++++++++++++++++"
	run_all_tests ${SCRIPT_DIR}/setup_make_debug.sh ${BUILD_DIR}/native-debug || return $?

	echo "+++++++++++++++++++ TESTING RELEASE ++++++++++++++++++++++"
	run_all_tests ${SCRIPT_DIR}/setup_make_release.sh ${BUILD_DIR}/native-release || return $?
}

test_build
