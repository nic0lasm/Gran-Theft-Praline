<#
  Set ProjectVersion in DefaultGame.ini from a release tag (Windows twin of
  stamp_version.py, for machines without Python).
  Usage: powershell -ExecutionPolicy Bypass -File tools\qa\stamp-version.ps1 v0.9.0-rc1 [-Check]
  Prints the numeric version (X.Y.Z); exit 1 with -Check when the ini differs.
#>
param(
  [Parameter(Mandatory = $true, Position = 0)][string]$Tag,
  [switch]$Check
)
$ErrorActionPreference = 'Stop'
if ($Tag -notmatch '^v?(\d+)\.(\d+)(?:\.(\d+))?(?:-(rc\.?\d+))?$') {
  Write-Error "not a release tag: $Tag (expected vX.Y.Z or vX.Y.Z-rcN)"; exit 2
}
$patch = if ($Matches[3]) { $Matches[3] } else { '0' }
$version = "$($Matches[1]).$($Matches[2]).$patch"
$ini = Join-Path $PSScriptRoot '..\..\GranTheftPraline\Config\DefaultGame.ini'
$text = [IO.File]::ReadAllText($ini)
$section = '[/Script/EngineSettings.GeneralProjectSettings]'
$start = $text.IndexOf($section)
if ($start -lt 0) { Write-Error "no $section in $ini"; exit 2 }
$rx = [regex]'(?m)^ProjectVersion=(.*?)(\r?)$'
$m = $rx.Match($text, $start)
if (-not $m.Success) { Write-Error 'no ProjectVersion key'; exit 2 }
$current = $m.Groups[1].Value
if ($Check) {
  if ($current -ne $version) { Write-Error "ProjectVersion is $current, tag says $version"; exit 1 }
} elseif ($current -ne $version) {
  $text = $text.Substring(0, $m.Index) + "ProjectVersion=$version" + $m.Groups[2].Value + $text.Substring($m.Index + $m.Length)
  [IO.File]::WriteAllText($ini, $text, (New-Object Text.UTF8Encoding($false)))
  Write-Host "ProjectVersion $current -> $version" -ForegroundColor DarkGray
}
Write-Output $version
