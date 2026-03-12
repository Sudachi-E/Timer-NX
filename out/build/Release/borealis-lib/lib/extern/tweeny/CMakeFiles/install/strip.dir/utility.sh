set -e

cd /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/borealis-lib/lib/extern/tweeny
/opt/devkitpro/msys2/usr/bin/cmake.exe -DCMAKE_INSTALL_DO_STRIP=1 -P cmake_install.cmake
