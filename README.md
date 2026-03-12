# Switch Timer App

A timer for the Nintendo Switch.

## Features

- **Flexible Timer Modes**:
  - Countdown timer with configurable duration
  - Pause/resume functionality
  - Vibrate and sound when timer completes

- **Time Setting Mode**:
  - Set custom timer duration (up to 99:59)
  - Easy adjustment of minutes and seconds

- **Saved Timers**:
  - Save frequently used timers and load them
- **Feedback & Alerts**:
  - Haptic feedback when timer completes
  - Audio beeps when timer completes

## Controls

### Timer Controls
- **A Button**: Start/Pause timer
- **X Button**: Reset to 5:00
- **L Button**: Stop timer
- **Y Button**: Toggle time setting mode (Set/Done)
- **RB Button**: Save current timer duration
- **ZL Button**: Load saved timers

### In Time Setting Mode
- **D-Pad Up/Down**: Increase/Decrease time
- **D-Pad Left/Right**: Switch between minutes/seconds
- **Y Button**: Exit setting mode

### Saved Timers Screen
- **D-Pad Up/Down**: Navigate through saved timers
- **A Button**: Select and load a timer
- **B Button**: Return to timer screen

### System
- **+ Button**: Exit application

## Building

### Prerequisites

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with Switch development tools installed
- `switch-dev` package
- `libnx` package
- CMake 3.10 or higher
- Ninja build system

### Building the Application

1. Open a terminal in the project directory
2. Create and navigate to the build directory:
   ```bash
   mkdir -p build-ninja
   cd build-ninja
   ```
3. Configure the project with CMake:
   ```bash
   cmake .. -G Ninja
   ```
4. Build the project:
   ```bash
   ninja
   ```
5. Generate the .nro file:
   ```bash
   ninja SwitchTimer.nro
   ```
6. The output will be `SwitchTimer.nro` in the `build-ninja` directory

### Installing on Switch

1. Copy the `SwitchTimer.nro` file from `build-ninja` to the `switch` folder on your SD card
2. Launch the Homebrew Launcher on your Switch
3. Select and run the Switch Timer

## License

This project is licensed under the GPL-3.0 License - see the license file for details.
