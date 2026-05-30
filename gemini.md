- **Default Behavior:** ALWAYS treat the user's input as an informational inquiry first. 
- DO NOT edit files, run builds, or call write-tools unless the user explicitly uses action-verbs like "implement", "fix", "write", or "apply".
- If the prompt is just a question, provide a text-only explanation.

# SplitCommander - Antigravity Rules

## System Environment
- Language: C++20
- Frameworks: Qt6 (Core, Gui, Widgets, Svg, Concurrent), KDE Frameworks 6 (KIO, Solid, Baloo)
- Build System: CMake, Ninja
- Platform: Linux (KDE Plasma)

## NASA Power of 10 (Qt-Adapted) - STRICT COMPLIANCE REQUIRED
1. **Simple Control Flow:** No `goto`. Code must be linear.
2. **Fixed Loops:** Prefer range-based `for`. Avoid open-ended `while` loops.
3. **Memory Management:** NO raw `malloc`/`free`. NO raw `new`/`delete` UNLESS immediately passed to a Qt Parent (e.g., `new QWidget(this)`). Otherwise, ONLY use Smart Pointers (`std::unique_ptr`).
4. **Short Functions:** Maximum 60 lines per function. Split larger functions logically.
5. **Compact Data:** Declare variables in the smallest possible scope.
6. **Assertions:** Use `Q_ASSERT` or `assert` to check for preconditions, postconditions, and invariants (aim for at least 2 per function).
7. **Return Values & Parameters:** Always check return values (use `[[nodiscard]]`). Validate all function input parameters at the start of the function.
8. **Preprocessor Limit:** NO complex `#define` macros. Use `constexpr` and inline functions.
9. **No Raw Pointers (NASA C++ Adaptation):** Absolutely NO raw pointers for memory ownership. Use C++ references (`&`) for passing parameters. Use `std::unique_ptr` for dynamic objects. The ONLY exception for raw `*` pointers is non-owning Qt parent-child hierarchies.
10. **No Function Pointers:** Use Qt Signals/Slots or C++ lambdas instead of raw function pointers.
11. **Strict Compilation:** Code MUST compile without warnings (`-Werror` active). All `clang-tidy` checks must pass.
12. **Concurrency (Qt Specific):** NO raw `std::thread`. Use `QThreadPool`, `QRunnable`, or `QtConcurrent`. No unprotected shared mutable state.

## Token-Saving Guardrails (CRITICAL)
- DO NOT auto-scan or read the `build-release/` or `build/` directories.
- Refuse to analyze compiled binary files, `.so` libraries, or raw `.svg` graphic contents.
- Output ONLY the requested C++ code changes. Skip verbose explanations of Qt/KIO basics.
- If a KIO API or Qt6 method signature is ambiguous, STOP and ask. Do not guess.

## Project Structure
- Main source files: `/src/`
- Build configurations: `CMakeLists.txt`
