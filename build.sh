#!/bin/bash
set -e

clear
# mkdir -p build
cd build
# cmake ..
cmake --build . -t syncserver -j 5

# echo; echo; echo
# echo "----------------------------------------RUNNING----------------------------------------"
# ./syncserver