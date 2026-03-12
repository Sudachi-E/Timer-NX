set -e

cd /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/borealis-lib
/opt/devkitpro/msys2/usr/bin/ccmake.exe -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
