set -e

cd /opt/devkitpro/BrewProjects/Switch-Clock/build-ninja/borealis-lib/lib/extern/fmt
/opt/devkitpro/msys2/usr/bin/ccmake.exe -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
