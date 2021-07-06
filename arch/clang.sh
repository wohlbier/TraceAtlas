#!/bin/bash
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"/../

# see README.md for modules to load.

CC=clang
CXX=clang++

COMP_ROOT=/srv/data_local/packages/spack/opt/spack/linux-rhel8-cascadelake/clang-9.0.1

NCURSES_ROOT=${COMP_ROOT}/ncurses-6.2-d2osnysxv2oos5e26x3oyydwgp47vczc
NLOHMANN_ROOT=${COMP_ROOT}/nlohmann-json-3.9.1-bnnf4qicdgr3v6xkvmqsgwjlrn7nc73f
PAPI_ROOT=/srv/data_local/packages/spack/opt/spack/linux-rhel8-cascadelake/gcc-11.1.0/papi-6.0.0.1-7smyvpqnadnfc67t2escqfr6fhkxmo63
ZLIB_ROOT=${COMP_ROOT}/zlib-1.2.11-pomrxghpiffhevjqouh76onyq4etboj2

CC=${CC} \
CXX=${CXX} \
cmake \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS=-I${NLOHMANN_ROOT}/include \
  -DCMAKE_C_FLAGS=-I${PAPI_ROOT}/include \
  -DCMAKE_INSTALL_PREFIX=/srv/data_local/jgwohlbier/DSSoC/DASH/install \
  -DCMAKE_EXE_LINKER_FLAGS="-L${NCURSES_ROOT}/lib -L${ZLIB_ROOT}/lib" \
  -DCMAKE_TOOLCHAIN_FILE=/srv/data_local/jgwohlbier/DSSoC/DASH/traceatlas/vcpkg/scripts/buildsystems/vcpkg.cmake \
  $dir
