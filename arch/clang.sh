#!/bin/bash
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"/../

# -DTIME
# -DINT_TIME

#CXXFLAGS=
TOOLCHAIN_FILE=/home/jgwohlbier/DSSoC/DASH/TraceAtlas/vcpkg/scripts/buildsystems/vcpkg.cmake

CC=clang \
CXX=clang++ \
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} \
  -DCMAKE_CXX_FLAGS=${CXXFLAGS} \
  $dir
