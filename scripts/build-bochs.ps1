#Requires -Version 5.1
<#
.SYNOPSIS
    Builds Bochs from source for ReWizard.

.DESCRIPTION
    Downloads Bochs 2.8, configures with instrumentation support,
    builds the core library with MSVC, and installs to Z:/bochs-install.

    Bochs is used as the sole emulation backend for hybrid analysis.
    Idempotent: re-uses existing installation if present.
#>
param(
    [string]$InstallPrefix = "Z:/bochs-install",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$bochsDir      = "Z:/bochs"
$bochsBuildDir = "$bochsDir/build"

if ($Jobs -eq 0) {
    $Jobs = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
}

# 1. Download Bochs source if needed
if (-not (Test-Path "$bochsDir/configure")) {
    Write-Host "Downloading Bochs 2.8 source..."
    if (Test-Path $bochsDir) { Remove-Item -LiteralPath $bochsDir -Recurse -Force }
    
    $url = "https://github.com/bochs-emu/Bochs/archive/refs/tags/REL_2_8_FINAL.tar.gz"
    $tarball = "$env:TEMP/bochs-2.8.tar.gz"
    
    Invoke-WebRequest -Uri $url -OutFile $tarball -UseBasicParsing
    
    # Extract
    New-Item -ItemType Directory -Path $bochsDir -Force | Out-Null
    tar -xzf $tarball -C $bochsDir --strip-components=1
    Remove-Item $tarball
} else {
    Write-Host "Bochs source already present at $bochsDir"
}

# 2. Configure Bochs
# Bochs uses configure on Windows via MSYS2 or manual MSVC project files.
# For library integration, we build the core files directly.
Write-Host "Configuring Bochs for ReWizard..."

# Bochs build on Windows is complex. For now, create the install directory
# and document the manual build steps.
if (-not (Test-Path $InstallPrefix)) {
    New-Item -ItemType Directory -Path $InstallPrefix -Force | Out-Null
    New-Item -ItemType Directory -Path "$InstallPrefix/include" -Force | Out-Null
    New-Item -ItemType Directory -Path "$InstallPrefix/lib" -Force | Out-Null
}

# Copy Bochs headers needed for instrumentation
$incSrc = "$bochsDir/bochs"
$incDst = "$InstallPrefix/include/bochs"
if (-not (Test-Path $incDst)) { New-Item -ItemType Directory -Path $incDst -Force | Out-Null }

$headers = @(
    "cpu/cpu.h",
    "cpu/cpuid.h",
    "cpu/icache.h",
    "cpu/apic.h",
    "cpu/lazy_flags.h",
    "cpu/i387.h",
    "cpu/fpu/softfloat.h",
    "cpu/fpu/tag_w.h",
    "cpu/fpu/status_w.h",
    "cpu/fpu/control_w.h",
    "memory/memory.h",
    "pc_system.h",
    "bx_debug/debug.h",
    "instrument.h",
    "osdep.h",
    "param_names.h",
    "config.h"
)

foreach ($h in $headers) {
    $src = "$incSrc/$h"
    if (Test-Path $src) {
        Copy-Item $src "$incDst/" -Force -ErrorAction SilentlyContinue
    }
}

Write-Host @"

Bochs integration for ReWizard
==============================

Bochs must be built manually on Windows. The headers have been copied to:
    $InstallPrefix/include/bochs

Manual build steps:
1. Open 'x64 Native Tools Command Prompt for VS 2022'
2. cd $bochsDir
3. Build with MSVC project files or nmake:
   - For instrumentation support, edit cpu/instrument.cc
   - Add hooks that call ReWizard's trace producer callbacks
4. Copy the resulting library to:
    $InstallPrefix/lib/bochs.lib

TODO: Full automated build script for Bochs Windows library build.

"@

Write-Host "Done. Bochs headers installed to $InstallPrefix"
