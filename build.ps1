# Build the project: patched mod_settings.dll (vendor/mod_settings/) followed by
# gwheel.dll and diagnostic tools.
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

# Find vcvars64.bat if environment is not already initialized
function Init-VcEnvironment {
  if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vcvarsCandidates = @(
      "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
      "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
      "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
      "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
      "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    )
    foreach ($v in $vcvarsCandidates) {
      if (Test-Path $v) {
        Write-Host "Loading MSVC environment from $v..."
        cmd /c "call `"$v`" && set" | ForEach-Object {
          if ($_ -match '^(.*?)=(.*)$') {
            Set-Item -Force -Path "ENV:$($matches[1])" -Value "$($matches[2])"
          }
        }
        break
      }
    }
  }
}

Init-VcEnvironment

# ---------- submodule init -------------------------------------------------

if (-not (Test-Path "vendor\RED4ext.SDK\CMakeLists.txt")) {
  Write-Host "Initializing top-level submodules..."
  git submodule update --init --recursive
  if ($LASTEXITCODE -ne 0) { throw "git submodule update --init --recursive failed." }
}

# ---------- patched mod_settings.dll ----------------------------------------

$msBuildDir = "vendor\mod_settings\build"
$msDllCandidates = @(
  "vendor\mod_settings\build\Release\mod_settings.dll",
  "vendor\mod_settings\build\mod_settings.dll"
)
$msDll = $msDllCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $msDll -and (Test-Path "vendor\mod_settings\CMakeLists.txt")) {
  Write-Host "Building patched mod_settings.dll (vendor/mod_settings/)..."
  if (Get-Command ninja -ErrorAction SilentlyContinue) {
    & $cmake -S vendor/mod_settings -B $msBuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
  } else {
    & $cmake -S vendor/mod_settings -B $msBuildDir -A x64
  }
  if ($LASTEXITCODE -ne 0) { throw "mod_settings CMake configure failed." }
  & $cmake --build $msBuildDir --config Release
  if ($LASTEXITCODE -ne 0) { throw "mod_settings CMake build failed." }
}

# ---------- gwheel.dll & tools ---------------------------------------------

Write-Host "Configuring gwheel..."
if (Get-Command ninja -ErrorAction SilentlyContinue) {
  & $cmake -S . -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
} else {
  & $cmake -S . -B $BuildDir -A x64
}
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "Building gwheel and tools..."
& $cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

$dll = Join-Path $BuildDir "gwheel\gwheel.dll"
if (-not (Test-Path $dll)) {
  $dll = Join-Path $BuildDir "gwheel\$Config\gwheel.dll"
}

if (Test-Path $dll) {
  Write-Host ""
  Write-Host "Built: $dll"
  Write-Host ""
  Write-Host "gwheel.dll is ready to deploy to Cyberpunk 2077/red4ext/plugins/gwheel/"
} else {
  throw "Build succeeded but gwheel.dll was not found at $dll"
}
