set -e

cd /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/borealis-lib/lib/extern/tweeny
/opt/devkitpro/msys2/usr/bin/cmake.exe --regenerate-during-build -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR)
