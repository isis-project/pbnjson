#!/bin/bash

SCRIPT_DIR="$(dirname $0)"

IDE=eclipse BUILD_TYPE=debug "$SCRIPT_DIR/setup_native.sh" "$@"
