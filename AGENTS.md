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
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --preset x64-debug -DBOOST_ROOT=Z:/boost/boost_1_85_0"
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-debug"
```

The Debug preset expects Boost at `C:/boost/x64/debug`. Override with `-DBOOST_INSTALL=Z:/path/to/boost`.

## LLVM Dependency

ReWizard links against the LLVM C++ libraries. CMake searches the following locations automatically:

1. `Z:/llvm-install/lib/cmake/llvm` (preferred — full C++ build)
2. `Z:/llvm/lib/cmake/llvm` (fallback — C API only, headers-only mode)

If neither is present, the IR layer falls back to forward declarations and stub implementations.

**Building LLVM from source (one-time):**

```powershell
.\scripts\build-llvm.ps1
```

This clones LLVM 19.1.0, configures a minimal Release build (X86 target only), and installs to `Z:/llvm-install`. The script is idempotent.

**Manual build:**

```bash
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake -G Ninja -S Z:/llvm-project-19.1.0/llvm -B Z:/llvm-project-19.1.0/build -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_BUILD_TOOLS=OFF -DLLVM_BUILD_TESTS=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_ENABLE_BINDINGS=OFF -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_OPTIMIZED_TABLEGEN=ON -DCMAKE_INSTALL_PREFIX=Z:/llvm-install -DLLVM_BUILD_LLVM_DYLIB=OFF -DLLVM_LINK_LLVM_DYLIB=OFF"
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --build Z:/llvm-project-19.1.0/build"
cmd /c "call Z:\VS\VC\Auxiliary\Build\vcvars64.bat && cmake --install Z:/llvm-project-19.1.0/build"
```

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