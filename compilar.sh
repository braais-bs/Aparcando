#!/usr/bin/env bash

# Linux
#gcc parking.c libparking.a -o parking -lm -m32

# Windows
#i686-w64-mingw32-g++ parking2.cpp -o parking2.exe -L. -lparking2 -DPARKING2_EXPORTS
i686-w64-mingw32-g++ parking2.cpp -o parking2.exe

#ejecutar con wine
# WINEPATH="/usr/i686-w64-mingw32/bin" wine parking2.exe
