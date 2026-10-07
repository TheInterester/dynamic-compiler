#!/usr/bin/env bash

set -e -o pipefail

# Change to the directory where this script is located
cd "$(dirname "$0")"

if [ $# -eq 0 ]; then
    ./build/cmake/release/mitscript -h
    exit 1
fi

./build/cmake/release/mitscript "$@"
