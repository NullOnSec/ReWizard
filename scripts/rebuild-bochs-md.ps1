#Requires -Version 5.1
<#
.SYNOPSIS
    Rebuilds Bochs with /MD (dynamic CRT) to match ReWizard's runtime.

.DESCRIPTION
    Bochs was originally built with /MT (static CRT) which is incompatible
    with ReWizard and its dependencies (LLVM, remill, LIEF) which all use /MD.
    This script overrides the RuntimeLibrary setting via an MSBuild .props file,
    rebuilds Bochs, and copies the resulting libraries/object files to
    Z:/bochs-install.
#>
param(
    [string]$BochsSrcDir = "Z:/bochs/bochs-3.0-msvc-src/bochs-3.0",
    [string]$InstallPrefix = "Z:/bochs-install"
)

$ErrorActionPreference = "Stop"

# 1. Create a .props file that overrides RuntimeLibrary for all C++ compiles
$propsPath = "$env:TEMP\bochs_md_override.props"
$propsContent = @'
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup Condition="'$(Configuration)'=='Release'">
    <ClCompile>
      <RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)'=='Debug'">
    <ClCompile>
      <RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
'@
$propsContent | Set-Content -Path $propsPath -Encoding UTF8

$solution = "$BochsSrcDir\vs2019\bochs.sln"
if (-not (Test-Path $solution)) {
    Write-Error "Bochs solution not found: $solution"
    exit 1
}

Write-Host "Rebuilding Bochs with /MD (dynamic CRT)..."
Write-Host "Props file: $propsPath"

# 2. Clean old object files to force rebuild
$objDir = "$BochsSrcDir\obj-release"
if (Test-Path $objDir) {
    Write-Host "Cleaning old object files..."
    Remove-Item -LiteralPath $objDir -Recurse -Force
    New-Item -ItemType Directory -Path $objDir | Out-Null
}

# 3. Build Release x64 with /MD override
# Use VS Developer Command Prompt environment
$vsPath = "${env:VSINSTALLDIR}\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $vsPath)) {
    # Fallback: find msbuild
    $vsPath = (Get-Command msbuild -ErrorAction SilentlyContinue).Source
    if (-not $vsPath) {
        Write-Error "MSBuild not found. Run from a VS Developer Command Prompt."
        exit 1
    }
}

& $vsPath $solution `
    /p:Configuration=Release `
    /p:Platform=x64 `
    /p:ForceImportBeforeCppTargets=$propsPath `
    /m `
    /verbosity:minimal

if ($LASTEXITCODE -ne 0) {
    Write-Error "Bochs build failed. Check output above."
    exit 1
}

Write-Host "Build succeeded. Copying libraries and objects to $InstallPrefix..."

# 4. Copy libs
$libSrc = "$BochsSrcDir\obj-release\*.lib"
$libDst = "$InstallPrefix\lib"
if (-not (Test-Path $libDst)) { New-Item -ItemType Directory -Path $libDst -Force | Out-Null }
Copy-Item $libSrc $libDst -Force

# 5. Copy objects we need for init
$neededObjs = @(
    "main.obj",
    "config.obj",
    "plugin.obj",
    "pc_system.obj",
    "logio.obj",
    "osdep.obj",
    "bxthread.obj",
    "crc.obj"
)
foreach ($obj in $neededObjs) {
    $src = "$BochsSrcDir\obj-release\$obj"
    if (Test-Path $src) {
        Copy-Item $src $libDst -Force
    } else {
        Write-Warning "Object file not found: $src"
    }
}

Write-Host "Done. Bochs rebuilt with /MD and installed to $InstallPrefix"
