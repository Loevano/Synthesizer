# Creating and Swapping DSP Modules

## Add a new module
1. Copy `modules/lowpass` to a new folder, e.g. `modules/notch`.
2. Rename class/files and update exported module name.
3. Add `add_subdirectory(modules/notch)` in root `CMakeLists.txt`.
4. Rebuild: `cmake --build build --target notch_module`.

## Swap module at runtime
- Start host with a selected module file:
  - `./build/synth_host --module build/modules/liblowpass_module.dylib`
- Rebuild that module while host is running.
- Host detects file timestamp change and reloads automatically.

## Required exports
Each module library must export:
- `createModule`
- `destroyModule`
- `getModuleName`

If one is missing, host logs an error and skips loading.
