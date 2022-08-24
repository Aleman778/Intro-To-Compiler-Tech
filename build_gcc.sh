#!/bin/bash

if ! [ -d build ]
then
    mkdir build
fi
pushd build

gcc ../code/main.cpp -DBUILD_X64 -DBUILD_POSIX

popd
