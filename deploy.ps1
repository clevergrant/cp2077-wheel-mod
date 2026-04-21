# Package or deploy gwheel for testing.
#
# Two modes:
#
#   Zip mode (default):
#     Produces dist/gwheel-<version>.zip laid out the way the FOMOD expects.
#     Drop that zip on Vortex via File -> Install from file.
#     Example:
#       powershell -ExecutionPolicy Bypass -File deploy.ps1
#
#   Direct mode (-Game <path>):
#     Copies files straight into a Cyberpunk 2077 install directory. Fastest
#     dev loop — no Vortex, no re-zip, no FOMOD prompts.
#     Example:
#       powershell -ExecutionPolicy Bypass -File deploy.ps1 -Game "C:\GOG Games\Cyberpunk 2077"
#
# Common flags:
#   -Config <Debug|Release|RelWithDebInfo>   Which build config to package.
#   -BuildDir <path>                          Where gwheel.dll lives (default "build").
#   -Clean                                    Wipe prior artifacts before deploying.
#   -NoBuild                                  Skip invoking build.ps1 even if the DLL is stale.

param(
  [string]$Game,
  [ValidateSet("Debug", "Release", "RelWithDebInfo")]
  [string]$Config = "Release",
  [string]$BuildDir = "build",
  [switch]$Clean,
  [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot
if (-not $repoRoot) { $repoRoot = (Get-Location).Path }
Set-Location $repoRoot

function Info($msg)    { Write-Host "[deploy] $msg" -ForegroundColor Cyan }
function Ok($msg)      { Write-Host "[deploy] $msg" -ForegroundColor Green }
function Warn($msg)    { Write-Host "[deploy] WARN: $msg" -ForegroundColor Yellow }
function Fail($msg)    {
  Write-Host "[deploy] ERROR: $msg" -ForegroundColor Red
  exit 1
}

# ---------- version ---------------------------------------------------------

$version = "0.1.0"
try {
  $modInfo = Get-Content -Raw "mod_info.json" | ConvertFrom-Json
  if ($modInfo.version) { $version = $modInfo.version }
} catch {
  Warn "Could not read mod_info.json; defaulting version to $version"
}
Info "Version: $version"

# ---------- pre-flight ------------------------------------------------------

$redsFiles = @(
  "gwheel_reds\gwheel_natives.reds",
  "gwheel_reds\gwheel_vehicle_override.reds",
  "gwheel_reds\gwheel_settings.reds"
)
foreach ($r in $redsFiles) {
  if (-not (Test-Path $r)) { Fail "Missing redscript source: $r" }
}

$fomodFiles = @(
  "fomod\info.xml",
  "fomod\ModuleConfig.xml"
)
foreach ($f in $fomodFiles) {
  if (-not (Test-Path $f)) { Fail "Missing FOMOD file: $f" }
}

# ---------- build (if needed) ----------------------------------------------

$dllPath = Join-Path $BuildDir "gwheel\$Config\gwheel.dll"

function Invoke-Build {
  if (-not (Test-Path "build.ps1")) { Fail "build.ps1 missing — can't build." }
  Info "Invoking build.ps1 -Config $Config -BuildDir $BuildDir"
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "build.ps1" -Config $Config -BuildDir $BuildDir
  if ($LASTEXITCODE -ne 0) { Fail "build.ps1 failed (exit $LASTEXITCODE). Fix compile errors before deploying." }
}

if (-not (Test-Path $dllPath)) {
  if ($NoBuild) { Fail "DLL not found at $dllPath and -NoBuild was specified." }
  Warn "DLL not found at $dllPath — running build first"
  Invoke-Build
  if (-not (Test-Path $dllPath)) { Fail "build.ps1 completed but $dllPath still missing." }
} elseif (-not $NoBuild) {
  # Rebuild if any source is newer than the DLL.
  $dllTime = (Get-Item $dllPath).LastWriteTime
  $srcTimes = Get-ChildItem -Recurse -Path "gwheel\src","gwheel\include","gwheel\CMakeLists.txt","CMakeLists.txt" -File |
    Select-Object -ExpandProperty LastWriteTime
  $newest = ($srcTimes | Measure-Object -Maximum).Maximum
  if ($newest -gt $dllTime) {
    Info "Sources newer than DLL — rebuilding"
    Invoke-Build
  } else {
    Info "DLL is up to date; skipping build. Pass -Clean to force."
  }
}

$dllSize = (Get-Item $dllPath).Length
Info "Using DLL: $dllPath ($dllSize bytes)"

# ---------- clean -----------------------------------------------------------

if ($Clean) {
  if (Test-Path "dist")    { Remove-Item -Recurse -Force "dist";    Info "Removed dist/" }
  if (Test-Path "staging") { Remove-Item -Recurse -Force "staging"; Info "Removed staging/" }
}

# ============================================================================
# Direct mode: -Game <path>
# ============================================================================

if ($Game) {
  if (-not (Test-Path $Game)) { Fail "Game path does not exist: $Game" }

  # Sanity: the path should contain bin\x64\Cyberpunk2077.exe.
  $gameExe = Join-Path $Game "bin\x64\Cyberpunk2077.exe"
  if (-not (Test-Path $gameExe)) {
    Fail "Path doesn't look like a Cyberpunk 2077 install (no bin\x64\Cyberpunk2077.exe): $Game"
  }
  Info "Target game install: $Game"

  # Warn if game is running (DLL will be locked).
  $running = Get-Process -Name "Cyberpunk2077" -ErrorAction SilentlyContinue
  if ($running) { Fail "Cyberpunk 2077 is running — close it first (the DLL is locked while the game is open)." }

  # red4ext presence check.
  $red4ext = Join-Path $Game "red4ext\RED4ext.dll"
  if (-not (Test-Path $red4ext)) {
    Warn "RED4ext not detected at $red4ext. The plugin will not load without it."
  }

  # redscript presence check.
  $redscript = Join-Path $Game "engine\tools\scc.exe"
  if (-not (Test-Path $redscript)) {
    Warn "redscript not detected at $redscript. The .reds files will not compile without it."
  }

  # ArchiveXL + Mod Settings presence check (warn-only).
  $archiveXl = Join-Path $Game "red4ext\plugins\ArchiveXL\ArchiveXL.dll"
  if (-not (Test-Path $archiveXl)) {
    Warn "ArchiveXL not detected. Mod Settings depends on it, so the Settings page will not appear."
  }
  $modSettings = Join-Path $Game "r6\scripts\mod_settings\ModSettings.reds"
  if (-not (Test-Path $modSettings)) {
    Warn "Mod Settings not detected. The wheel will work but the in-game Settings page will not appear."
  }

  # Deploy.
  $pluginDir = Join-Path $Game "red4ext\plugins\gwheel"
  $scriptDir = Join-Path $Game "r6\scripts\gwheel"

  if ($Clean) {
    if (Test-Path $pluginDir) { Remove-Item -Recurse -Force $pluginDir; Info "Removed $pluginDir" }
    if (Test-Path $scriptDir) { Remove-Item -Recurse -Force $scriptDir; Info "Removed $scriptDir" }
  }

  New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
  New-Item -ItemType Directory -Force -Path $scriptDir | Out-Null

  Copy-Item -Force $dllPath (Join-Path $pluginDir "gwheel.dll")
  Info "Deployed DLL -> $(Join-Path $pluginDir 'gwheel.dll')"

  foreach ($r in $redsFiles) {
    $dest = Join-Path $scriptDir (Split-Path $r -Leaf)
    Copy-Item -Force $r $dest
    Info "Deployed reds -> $dest"
  }

  # Invalidate redscript cache so the new .reds files compile on next launch.
  $cache = Join-Path $Game "r6\cache\modded\final.redscripts"
  if (Test-Path $cache) {
    try {
      Remove-Item -Force $cache
      Info "Invalidated redscript cache: $cache (will recompile on next launch)"
    } catch {
      Warn "Could not invalidate redscript cache — next launch may use stale compiled script. File: $cache"
    }
  }

  Write-Host ""
  Ok "Deploy complete."
  Write-Host ""
  Write-Host "Next steps:" -ForegroundColor Cyan
  Write-Host "  1. Launch Cyberpunk 2077. First launch after a .reds change is slow (30-60s) due to recompile."
  Write-Host "  2. Check logs for load confirmation:"
  Write-Host "       $(Join-Path $Game 'red4ext\logs\gwheel-*.log')"
  Write-Host "     Look for:   [gwheel] loaded v$version"
  Write-Host "     And then:   [gwheel] device acquired: <Model> (axes=N buttons=M FFB=yes/no)"
  Write-Host "  3. Check redscript compile log (if the Settings page or vehicle hook breaks):"
  Write-Host "       $(Join-Path $Game 'r6\cache\modded\final.redscripts.log')"
  Write-Host "  4. In-game: Main Menu -> Settings -> Mod Settings -> G-series Wheel."
  exit 0
}

# ============================================================================
# Zip mode (default)
# ============================================================================

$stagingDir = Join-Path $repoRoot "staging"
$distDir    = Join-Path $repoRoot "dist"
$zipPath    = Join-Path $distDir "gwheel-$version.zip"

if (Test-Path $stagingDir) { Remove-Item -Recurse -Force $stagingDir }
New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null
New-Item -ItemType Directory -Force -Path $distDir    | Out-Null

# Layout for the FOMOD — source paths match what ModuleConfig.xml references.
New-Item -ItemType Directory -Force -Path (Join-Path $stagingDir "build")       | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $stagingDir "fomod")       | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $stagingDir "gwheel_reds") | Out-Null

Copy-Item -Force $dllPath                          (Join-Path $stagingDir "build\gwheel.dll")
Copy-Item -Force "fomod\info.xml"                  (Join-Path $stagingDir "fomod\info.xml")
Copy-Item -Force "fomod\ModuleConfig.xml"          (Join-Path $stagingDir "fomod\ModuleConfig.xml")
foreach ($r in $redsFiles) {
  Copy-Item -Force $r (Join-Path $stagingDir (Split-Path $r -Leaf | ForEach-Object { "gwheel_reds\$_" }))
}

# Include README + CHANGELOG as top-level files so Vortex shows them.
if (Test-Path "README.md")    { Copy-Item -Force "README.md"    $stagingDir }
if (Test-Path "CHANGELOG.md") { Copy-Item -Force "CHANGELOG.md" $stagingDir }

if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath -Force

$zipSize = [Math]::Round((Get-Item $zipPath).Length / 1024, 1)

Write-Host ""
Ok "Package ready: $zipPath ($zipSize KB)"
Write-Host ""
Write-Host "To test in-game:" -ForegroundColor Cyan
Write-Host "  Vortex -> File -> Install from file -> $zipPath"
Write-Host "  Deploy (Vortex auto-prompts), then launch the game."
Write-Host ""
Write-Host "For fast dev iteration, skip Vortex:" -ForegroundColor Cyan
Write-Host "  .\deploy.ps1 -Game `"<path-to-cyberpunk-install>`""
