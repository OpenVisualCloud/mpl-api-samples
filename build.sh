#!/bin/bash -e

source /opt/intel/oneapi/setvars.sh --force


cmake -S . -B build
#cmake -S . -B build -DVTUNEINFO=ON
cmake --build build -j`nproc`
#cpack --config build/CPackConfig.cmake -B build/
