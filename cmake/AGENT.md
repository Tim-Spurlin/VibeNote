# AGENT.md

## Purpose
CMake helper modules and toolchain files used by both app and daemon builds.

## Sibling files
- **toolchains.cmake** – selects compilers and Qt paths.
- **warnings.cmake** – enforces warnings-as-errors and common flags.

## Integration
Included from top-level and subproject CMakeLists to keep build settings consistent.
