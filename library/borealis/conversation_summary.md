# Borealis Demo Build & Troubleshooting Summary

This document summarizes the conversation regarding building the **borealis** demo, specifically for the Nintendo Switch, and resolving environment-related errors.

## 1. General Build Instructions
The borealis project uses CMake for cross-platform builds. The general steps are:
- Create a build directory: `cmake -B build`
- Configure for your platform: `cmake -S . -B build -DPLATFORM_<NAME>=ON`
- Build the project: `cmake --build build`

### Platform Options:
- `PLATFORM_DESKTOP`: Windows, macOS, Linux
- `PLATFORM_SWITCH`: Nintendo Switch
- `PLATFORM_ANDROID`: Android
- `PLATFORM_IOS`: iOS

## 2. Nintendo Switch Build Process
To build for the Nintendo Switch, you need **devkitPro** installed with `devkitA64` and `libnx`.

### Prerequisites:
- **devkitPro**: Set the `DEVKITPRO` environment variable.
- **Tools**: `nacptool`, `elf2nro`, and `dkp-pacman`.

### Build Commands:
```bash
# Configure with deko3d (native Switch graphics)
cmake -S . -B build/switch -G "Ninja" \
  -DPLATFORM_SWITCH=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_DEKO3D=ON

# Build the NRO
cmake --build build/switch --target borealis_demo.nro
```

## 3. Troubleshooting & Errors

### Error: "multiple target patterns" (GNU Make)
- **Cause**: This occurs in MSYS2/Windows environments when CMake generates `compiler_depend.make` files containing mixed path styles (e.g., `C:/path` inside a Unix-style Makefile).
- **Fix 1 (Recommended)**: Use the **Ninja** generator (`-G "Ninja"`), which handles Windows paths more robustly than GNU Make.
- **Fix 2**: Disable compiler dependency scanning by adding `-DCMAKE_DEPENDS_USE_COMPILER=FALSE` to your CMake command.

### Error: "Ninja not found" in MSYS2
- **Cause**: The Ninja build tool is missing from the MSYS2 environment.
- **Fix**: Install it using the devkitPro package manager:
  ```bash
  dkp-pacman -S ninja
  ```

## 4. Key Project Files
- [CMakeLists.txt](file:///c:/Users/Nasra/Desktop/borealis-35734ab7e40e6a790582f6d00721bbfbf8543429/borealis-35734ab7e40e6a790582f6d00721bbfbf8543429/CMakeLists.txt): Contains NRO packaging logic and demo configuration.
- [toolchain.cmake](file:///c:/Users/Nasra/Desktop/borealis-35734ab7e40e6a790582f6d00721bbfbf8543429/borealis-35734ab7e40e6a790582f6d00721bbfbf8543429/library/cmake/toolchain.cmake): Configures the Switch toolchain and graphics backends.
- [build_libromfs_generator.sh](file:///c:/Users/Nasra/Desktop/borealis-35734ab7e40e6a790582f6d00721bbfbf8543429/borealis-35734ab7e40e6a790582f6d00721bbfbf8543429/build_libromfs_generator.sh): Helper script for resource bundling.

---
*Generated on 2026-02-13*
