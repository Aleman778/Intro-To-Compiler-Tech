@echo off

call vcvarsall.bat x64

IF NOT EXIST build mkdir build
pushd build

cl ../code/main.cpp -nologo -DBUILD_X64 -DBUILD_WINDOWS -Od -Zi -Zo -FC -link -incremental:no -opt:ref  -out:compiler.exe

popd
