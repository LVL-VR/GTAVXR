param(
    [Parameter(Mandatory=$true)][string]$OpenXRInclude,
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$Output,
    [switch]$ObsCapture,
    [string]$HudArt
)
# Run from an x64 Visual Studio developer shell.
$ErrorActionPreference = 'Stop'
$include = (Resolve-Path $OpenXRInclude).Path
$runtimeDir = (Resolve-Path $Runtime).Path
$api = (Resolve-Path "$runtimeDir/scripthost/ScriptHookVDotNet3.dll").Path
$loader = (Resolve-Path "$runtimeDir/openxr_loader.dll").Path
if ($HudArt) { $pack = (Resolve-Path $HudArt).Path }
$root = (New-Item -ItemType Directory -Force $Output).FullName
$layer = (New-Item -ItemType Directory -Force "$root/layer").FullName
$scripts = (New-Item -ItemType Directory -Force "$root/package/scripts").FullName
$build = (New-Item -ItemType Directory -Force "$root/obj").FullName
$csc = "$env:WINDIR/Microsoft.NET/Framework64/v4.0.30319/csc.exe"
$flags = @('/nologo','/EHsc','/std:c++17','/MT',"/I$include")
$resources = @()
$priorLayerPath = $env:XR_API_LAYER_PATH
Push-Location $build
try {
    if ($HudArt) {
        Copy-Item -LiteralPath $pack -Destination 'local.icons' -Force
        '101 RCDATA "local.icons"' | Set-Content local.rc -Encoding Ascii
        & rc /nologo local.rc
        if ($LASTEXITCODE) { throw 'Could not build the local artwork resource.' }
        $resources = @('local.res')
        $flags += '/DWITH_HUD_ART'
    }
    & cl @flags /O2 /LD "$PSScriptRoot\layer.cpp" /link @resources user32.lib "/OUT:$layer/QuestFourSlotsXR.dll" /IMPLIB:layer.lib
    if ($LASTEXITCODE) { throw 'OpenXR layer build failed.' }
    Copy-Item "$PSScriptRoot\QuestFourSlots.json" $layer -Force
    if ($ObsCapture) {
        $mirrorDir = Join-Path $env:ProgramFiles 'OpenXR-OBSMirror'
        $mirrorName = 'XR_APILAYER_NOVENDOR_OBSMirror'
        $manifest = Get-Content "$mirrorDir/$mirrorName.json" -Raw | ConvertFrom-Json
        $manifest.api_layer.library_path = (Resolve-Path "$mirrorDir/$mirrorName.dll").Path
        $manifest.api_layer.PSObject.Properties.Remove('disable_environment')
        $manifest | ConvertTo-Json -Depth 8 | Set-Content "$layer/OBSMirror.json" -Encoding ascii
    } elseif (Test-Path "$layer/OBSMirror.json") {
        Remove-Item -LiteralPath "$layer/OBSMirror.json"
    }
    & cl @flags "$PSScriptRoot\check.cpp" /Fe:check.exe /link @resources user32.lib
    if ($LASTEXITCODE) { throw 'Test build failed.' }
    $env:XR_API_LAYER_PATH = $layer
    & .\check.exe $loader
    if ($LASTEXITCODE) { throw 'Holster checks failed.' }
    & $csc /nologo /target:library /platform:x64 "/reference:$api" "/out:$scripts\QuestFourSlots.3.dll" "$PSScriptRoot\Inventory.cs" "$PSScriptRoot\QueuedDraw.cs" "$PSScriptRoot\Handoff.cs"
    if ($LASTEXITCODE) { throw 'Suite script build failed.' }
    Compress-Archive -LiteralPath "$root/package/scripts" -DestinationPath "$root/QuestFourSlots.zip" -Force
} finally {
    $env:XR_API_LAYER_PATH = $priorLayerPath
    Pop-Location
}
