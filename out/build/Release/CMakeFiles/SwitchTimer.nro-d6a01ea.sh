set -e

cd /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release
/opt/devkitpro/tools/bin/nacptool.exe --create SwitchTimer SwitchTimer 1.0.0 SwitchTimer.nacp
/opt/devkitpro/msys2/usr/bin/cmake.exe -E copy_directory /opt/devkitpro/BrewProjects/Switch-Clock/library/borealis/resources /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/resources
/opt/devkitpro/msys2/usr/bin/cmake.exe -E copy_directory /opt/devkitpro/BrewProjects/Switch-Clock/assets /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/resources
/opt/devkitpro/msys2/usr/bin/cmake.exe -E remove_directory /opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/resources/font
/opt/devkitpro/tools/bin/elf2nro.exe SwitchTimer.elf SwitchTimer.nro --icon=/opt/devkitpro/BrewProjects/Switch-Clock/icon.jpg --nacp=SwitchTimer.nacp --romfsdir=/opt/devkitpro/BrewProjects/Switch-Clock/out/build/Release/resources
