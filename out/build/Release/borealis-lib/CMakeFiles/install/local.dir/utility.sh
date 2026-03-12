set -e

cd /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/borealis-lib
/opt/devkitpro/msys2/usr/bin/cmake.exe -DCMAKE_INSTALL_LOCAL_ONLY=1 -P cmake_install.cmake
