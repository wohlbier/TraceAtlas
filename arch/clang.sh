#!/bin/bash
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"/../

# -DTIME
# -DINT_TIME

CC=`which clang`
CXX=`which clang++`
#CXXFLAGS=-flto
TOOLCHAIN_FILE=/home/jgwohlbier/DSSoC/DASH/TraceAtlas/vcpkg/scripts/buildsystems/vcpkg.cmake
#LDFLAGS=-fuse-ld=lld

cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} \
  $dir


#  -DCMAKE_C_COMPILER=${CC} \
#  -DCMAKE_CXX_COMPILER=${CXX} \
#  -DCMAKE_CXX_FLAGS=${CXXFLAGS} \
#  -DCMAKE_AR=`which llvm-ar` \
#  -DCMAKE_RANLIB=`which llvm-ranlib` \
#  -DCMAKE_EXE_LINKER_FLAGS=${LDFLAGS} \
#  -DCMAKE_CXX_LINK_EXECUTABLE="<CMAKE_CXX_COMPILER> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>" \
