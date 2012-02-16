#!/bin/bash

SCRIPT_DIR=$(dirname $0)
PROJ_DIR="${SCRIPT_DIR}/.."
SRC_DIR="${PROJ_DIR}/src"
YAJL_DIR="${SRC_DIR}/pjson_engine/yajl"

if [ ! -d ${YAJL_DIR} ]; then
	echo "Use scripts/setup_* first" >&2
	exit 1
fi

pushd "$YAJL_DIR"

# Get the latest yajl submission
LATEST_YAJL_SUBMISSION=$(${SCRIPT_DIR}/which-yajl-submission.sh 2>/dev/null)
YAJL_CURRENT_COMMIT=$(git log | head -n 1)
YAJL_LATEST_COMMIT=$(git log ${LATEST_YAJL_SUBMISSION} | head -n 1)

if [ "$YAJL_LATEST_COMMIT" != "$YAJL_CURRENT_COMMIT" ]; then
	echo "Mismatch between latest submission $LATEST_YAJL_SUBMISSION (commit $YAJL_LATEST_COMMIT) & the current commit ($YAJL_CURRENT_COMMIT)"
	git checkout "$LATEST_YAJL_SUBMISSION"
	cd $YAJL_DIR/../
	echo "Run 'git add yajl && git commit' to update your yajl submission"
else
	echo "YAJL is up to date"
fi

popd
	
