- **Default Behavior:** ALWAYS treat the user's input as an informational inquiry first. 
- DO NOT edit files, run builds, or call write-tools unless the user explicitly uses action-verbs like "implement", "fix", "write", or "apply".
- If the prompt is just a question, provide a text-only explanation.

# SplitCommander - Claude Code Rules

## Environment
- Stack: C++20 / Qt6 / KF6 (KIO)
- Build: `cmake -B build-release -S . -G Ninja`

## Token Control
- **No Deep Scans:** Act only on files specified by the user. Do not crawl `/src` recursively.
- **Build Errors:** If `cmake --build` fails, output the first 3 lines of the error and STOP. Do not loop to auto-fix.
- **Style:** Compact C++20. Prefer standard Qt6 patterns. Do not generate boilerplate comments.

## Commands
- Build: `cmake --build build-release`
