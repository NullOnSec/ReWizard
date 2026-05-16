#Requires -Version 5.1
<#
.SYNOPSIS
    Installs Bochs libraries and headers for ReWizard integration.

.DESCRIPTION
    Copies built Bochs static libraries and required headers to Z:/bochs-install.
    This is called after building Bochs from source.

    The custom ReWizard instrumentation module replaces the default stubs.
#>
param(
    [string]$BochsSourceDir = "Z:/bochs/bochs-3.0-msvc-src/bochs-3.0",
    [string]$InstallPrefix = "Z:/bochs-install"
)

$ErrorActionPreference = "Stop"

$bochsBuildDir = "$BochsSourceDir/obj-release"

if (-not (Test-Path $bochsBuildDir)) {
    throw "Bochs build directory not found at $bochsBuildDir. Build Bochs first."
}

Write-Host "Installing Bochs to $InstallPrefix ..."

# Create directories
$dirs = @(
    "$InstallPrefix/lib",
    "$InstallPrefix/include/bochs",
    "$InstallPrefix/include/bochs/cpu",
    "$InstallPrefix/include/bochs/cpu/fpu",
    "$InstallPrefix/include/bochs/memory",
    "$InstallPrefix/include/bochs/gui",
    "$InstallPrefix/include/bochs/iodev",
    "$InstallPrefix/include/bochs/bx_debug",
    "$InstallPrefix/include/bochs/instrument"
)

foreach ($d in $dirs) {
    if (-not (Test-Path $d)) {
        New-Item -ItemType Directory -Path $d -Force | Out-Null
    }
}

# Copy libraries
$libs = @(
    "cpu.lib",
    "avx.lib",
    "memory.lib",
    "fpu.lib",
    "softfloat3e.lib",
    "cpudb.lib",
    "bx_debug.lib",
    "stubs.lib",
    "gui.lib",
    "iodev.lib",
    "iodev_display.lib",
    "iodev_hdimage.lib",
    "iodev_network.lib",
    "iodev_sound.lib",
    "iodev_usb.lib"
)

foreach ($lib in $libs) {
    $src = "$bochsBuildDir/$lib"
    if (Test-Path $src) {
        Copy-Item $src "$InstallPrefix/lib/" -Force
        Write-Host "  Installed: $lib"
    } else {
        Write-Warning "  Missing: $lib"
    }
}

# Copy headers
$headers = @(
    "bochs.h",
    "config.h",
    "osdep.h",
    "param_names.h",
    "instrument/stubs/instrument.h"
)

foreach ($h in $headers) {
    $src = "$BochsSourceDir/$h"
    if (Test-Path $src) {
        Copy-Item $src "$InstallPrefix/include/bochs/" -Force -Recurse
        Write-Host "  Installed: $h"
    }
}

# Copy CPU headers
$cpuHeaders = Get-ChildItem "$BochsSourceDir/cpu" -Filter "*.h" -Recurse
foreach ($h in $cpuHeaders) {
    $relativePath = $h.FullName.Substring($BochsSourceDir.Length + 1)
    $destDir = "$InstallPrefix/include/bochs/" + (Split-Path $relativePath -Parent)
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }
    Copy-Item $h.FullName $destDir -Force
}

# Copy memory headers
$memHeaders = Get-ChildItem "$BochsSourceDir/memory" -Filter "*.h" -Recurse
foreach ($h in $memHeaders) {
    Copy-Item $h.FullName "$InstallPrefix/include/bochs/memory/" -Force
}

# Copy gui headers
$guiHeaders = Get-ChildItem "$BochsSourceDir/gui" -Filter "*.h" -Recurse
foreach ($h in $guiHeaders) {
    Copy-Item $h.FullName "$InstallPrefix/include/bochs/gui/" -Force
}

# Copy iodev headers
$ioHeaders = Get-ChildItem "$BochsSourceDir/iodev" -Filter "*.h" -Recurse
foreach ($h in $ioHeaders) {
    $relativePath = $h.FullName.Substring($BochsSourceDir.Length + 1)
    $destDir = "$InstallPrefix/include/bochs/" + (Split-Path $relativePath -Parent)
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }
    Copy-Item $h.FullName $destDir -Force
}

# Copy custom ReWizard instrumentation
Copy-Item "$BochsSourceDir/instrument/rewizard/instrument.cc" "$InstallPrefix/include/bochs/instrument/" -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Bochs installed to $InstallPrefix"
Write-Host "Libraries: $InstallPrefix/lib"
Write-Host "Headers: $InstallPrefix/include/bochs"
