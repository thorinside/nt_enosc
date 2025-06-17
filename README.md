# Ensemble Oscillator for Disting NT

This repository contains a port of the 4ms Ensemble Oscillator for the Expert Sleepers Disting NT platform. The Ensemble Oscillator is a unique polyphonic oscillator that generates a rich tapestry of sound by stacking and modulating multiple oscillator voices.

## Implementation Philosophy

Every effort has been made to use the original source code from the `enosc` directory with a minimal wrapper for the Disting NT API. The goal is to preserve the core logic and behavior of the original module while adapting it for the new platform.

## Current Limitations

The learning features for creating and saving custom scales have been omitted in the current version of this plugin.

## Features

This plugin provides access to all the core functionality of the original hardware module, translated into a parameter-based interface for the Disting NT.

*   **16 Oscillator Voices**: Create dense, complex sounds with up to 16 sine-wave oscillators.
*   **Pitch and Scale Control**: Control the root note, pitch, spread, and detuning of the oscillator bank.
*   **Three Scale Banks**: Choose from three banks of scales:
    *   **12-TET**: Standard 12-tone equal temperament scales.
    *   **Octave**: Just intonation and other non-standard tunings.
    *   **Free**: User-creatable custom scales.
*   **Custom Scale Learning**: Create and save your own custom scales.
*   **Cross FM**: Modulate the oscillators against each other for complex timbres.
*   **Twist and Warp**: Apply unique wave-shaping and distortion effects.
*   **Freeze**: Hold the current state of the oscillators for sustained drones and textures.
*   **Stereo Output**: Configure how the oscillators are distributed in the stereo field.

## Building

### Prerequisites

- **ARM GNU Toolchain**: Ensure `arm-none-eabi-gcc` and related tools are installed and in your system's `PATH`.
- **Git Submodules**: This repository uses Git submodules. Clone it using the `--recurse-submodules` flag. If you've already cloned it without the submodules, run `git submodule update --init --recursive`.

### Compilation

The included `Makefile` automates the build process.

-   **`make`**: Compiles the plugin. The output binary, `nt_enosc.o`, will be located in the `plugins/` directory.
-   **`make check`**: Verifies the compiled plugin for undefined symbols and checks its memory footprint against the Disting NT's limits.
-   **`make clean`**: Removes all build artifacts and generated source files. 