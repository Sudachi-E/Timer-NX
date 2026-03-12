set -e

cd /opt/devkitpro/BrewProjects/Switch-Clock/build-ninja/borealis-lib/lib/extern/fmt
/opt/devkitpro/msys2/usr/bin/cmake.exe -DCMAKE_INSTALL_LOCAL_ONLY=1 -P cmake_install.cmake
