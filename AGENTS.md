# ReWizard Project Instructions

## Project Overview

ReWizard is a C++20 binary analysis framework for x86/x86_64 reverse engineering. It uses a pass-based architecture inspired by compiler IR pipelines. Read `docs/PROJECT.md` for the full architecture and `docs/WORK_PLAN.md` for the development roadmap.

## Code Conventions

- C++20 standard, MSVC + Ninja build system
- All code lives under `ReWizardLib/` (library) and `ReWizardCLI/` (CLI frontend)
- Headers use `#ifndef` include guards, not `#pragma once`
- Namespaces: `ReWizard` for all library code
- Passes auto-register via `PassRegistrar<T>` — include the new pass header in `PassProvider.cpp`
- Logging uses spdlog (`spdlog::info`, `spdlog::debug`, `spdlog::warn`, `spdlog::error`)
- Smart pointers (`unique_ptr`, `make_unique`) over raw owning pointers
- No comments in code unless explicitly requested
- Dependencies download to `Z:`, never `C:`

## Build Commands

**IMPORTANT:** All build commands must run inside a Visual Studio Developer Command Prompt environment. Because each shell invocation starts a new process, you MUST chain the .bat and build commands together using `cmd /c`:

```bash
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvarsamd64_x86.bat && cmake --preset x64-debug -DBOOST_ROOT=Z:/boost/boost_1_85_0"
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvarsamd64_x86.bat && cmake --build out/build/x64-debug"
```

The Debug preset expects Boost at `C:/boost/x64/debug`. Override with `-DBOOST_INSTALL=Z:/path/to/boost`.

## Testing

Tests will be under `tests/`. Build with:

```bash
cmake --build out/build/x64-debug --target ReWizardTests
ctest --test-dir out/build/x64-debug
```

## Key Files

- Architecture overview: `docs/PROJECT.md`
- Work plan / roadmap: `docs/WORK_PLAN.md`
- Pass registration: `ReWizardLib/source/Analysis/PassProvider.cpp`
- Pass base class: `ReWizardLib/include/ReWizard/Analysis/Passes/BasePass.hpp`
- CLI entry: `ReWizardCLI/main.cpp`

## Git Conventions

- Commit messages: `<type>(<scope>): <description>` where type is one of `feat, fix, refactor, test, docs, chore, build`
- Scope examples: `analysis, loader, disasm, cli, pass, cfg, hybrid`
- Test every change before committing
- **Branching:** Push stable changes to `main`, unstable/experimental changes to `dev` branches
- **End-of-session commits:** Before committing, tell the user what will be committed and to which branch, then ask for confirmation