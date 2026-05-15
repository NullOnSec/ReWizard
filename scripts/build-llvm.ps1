#Requires -Version 5.1
<#
.SYNOPSIS
    Builds LLVM from source for ReWizard.

.DESCRIPTION
    Clones the llvm-project monorepo, configures a minimal Release build
    (X86 target only, no tools/tests), and installs it to Z:/llvm-install.
    The script is idempotent: running it again re-uses the clone and
    rebuilds only what changed.

.PARAMETER Version
    LLVM git tag to build. Default: llvmorg-19.1.0

.PARAMETER InstallPrefix
    Directory to install LLVM. Default: Z:/llvm-install

.PARAMETER Jobs
    Number of parallel Ninja jobs. Default: auto-detected from core count.
#>
param(
    [string]$Version = "llvmorg-19.1.0",
    [string]$InstallPrefix = "Z:/llvm-install",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

$llvmProjectDir = "Z:/llvm-project-$($Version -replace 'llvmorg-','')"
$llvmBuildDir   = "$llvmProjectDir/build"

if ($Jobs -eq 0) {
    $Jobs = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
}

# ------------------------------------------------------------------
# 1. Clone LLVM monorepo (shallow, single branch)
# ------------------------------------------------------------------
if (-not (Test-Path "$llvmProjectDir/llvm/CMakeLists.txt")) {
    Write-Host "Cloning LLVM $Version ..."
    if (Test-Path $llvmProjectDir) {
        Remove-Item -LiteralPath $llvmProjectDir -Recurse -Force
    }
    git clone --depth 1 --branch $Version `
        https://github.com/llvm/llvm-project.git $llvmProjectDir
} else {
    Write-Host "LLVM source already present at $llvmProjectDir"
}

# ------------------------------------------------------------------
# 2. Configure
# ------------------------------------------------------------------
if (-not (Test-Path $llvmBuildDir)) {
    New-Item -ItemType Directory -Path $llvmBuildDir -Force | Out-Null
}

$configureArgs = @(
    "-G", "Ninja"
    "-S", "$llvmProjectDir/llvm"
    "-B", $llvmBuildDir
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"
    "-DLLVM_TARGETS_TO_BUILD=X86"
    "-DLLVM_BUILD_TOOLS=OFF"
    "-DLLVM_BUILD_EXAMPLES=OFF"
    "-DLLVM_BUILD_TESTS=OFF"
    "-DLLVM_INCLUDE_TESTS=OFF"
    "-DLLVM_INCLUDE_EXAMPLES=OFF"
    "-DLLVM_INCLUDE_BENCHMARKS=OFF"
    "-DLLVM_ENABLE_BINDINGS=OFF"
    "-DLLVM_ENABLE_ZLIB=OFF"
    "-DLLVM_ENABLE_LIBXML2=OFF"
    "-DLLVM_ENABLE_TERMINFO=OFF"
    "-DLLVM_OPTIMIZED_TABLEGEN=ON"
    "-DCMAKE_INSTALL_PREFIX=$InstallPrefix"
    "-DLLVM_BUILD_LLVM_DYLIB=OFF"
    "-DLLVM_LINK_LLVM_DYLIB=OFF"
)

Write-Host "Configuring LLVM ..."
cmd /c "call `"Z:\VS\VC\Auxiliary\Build\vcvars64.bat`" && cmake $configureArgs"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

# ------------------------------------------------------------------
# 3. Build
# ------------------------------------------------------------------
Write-Host "Building LLVM with $Jobs parallel jobs ..."
cmd /c "call `"Z:\VS\VC\Auxiliary\Build\vcvars64.bat`" && cmake --build $llvmBuildDir --parallel $Jobs"
if ($LASTEXITCODE -ne 0) { throw "LLVM build failed" }

# ------------------------------------------------------------------
# 4. Install
# ------------------------------------------------------------------
Write-Host "Installing LLVM to $InstallPrefix ..."
cmd /c "call `"Z:\VS\VC\Auxiliary\Build\vcvars64.bat`" && cmake --install $llvmBuildDir"
if ($LASTEXITCODE -ne 0) { throw "LLVM install failed" }

Write-Host "Done. LLVM $Version installed to $InstallPrefix"
Write-Host "ReWizard CMake will auto-detect it on the next configure."
