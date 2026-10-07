#!/bin/bash

# Change to the directory where this script is located
cd "$(dirname "$0")"

if [[ -n $1 ]]; then
tests/grade.py $1 --run-sh ./run.sh --test-dir tests/phase1
echo $1
else
tests/grade.py phase1 --run-sh ./run.sh --test-dir tests/phase1
echo "1st param is $1"
fi
