#!/bin/bash
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"/../

# -DTIME
# -DINT_TIME

#CXXFLAGS=-flto
TOOLCHAIN_FILE=/home/jgwohlbier/DSSoC/DASH/TraceAtlas/vcpkg/scripts/buildsystems/vcpkg.cmake
#LDFLAGS=-fuse-ld=lld

CC=`which clang` \
CXX=`which clang++` \
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} \
  $dir
