# Install (or restore) the PSVR2/SteamVR GTAVXR bridge into an installed GTAVXR 1.0.2 Suite.
#   powershell -ExecutionPolicy Bypass -File install-psvr2-bridge.ps1 [-DryRun] [-Restore] [-SuiteData <folder>]
# The Suite checks runtime\gtavxr-1-0-2\gtavxr_native_openxr.dll against the hash recorded in
# versions\gtavxr-1-0-2.json before every Play, so both are updated together. The official DLL and record are backed
# up once to <Suite data>\psvr2-bridge-backup and -Restore puts them back. If <game>\CheekyFoveatedDLSS exists, the
# GTAVXR CheekyOpenXRLayer.dll shim is placed there as well (optional CheekyFoveatedDLSS integration).
# Close GTA and the Suite first. No administrator rights are needed.
param([switch]$Restore, [switch]$DryRun, [string]$SuiteData)
$ErrorActionPreference = 'Stop'
$Version = 'gtavxr-1-0-2'
$Dll = 'gtavxr_native_openxr.dll'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Say($m) { Write-Host $m }
function Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToUpperInvariant() }

if (-not $SuiteData) {
    $pointer = Join-Path $env:LOCALAPPDATA 'GTAVXR\suite-location.json'
    if (Test-Path -LiteralPath $pointer) { $SuiteData = (Get-Content -Raw -LiteralPath $pointer | ConvertFrom-Json).path }
    if (-not $SuiteData) { $SuiteData = Join-Path $env:LOCALAPPDATA 'GTAVXR\Suite' }
}
$manifest = Join-Path $SuiteData "versions\$Version.json"
$target = Join-Path $SuiteData "runtime\$Version\$Dll"
$backup = Join-Path $SuiteData 'psvr2-bridge-backup'
if (-not (Test-Path -LiteralPath $manifest) -or -not (Test-Path -LiteralPath $target)) {
    throw "GTAVXR 1.0.2 is not installed in '$SuiteData' (missing versions\$Version.json or the runtime folder). Install GTAVXR first, or pass -SuiteData."
}
# Exited leftovers of an elevated run have no threads; only live processes block.
$live = Get-Process -Name GTA5, GTAVXR_SourceBaseline, PlayGTAV, GTAVXR -ErrorAction SilentlyContinue | Where-Object { $_.Threads.Count -gt 0 }
if ($live) { throw "Close these first: $(($live | ForEach-Object ProcessName | Sort-Object -Unique) -join ', ')" }

if ($Restore) {
    if (-not (Test-Path -LiteralPath (Join-Path $backup $Dll))) { throw "No backup in $backup; nothing to restore." }
    Say "Restore official bridge and record from $backup"
    if (-not $DryRun) {
        Copy-Item -LiteralPath (Join-Path $backup $Dll) -Destination $target -Force
        Copy-Item -LiteralPath (Join-Path $backup "$Version.json") -Destination $manifest -Force
    }
    Say "Done. Bridge: $(Sha $target)"; exit 0
}

$source = Join-Path $here $Dll
if (-not (Test-Path -LiteralPath $source)) { throw "$Dll not found beside this script." }
$text = [IO.File]::ReadAllText($manifest)
$pattern = '("path"\s*:\s*"gtavxr_native_openxr\.dll"\s*,\s*"sha256"\s*:\s*")[0-9A-Fa-f]{64}("\s*,\s*"bytes"\s*:\s*)\d+'
if (-not [regex]::IsMatch($text, $pattern)) { throw "Unexpected version record format; $Dll entry not found in $manifest." }
$hash = Sha $source; $bytes = (Get-Item -LiteralPath $source).Length
Say "Suite data : $SuiteData"
Say "Bridge     : $hash ($bytes bytes)"
if (-not (Test-Path -LiteralPath (Join-Path $backup $Dll))) {
    Say "Backing up the current bridge and record to $backup (first install only)"
    if (-not $DryRun) {
        New-Item -ItemType Directory -Force -Path $backup | Out-Null
        Copy-Item -LiteralPath $target -Destination (Join-Path $backup $Dll)
        Copy-Item -LiteralPath $manifest -Destination (Join-Path $backup "$Version.json")
    }
}
$updated = [regex]::Replace($text, $pattern, { param($m) $m.Groups[1].Value + $hash + $m.Groups[2].Value + $bytes })
if (-not $DryRun) {
    Copy-Item -LiteralPath $source -Destination $target -Force
    $tmp = "$manifest.tmp"; [IO.File]::WriteAllText($tmp, $updated, (New-Object Text.UTF8Encoding($false)))
    Move-Item -LiteralPath $tmp -Destination $manifest -Force
    if ((Sha $target) -ne $hash) { throw "Copy verification failed for $target" }
}

# Optional: CheekyFoveatedDLSS shim beside a Cheeky standalone host in the game folder.
$settings = Join-Path $SuiteData 'settings.json'
$game = if (Test-Path -LiteralPath $settings) { (Get-Content -Raw -LiteralPath $settings | ConvertFrom-Json).game_dir }
$shim = Join-Path $here 'CheekyOpenXRLayer.dll'
if ($game -and (Test-Path -LiteralPath (Join-Path $game 'CheekyFoveatedDLSS\CheekyFoveatedDLSSHost.dll')) -and (Test-Path -LiteralPath $shim)) {
    $dest = Join-Path $game 'CheekyFoveatedDLSS\CheekyOpenXRLayer.dll'
    Say "Cheeky     : placing GTAVXR's CheekyOpenXRLayer.dll shim in $(Split-Path $dest)"
    if (-not $DryRun) {
        if ((Test-Path -LiteralPath $dest) -and (Sha $dest) -ne (Sha $shim)) {
            Set-ItemProperty -LiteralPath $dest -Name IsReadOnly -Value $false
            Copy-Item -LiteralPath $dest -Destination "$dest.bak" -Force
        }
        Copy-Item -LiteralPath $shim -Destination $dest -Force
    }
} else { Say "Cheeky     : not installed in the game folder (optional, skipped)" }
Say $(if ($DryRun) { 'Dry run: nothing changed.' } else { 'Installed. Start SteamVR, then the Suite, then Play.' })
