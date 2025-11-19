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

- **Feedback & Alerts**:
  - Haptic feedback when timer completes
  - Audio beeps when timer completes

## Controls

### Timer Controls
- **A Button**: Start/Stop/Pause timer
- **X Button**: Reset to 5:00
- **L Button**: Stop timer and reset to 5:00
- **Y Button**: Toggle time setting mode

### In Time Setting Mode
- **D-Pad Up/Down**: Increase/Decrease time
- **D-Pad Left/Right**: Switch between minutes/seconds
- **Y Button**: Exit setting mode

### System
- **+ Button**: Exit application

## Building

### Prerequisites

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with Switch development tools installed
- `switch-dev` package
- `libnx` package

### Building the Application

1. Open a terminal in the project directory
2. Run `make` to build the project
3. The output will be `SwitchTimer.nro` in the project directory

### Installing on Switch

1. Copy the `SwitchTimer.nro` file to the `switch` folder on your SD card
2. Launch the Homebrew Launcher on your Switch
3. Select and run the Switch Timer

## License

This project is licensed under the GPL-3.0 License - see the license file for details.
