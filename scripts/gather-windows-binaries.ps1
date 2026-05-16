#Requires -Version 5.1
<#
.SYNOPSIS
    Gathers Windows system binaries required for Bochs emulation.

.DESCRIPTION
    Copies ntoskrnl.exe, hal.dll, ntdll.dll, kernel32.dll, and kernelbase.dll
    from the host Windows installation into the project's binaries/ directory.

    These files are required for Bochs to boot a partial Windows kernel
    environment for dynamic analysis. They are NOT included in the git repo.

    Run this script after cloning the repository and before building Bochs
    integration.

    The script is idempotent: it will not overwrite existing files.
#>
param(
    [string]$BinariesDir = "$PSScriptRoot/../binaries",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$binariesDir = Resolve-Path $BinariesDir -ErrorAction SilentlyContinue
if (-not $binariesDir) {
    $binariesDir = (Resolve-Path "$PSScriptRoot/../binaries").Path
}

$sys32 = "$env:SystemRoot\System32"

$requiredFiles = @(
    @{ Name = "ntoskrnl.exe"; Source = "$sys32\ntoskrnl.exe"; Desc = "Windows kernel image" },
    @{ Name = "hal.dll";      Source = "$sys32\hal.dll";      Desc = "Hardware abstraction layer" },
    @{ Name = "ntdll.dll";    Source = "$sys32\ntdll.dll";    Desc = "Native API layer" },
    @{ Name = "kernel32.dll"; Source = "$sys32\kernel32.dll"; Desc = "Base Win32 API" },
    @{ Name = "kernelbase.dll"; Source = "$sys32\kernelbase.dll"; Desc = "Base API (Win7+)" }
)

Write-Host "ReWizard: Windows System Binary Gatherer"
Write-Host "========================================"
Write-Host ""
Write-Host "This script copies required Windows system binaries from your"
Write-Host "licensed Windows installation into the project's binaries/ directory."
Write-Host "These files are needed for Bochs emulation but are NOT included in git."
Write-Host ""

if (-not (Test-Path $binariesDir)) {
    New-Item -ItemType Directory -Path $binariesDir -Force | Out-Null
    Write-Host "Created: $binariesDir"
}

$copied = 0
$skipped = 0
$failed = 0

foreach ($file in $requiredFiles) {
    $dest = Join-Path $binariesDir $file.Name
    $src = $file.Source

    Write-Host ""
    Write-Host "[$($file.Name)] $($file.Desc)"

    if (-not (Test-Path $src)) {
        Write-Warning "  Source not found: $src"
        $failed++
        continue
    }

    if ((Test-Path $dest) -and -not $Force) {
        $existingSize = (Get-Item $dest).Length
        $sourceSize = (Get-Item $src).Length
        if ($existingSize -eq $sourceSize) {
            Write-Host "  Already present (size match). Skipping."
            $skipped++
            continue
        } else {
            Write-Host "  Exists but size differs. Re-copying..."
        }
    }

    try {
        Copy-Item -LiteralPath $src -Destination $dest -Force
        $size = (Get-Item $dest).Length
        Write-Host "  Copied successfully ($size bytes)"
        $copied++
    } catch {
        Write-Error "  Failed to copy: $_"
        $failed++
    }
}

Write-Host ""
Write-Host "========================================"
Write-Host "Done."
Write-Host "  Copied:   $copied"
Write-Host "  Skipped:  $skipped"
Write-Host "  Failed:   $failed"
Write-Host ""

if ($failed -gt 0) {
    Write-Warning "Some files could not be copied. Bochs emulation will not work without them."
    exit 1
} else {
    Write-Host "All required binaries are present in: $binariesDir"
    exit 0
}
