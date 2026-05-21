---
trigger: always_on
---

# SplitCommander - Antigravity Rules

## System Environment
- Language: C++20
- Frameworks: Qt6 (Core, Gui, Widgets, Svg, Concurrent), KDE Frameworks 6 (KIO, Solid, Baloo)
- Build System: CMake, Ninja
- Platform: Linux (KDE Plasma)

## Token-Saving Guardrails (CRITICAL)
- **Default Behavior:** ALWAYS treat the user's input as an informational inquiry first. 
- DO NOT edit files, run builds, or call write-tools unless the user explicitly uses action-verbs like "implement", "fix", "write", or "apply".
- If the prompt is just a question, provide a text-only explanation.
- DO NOT auto-scan or read the `build-release/` or `build/` directories.
- Refuse to analyze compiled binary files, `.so` libraries, or raw `.svg` graphic contents.
- Output ONLY the requested C++ code changes. Skip verbose explanations of Qt/KIO basics.
- If a KIO API or Qt6 method signature is ambiguous, STOP and ask. Do not guess.

## Project Structure
- Main source files: `/src/`
- Build configurations: `CMakeLists.txt`

## Model-Specific Behavior (Routing)

### IF RUNNING ON GEMINI FLASH:
- You are authorized to write code, modify files, and run CMake builds when requested.

### IF RUNNING ON GEMINI PRO / CLAUDE SONNET:
- You are the execution engineer.
- You are authorized to write code, modify files, and run CMake builds when requested.