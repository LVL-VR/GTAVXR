# Turn Virtual Desktop's implicit OpenXR layer (XR_APILAYER_VIRTUALDESKTOP_oculus_compatibility) on or off system-wide.
#   powershell -File vd-openxr-layer.ps1 status | on | off
# The OpenXR loader skips an implicit layer whose registry value is non-zero (0 = enabled, 1 = disabled).
# Changing it needs admin; the script asks for elevation itself. Restart the OpenXR app afterwards.
param([ValidateSet('status','on','off')][string]$Action = 'status')
$ErrorActionPreference = 'Stop'
$key = 'HKLM:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit'
$manifest = 'C:\Program Files\Virtual Desktop Streamer\openxr-oculus-compatibility.json'

function Show {
    $v = (Get-ItemProperty -Path $key -Name $manifest -ErrorAction SilentlyContinue).$manifest
    if ($null -eq $v) { 'Virtual Desktop OpenXR layer: not registered' }
    elseif ($v -eq 0) { 'Virtual Desktop OpenXR layer: ON' }
    else { 'Virtual Desktop OpenXR layer: OFF' }
}

if ($Action -eq 'status') { Show; exit 0 }
if ($null -eq (Get-ItemProperty -Path $key -Name $manifest -ErrorAction SilentlyContinue)) { Show; exit 1 }

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    $p = Start-Process powershell -Verb RunAs -Wait -PassThru -WindowStyle Hidden `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", $Action)
    Show; exit $p.ExitCode
}
Set-ItemProperty -Path $key -Name $manifest -Value ([int]($Action -eq 'off')) -Type DWord
Show
