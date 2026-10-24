# LiveBus Mixer

LiveBus Mixer is a JUCE/C++17 desktop audio mixer for Windows 10/11 with 16 input channels, independent Stream and Monitor buses, channel DSP, VST3 hosting, and a Mixcraft-inspired mixer workflow.

## Features

- Up to 16 input strips with source selection, fader, peak meter, gain, pan, mute, solo, and independent Stream/Monitor routing.
- Horizontal channel navigation, add-channel source selection, and adjustable mixer panel height.
- Independent Stream and Monitor master buses.
- Per-channel and bus DSP, including EQ, vocal processing, ducking, and audio safety.
- Scenes, routing, presets, MIDI controls, playback, OBS integration, and YouTube captured-audio input.
- Automated audio, routing, DSP, scene, and plugin-host tests.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 or newer with the Desktop development with C++ workload
- CMake 3.22 or newer
- JUCE 8.0.4 (licensed separately under JUCE's terms)

The JUCE framework is not vendored in this repository. From the repository root, clone the required JUCE version into `JUCE`:

```powershell
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
```

ASIO support is optional. To enable it, obtain the Steinberg ASIO SDK under its license and place it in `asiosdk` so that `asiosdk/asio-main/common/iasiodrv.h` exists. Without the SDK, CMake builds without ASIO.

## Build

Run these commands from the repository root in a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The mixer, VST3 helper, and test executable are generated under their respective `build/*_artefacts/Release` directories.

## Test

```powershell
ctest --test-dir build -C Release --output-on-failure
```

## Audio and safety

Audio device availability, input/output channel counts, VST3 plugin availability, and YouTube loopback capture depend on the host computer and installed drivers. Connect only outputs that are appropriate for your audio setup; use the independent bus routing and Audio Safety controls to avoid feedback and excessive levels.
