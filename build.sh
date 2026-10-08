#!/bin/bash
rm -rf ./build
rm -rf dist
cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" -S . -B build
cmake --build build --target RunImage
mkdir dist
cp -r ./res/* ./dist/
rm -f ./dist/res.txt
cp  ./build/!RunImage,ff8 ./dist/

