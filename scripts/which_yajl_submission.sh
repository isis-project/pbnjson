#!/bin/bash

SCRIPT_DIR=$(dirname $0)
SRC_DIR="${SCRIPT_DIR}/../"
YAJL_DIR="${SRC_DIR}/src/pjson_engine/yajl"

pushd "$YAJL_DIR"

SUBMISSION=$(git tag | sort --numeric-sort --field-separator=- --key 3 | tail -n 1)
echo "Latest yajl submission is: ${SUBMISSION}" >&2
echo "$SUBMISSION"
