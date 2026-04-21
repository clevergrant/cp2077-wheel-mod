# Build gwheel.dll via CMake + MSVC.
#
# Requires:
#   - Visual Studio 2022 with the "Desktop development with C++" workload
#   - CMake 3.21+ on PATH, OR the "C++ CMake tools for Windows" VS workload
#     (which bundles a cmake.exe under the VS install dir)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File build.ps1
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Config Debug

param(
  [ValidateSet("Debug", "Release", "RelWithDebInfo")]
  [string]$Config = "Release",

  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

function Resolve-CMake {
  $candidates = @(
    (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source,
    "${Env:ProgramFiles}\CMake\bin\cmake.exe",
    "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )
  foreach ($c in $candidates) {
    if ($c -and (Test-Path $c)) { return $c }
  }
  throw "CMake not found. Install CMake 3.21+ or add the 'C++ CMake tools for Windows' VS workload."
}

$cmake = Resolve-CMake
Write-Host "Using CMake: $cmake"

# Ensure the RED4ext.SDK submodule is present.
if (-not (Test-Path "gwheel\vendor\RED4ext.SDK\CMakeLists.txt")) {
  Write-Host "Initializing RED4ext.SDK submodule..."
  git submodule update --init --recursive
}

# Configure.
& $cmake -S . -B $BuildDir -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

# Build.
& $cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

$dll = Join-Path $BuildDir "gwheel\$Config\gwheel.dll"
if (Test-Path $dll) {
  Write-Host ""
  Write-Host "Built: $dll"
  Write-Host ""
  Write-Host "To install into a Cyberpunk 2077 directory, copy:"
  Write-Host "  $dll  ->  <CP2077>\red4ext\plugins\gwheel\gwheel.dll"
  Write-Host "  gwheel_reds\*.reds  ->  <CP2077>\r6\scripts\gwheel\"
} else {
  throw "Build succeeded but gwheel.dll was not found at $dll"
}
