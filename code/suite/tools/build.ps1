param([Parameter(Mandatory=$true)][string]$Output,[switch]$Probe,
      [string]$DistributionBundle,[string]$DistributionSha256)
# Suite 2 build: in-box .NET Framework 4.x csc (C# 5) + WPF. Fresh output folder only; never overwrites a previous build.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$fixtureBuild=Test-Path -LiteralPath (Join-Path $root 'src/Content/FixtureContentProvider.cs')
if($fixtureBuild -and ($DistributionBundle -or $DistributionSha256)){throw 'The fixture build cannot embed a release bundle'}
$bundleHash=$null
if($DistributionBundle -or $DistributionSha256){
    if(!$DistributionBundle -or $DistributionSha256 -notmatch '^[A-Fa-f0-9]{64}$'){throw 'A bundle and its SHA-256 are both required'}
    $DistributionBundle=(Resolve-Path -LiteralPath $DistributionBundle).ProviderPath
    $sourceRoot=[IO.Path]::GetFullPath($root).TrimEnd('\')+'\'
    if($DistributionBundle.StartsWith($sourceRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'The private bundle must be outside the source tree'}
    $bundleHash=(Get-FileHash -LiteralPath $DistributionBundle).Hash
    if($bundleHash -ne $DistributionSha256){throw 'Distribution bundle identity mismatch'}
}
$Output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)
if(Test-Path -LiteralPath $Output){throw 'Build output must be fresh'}
[void][IO.Directory]::CreateDirectory($Output)
$fw='C:/Windows/Microsoft.NET/Framework64/v4.0.30319'
$compiler="$fw/csc.exe"
$wpf="$fw/WPF"
$sources=@(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Recurse -Filter '*.cs' -File | Sort-Object FullName | ForEach-Object FullName)
$refs=@('System.dll','System.Core.dll','System.Drawing.dll','System.Xml.dll','System.Xml.Linq.dll','System.Web.Extensions.dll',
 'System.IO.Compression.dll','System.IO.Compression.FileSystem.dll','System.Windows.Forms.dll','System.Xaml.dll','Microsoft.CSharp.dll',
 "$wpf/PresentationCore.dll","$wpf/PresentationFramework.dll","$wpf/WindowsBase.dll")
$target=Join-Path $Output 'GTAVXR.exe'
$cargs=@('/nologo','/target:winexe','/platform:x64','/optimize+','/debug-','/warn:4','/warnaserror+','/nowarn:1591',("/out:$target"),
 ('/win32manifest:'+(Join-Path $root 'resources/app.manifest')),('/win32icon:'+(Join-Path $root 'resources/gtavxr.ico')))
if($fixtureBuild){$cargs+='/define:SUITE_FIXTURES'}
foreach($r in $refs){$cargs+=('/reference:'+$r)}
foreach($res in Get-ChildItem -LiteralPath (Join-Path $root 'resources/embedded') -File -ErrorAction SilentlyContinue | Sort-Object Name){$cargs+=('/resource:'+$res.FullName+',GtavxrSuite.'+$res.Name)}
foreach($recipe in Get-ChildItem -LiteralPath (Join-Path $root 'resources/recipes') -Filter '*.json' -File | Sort-Object Name){$cargs+=('/resource:'+$recipe.FullName+',GtavxrSuite.Recipe.'+$recipe.Name)}
if($bundleHash){$cargs+=('/resource:'+$DistributionBundle+',GtavxrSuite.Distribution.Bundle')}
if(-not (Test-Path (Join-Path $root 'resources/gtavxr.ico'))){$cargs=@($cargs|Where-Object {$_ -notlike '/win32icon:*'})}
$cargs+=$sources
$log=& $compiler @cargs 2>&1
$exit=$LASTEXITCODE
$log|Set-Content -LiteralPath (Join-Path $Output 'compile.log') -Encoding utf8
if($exit -ne 0){$log|Select-Object -First 60|ForEach-Object {Write-Host $_};throw "csc failed ($exit)"}
if($bundleHash -and (Get-FileHash -LiteralPath $DistributionBundle).Hash -ne $bundleHash){throw 'Distribution bundle changed during build'}
$src=@(foreach($s in $sources){@{path=$s.Substring($root.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $s).Hash}})
@{schema='gtavxr-suite2-build-v1';exe=@{sha256=(Get-FileHash -LiteralPath $target).Hash;bytes=(Get-Item $target).Length};sources=$src;distribution_sha256=$bundleHash;utc=[DateTime]::UtcNow.ToString('o')}|
 ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $Output 'build.json') -Encoding utf8
Write-Host "PASS build $target"
