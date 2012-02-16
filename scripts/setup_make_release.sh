#!/bin/bash

SCRIPT_DIR="$(dirname $0)"

IDE=make BUILD_TYPE=release "$SCRIPT_DIR/setup_native.sh" "$@"
