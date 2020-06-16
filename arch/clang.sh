#!/bin/bash
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"/../

TOOLCHAIN_FILE=/home/jgwohlbier/DSSoC/DASH/TraceAtlas/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} \
  $dir
