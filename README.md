# Giuroll-Desync-Detector
Interfaces with Giruoll to display whether the game is desynced or not.
# Installation
1. Ensure that your version of Giuroll is at least 0.6.13, downloadable [here](https://github.com/Giufin/giuroll/releases),
2. Download the latest `giuroll_desync_detector.dll` mod from the [releases](https://github.com/kookie2332/Giuroll-Desync-Detector/releases) section,
3. Move `giuroll_desync_detector.dll` into a folder named `giuroll_desync_detector` within the `Modules` folder of the game,
4. Add the line `giurollDesyncDetector=Modules/giuroll_desync_detector/giuroll_desync_detector.dll` into your `SWRSToys.ini` file.

<strong>Note:</strong>
The mod will not work if you are using Giruoll version 0.6.12 or earlier. A pop-up message box will appear in the network screen if this is the case for you.

# Issues
Feel free to report any issues under the [Issues](https://github.com/kookie2332/Giuroll-Desync-Detector/issues) tab. Please:
- Clearly describe what the problem is,
- When you believe the problem occurs, and
- Show the other mods that were active during the problem's occurrence. You can find the active mods in your `SWRSToys.ini` file.

I may close an issue if it written too vaguely for me to understand the problem being described.

# Build
Requires CMake, git and the Visual Studio Compiler (MSVC).
Both git and CMake must be in the PATH environment variable.

All commands are to be run in `x86 Native Tools Command Prompt for VS 20XX`, unless stated otherwise.

## Initialisation
1. Change directory to desired location. In this example it will be `C:\Users\_Kookie\SokuProjects`.

```cd C:\Users\_Kookie\SokuProjects```

2. Clone the repo and initialize.
```
git clone https://github.com/kookie2332/Giuroll-Desync-Detector.git
cd Giuroll-Desync-Detector
git submodule init
git submodule update
mkdir build
cd build
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
```
To build in Release, replace `-DCMAKE_BUILD_TYPE=Debug` with `-DCMAKE_BUILD_TYPE=Release`.

## Compiling
1. Go to the build directory (if you did the previous step you're already inside)

```cd C:\Users\_Kookie\SokuProjects\Giuroll-Desync-Detector\build```

2. Invoke the compiler by running 

```cmake --build . --target giuroll_desync_detector```

3. The file `giuroll_desync_detector.dll` will appear inside the build folder.
4. To test the mod, you can add 

```Giuroll_Desync_Detector=C:/Users/_Kookie/SokuProjects/Giuroll-Desync-Detector/build/giuroll_desync_detector.dll``` 

on a new line in your `SWRSToys.ini` file.
