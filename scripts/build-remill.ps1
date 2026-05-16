#Requires -Version 5.1
<#
.SYNOPSIS
    Builds remill from source for ReWizard.

.DESCRIPTION
    Clones remill v6.0.1, builds dependencies via superbuild,
    patches remill for x86-only support, builds with MSVC/clang-cl,
    and installs to Z:/remill-install.

    Idempotent: re-uses clone, rebuilds only what changed.
#>
param(
    [string]$InstallPrefix = "Z:/remill-install",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$remillDir      = "Z:/remill"
$remillBuildDir = "$remillDir/build"
$depsBuildDir   = "$remillDir/dependencies/build"

if ($Jobs -eq 0) {
    $Jobs = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
}

function Apply-RemillPatches {
    Write-Host "Applying x86-only patches to remill source..."

    # Patch 1: Root CMakeLists.txt - only link x86 LLVM components
    $rootCMake = "$remillDir/CMakeLists.txt"
    $content = Get-Content $rootCMake -Raw
    $content = $content -replace "aarch64info aarch64desc aarch64codegen aarch64asmparser\s*\n\s*armcodegen armasmparser\s*\n\s*interpreter mcjit\s*\n\s*x86info x86codegen x86asmparser\s*\n\s*sparccodegen sparcasmparser", "x86info x86codegen x86asmparser"
    Set-Content $rootCMake $content -NoNewline

    # Patch 2: lib/Arch/CMakeLists.txt - only build x86 arch
    $archCMake = "$remillDir/lib/Arch/CMakeLists.txt"
    Set-Content $archCMake "add_subdirectory(X86)`n`ntarget_link_libraries(remill_arch PUBLIC`n  remill_arch_x86`n)`n`nif(REMILL_ENABLE_INSTALL_TARGET)`n  install(TARGETS remill_arch`n    EXPORT remillTargets)`nendif()`n" -NoNewline

    # Patch 3: cmake/BCCompiler.cmake - find llvm-link without target
    $bcCompiler = "$remillDir/cmake/BCCompiler.cmake"
    $content = Get-Content $bcCompiler -Raw
    if (-not ($content -match "find_program\(LLVMLINK_PATH")) {
        $old = 'get_target_property(LLVMLINK_PATH llvm-link LOCATION)' + "`n" + 'if(NOT EXISTS "${LLVMLINK_PATH}")' + "`n" + '  message(FATAL_ERROR "llvm-link not found")' + "`n" + 'endif()' + "`n`n" + 'get_filename_component(LLVMLINK_PATH_DIR ${LLVMLINK_PATH} DIRECTORY)' + "`n" + 'find_program(CLANG_PATH NAMES clang++ clang PATHS ${LLVMLINK_PATH_DIR} NO_DEFAULT_PATH REQUIRED)'
        $new = 'if(TARGET llvm-link)' + "`n" + '  get_target_property(LLVMLINK_PATH llvm-link LOCATION)' + "`n" + 'endif()' + "`n" + 'if(NOT EXISTS "${LLVMLINK_PATH}")' + "`n" + '  find_program(LLVMLINK_PATH NAMES llvm-link PATHS "${LLVM_TOOLS_BINARY_DIR}" "${LLVM_BINARY_DIR}/bin" NO_DEFAULT_PATH)' + "`n" + 'endif()' + "`n" + 'if(NOT EXISTS "${LLVMLINK_PATH}")' + "`n" + '  find_program(LLVMLINK_PATH NAMES llvm-link)' + "`n" + 'endif()' + "`n" + 'if(NOT EXISTS "${LLVMLINK_PATH}")' + "`n" + '  message(FATAL_ERROR "llvm-link not found")' + "`n" + 'endif()' + "`n`n" + 'get_filename_component(LLVMLINK_PATH_DIR ${LLVMLINK_PATH} DIRECTORY)' + "`n" + 'find_program(CLANG_PATH NAMES clang++ clang PATHS ${LLVMLINK_PATH_DIR} "${LLVM_TOOLS_BINARY_DIR}" "${LLVM_BINARY_DIR}/bin" NO_DEFAULT_PATH)' + "`n" + 'if(NOT CLANG_PATH)' + "`n" + '  find_program(CLANG_PATH NAMES clang++ clang)' + "`n" + 'endif()' + "`n" + 'if(NOT CLANG_PATH)' + "`n" + '  message(FATAL_ERROR "clang++ not found")' + "`n" + 'endif()'
        $content = $content -replace [regex]::Escape($old), $new
        Set-Content $bcCompiler $content -NoNewline
    }

    # Patch 4: lib/Arch/Arch.cpp - only support x86 architectures
    $archCpp = "$remillDir/lib/Arch/Arch.cpp"
    $content = Get-Content $archCpp -Raw
    $content = $content -replace "case kArchX86_SLEIGH:\s*case kArchAArch32LittleEndian:\s*case kArchThumb2LittleEndian:\s*case kArchSparc32:\s*case kArchSparc32_SLEIGH:\s*case kArchPPC: return 32;", "case kArchX86_AVX:`n    case kArchX86_AVX512: return 32;"
    $content = $content -replace "case kArchAMD64_SLEIGH:\s*case kArchAArch64LittleEndian:\s*case kArchAArch64LittleEndian_SLEIGH:\s*case kArchSparc64: return 64;", "case kArchAMD64_AVX:`n    case kArchAMD64_AVX512: return 64;"
    $getArchPattern = "auto Arch::GetArchByName\(llvm::LLVMContext \*context_, OSName os_name_,\s*ArchName arch_name_\) -> ArchPtr \{[\s\S]*?\n\}"
    $getArchReplacement = 'auto Arch::GetArchByName(llvm::LLVMContext *context_, OSName os_name_,' + "`n" + '                         ArchName arch_name_) -> ArchPtr {' + "`n" + '  switch (arch_name_) {' + "`n" + '    case kArchInvalid:' + "`n" + '      LOG(FATAL) << "Unrecognized architecture.";' + "`n" + '      return nullptr;' + "`n`n" + '    case kArchX86: {' + "`n" + '      DLOG(INFO) << "Using architecture: X86";' + "`n" + '      return GetX86(context_, os_name_, arch_name_);' + "`n" + '    }' + "`n`n" + '    case kArchX86_AVX: {' + "`n" + '      DLOG(INFO) << "Using architecture: X86, feature set: AVX";' + "`n" + '      return GetX86(context_, os_name_, arch_name_);' + "`n" + '    }' + "`n`n" + '    case kArchX86_AVX512: {' + "`n" + '      DLOG(INFO) << "Using architecture: X86, feature set: AVX512";' + "`n" + '      return GetX86(context_, os_name_, arch_name_);' + "`n" + '    }' + "`n`n" + '    case kArchAMD64: {' + "`n" + '      DLOG(INFO) << "Using architecture: AMD64";' + "`n" + '      return GetX86(context_, os_name_, arch_name_);' + "`n" + '    }' + "`n`n" + '    case kArchAMD64_AVX: {' + "`n" + '      DLOG(INFO) << "Using architecture: AMD64, feature set: AVX";' + "`n" + '      return GetX86(context_, os_name_, arch_name_);' + "`n" + '    }' + "`n`n" + '    case kArchAMD64_AVX512: {' + "`n" + '      DLOG(INFO) << "Using architecture: AMD64, feature set: AVX512";' + "`n" + '      return GetX86(context_, os_name_, arch_name_);' + "`n" + '    }' + "`n`n" + '    default: {' + "`n" + '      LOG(FATAL) << "Architecture not supported in this build: " << GetArchName(arch_name_);' + "`n" + '      return nullptr;' + "`n" + '    }' + "`n" + '  }' + "`n" + '}'
    $content = [regex]::Replace($content, $getArchPattern, $getArchReplacement)
    $content = $content -replace "ArchLocker Arch::Lock\(ArchName arch_name_\) \{[\s\S]*?\n\}", "ArchLocker Arch::Lock(ArchName arch_name_) { (void) arch_name_; return ArchLocker(); }"
    Set-Content $archCpp $content -NoNewline

    Write-Host "Patches applied."
}

# 1. Clone remill if needed
if (-not (Test-Path "$remillDir/CMakeLists.txt")) {
    Write-Host "Cloning remill v6.0.1 ..."
    if (Test-Path $remillDir) { Remove-Item -LiteralPath $remillDir -Recurse -Force }
    git clone --depth 1 --branch v6.0.1 https://github.com/lifting-bits/remill.git $remillDir
} else {
    Write-Host "remill source already present at $remillDir"
}

# 2. Apply patches
Apply-RemillPatches

# 3. Ensure llvm-link is available
if (-not (Test-Path "Z:/llvm-install/bin/llvm-link.exe")) {
    Write-Host "Building llvm-link from existing LLVM source..."
    cmd /c "call `"Z:/VS/VC/Auxiliary/Build/vcvars64.bat`" && cmake --build Z:/llvm-project-19.1.0/build --target llvm-link"
    if (-not (Test-Path "Z:/llvm-project-19.1.0/build/bin/llvm-link.exe")) { throw "Failed to build llvm-link" }
    Copy-Item "Z:/llvm-project-19.1.0/build/bin/llvm-link.exe" "Z:/llvm-install/bin/llvm-link.exe" -Force
    Write-Host "llvm-link installed to Z:/llvm-install/bin/"
}

# 4. Build dependencies superbuild
$env:PATH = "$env:PATH;Z:/VS/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"

if (-not (Test-Path "$depsBuildDir/build.ninja")) {
    Write-Host "Configuring remill dependencies superbuild..."
    cmd /c "call `"Z:/VS/VC/Auxiliary/Build/vcvars64.bat`" && cmake -G Ninja -S Z:/remill/dependencies -B $depsBuildDir -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DUSE_EXTERNAL_LLVM=ON -DCMAKE_PREFIX_PATH=Z:/llvm-install -DCMAKE_INSTALL_PREFIX=$InstallPrefix"
    if ($LASTEXITCODE -ne 0) { throw "Dependencies configure failed" }
}

Write-Host "Building remill dependencies (this may take a while)..."
cmd /c "call `"Z:/VS/VC/Auxiliary/Build/vcvars64.bat`" && cmake --build $depsBuildDir --parallel $Jobs"
if ($LASTEXITCODE -ne 0) { throw "Dependencies build failed" }

# 5. Build remill
if (-not (Test-Path "$remillBuildDir/build.ninja")) {
    Write-Host "Configuring remill..."
    cmd /c "call `"Z:/VS/VC/Auxiliary/Build/vcvars64.bat`" && cmake -G Ninja -S $remillDir -B $remillBuildDir -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_PREFIX_PATH=$InstallPrefix -DCMAKE_INSTALL_PREFIX=$InstallPrefix -DCMAKE_BUILD_TYPE=Release -DREMILL_ENABLE_TESTING=OFF"
    if ($LASTEXITCODE -ne 0) { throw "remill configure failed" }
}

Write-Host "Building remill..."
cmd /c "call `"Z:/VS/VC/Auxiliary/Build/vcvars64.bat`" && cmake --build $remillBuildDir --parallel $Jobs"
if ($LASTEXITCODE -ne 0) { throw "remill build failed" }

# 6. Manual install (cmake install has a sleigh file bug)
Write-Host "Installing remill libraries and headers to $InstallPrefix ..."

$libSrc = "$remillBuildDir/lib"
$libDst = "$InstallPrefix/lib"
if (-not (Test-Path $libDst)) { New-Item -ItemType Directory -Path $libDst -Force | Out-Null }
Copy-Item "$libSrc/Arch/remill_arch.lib" "$libDst/remill_arch.lib" -Force
Copy-Item "$libSrc/Arch/X86/remill_arch_x86.lib" "$libDst/remill_arch_x86.lib" -Force
Copy-Item "$libSrc/BC/remill_bc.lib" "$libDst/remill_bc.lib" -Force
Copy-Item "$libSrc/OS/remill_os.lib" "$libDst/remill_os.lib" -Force
Copy-Item "$libSrc/Version/remill_version.lib" "$libDst/remill_version.lib" -Force

$incSrc = "$remillDir/include"
$incDst = "$InstallPrefix/include"
if (-not (Test-Path $incDst)) { New-Item -ItemType Directory -Path $incDst -Force | Out-Null }
robocopy $incSrc $incDst /E /MT /R:0 /W:0 /FFT /XD .git | Out-Null

$cmakeDst = "$InstallPrefix/lib/cmake/remill"
if (-not (Test-Path $cmakeDst)) { New-Item -ItemType Directory -Path $cmakeDst -Force | Out-Null }
Copy-Item "$remillBuildDir/remillConfig.cmake" "$cmakeDst/remillConfig.cmake" -Force -ErrorAction SilentlyContinue
Copy-Item "$remillBuildDir/remillConfigVersion.cmake" "$cmakeDst/remillConfigVersion.cmake" -Force -ErrorAction SilentlyContinue
Copy-Item "$remillBuildDir/remillTargets.cmake" "$cmakeDst/remillTargets.cmake" -Force -ErrorAction SilentlyContinue
Copy-Item "$remillBuildDir/remillTargets-release.cmake" "$cmakeDst/remillTargets-release.cmake" -Force -ErrorAction SilentlyContinue

Write-Host "Done. remill v6.0.1 (x86-only) installed to $InstallPrefix"
Write-Host "ReWizard CMake will auto-detect it on the next configure."
