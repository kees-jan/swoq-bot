#!/bin/bash -eux

export PATH=~/opt/grpc/bin:$PATH

[ -d build ] || cmake -S . -B build -G Ninja  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14
cmake --build build

while build/src/bot
do
    true
done
