#!/bin/bash
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"/../

CC=clang
CXX=clang++

CC=${CC} \
CXX=${CXX} \
cmake \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=/srv/data_local/jgwohlbier/DSSoC/DASH/traceatlas/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_INSTALL_PREFIX=/srv/data_local/jgwohlbier/DSSoC/DASH/install \
  $dir

# module load llvm-9.0.1-clang-11.0.0-5u5pz7m
# module load papi-6.0.0.1-gcc-10.2.0-m2k5fwr
# module load nlohmann-json-3.9.1-gcc-10.2.0-widbhew
# module load zlib-1.2.11-clang-9.0.1-pomrxgh
# module load spdlog-1.8.1-gcc-10.2.0-53bou2j
# module load cmake-3.20.1-gcc-10.2.0-led4xjq
# module load ninja-1.10.2-gcc-10.2.0-erzvwdq
