#!/bin/bash

SCRIPT_DIR="$(dirname $0)"

IDE=eclipse BUILD_TYPE=release "$SCRIPT_DIR/setup_native.sh" "$@"
