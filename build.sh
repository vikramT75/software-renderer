#!/bin/bash

set -e  # stop on error

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/swr