<#
.SYNOPSIS
  Package Gran Theft Praline for Windows into Builds\<version>\GranTheftPraline\.

.DESCRIPTION
  Windows counterpart of tools/build/package-mac.sh. Runs BuildCookRun for
  Win64 under a watchdog, moves the staged game into Builds\<version>\ and
  writes build-info.txt next to it.

  Usage: powershell -ExecutionPolicy Bypass -File tools\build\package-win.ps1 [version] [configuration] [-DryRun]
    version        default: ProjectVersion from Config\DefaultGame.ini, as major.minor (0.1.0 -> 0.1)
    configuration  Shipping (default), Development or Test
    -DryRun        resolve paths and print the BuildCookRun command, run nothing

  Environment:
    UE_ROOT        Unreal Engine install (default: C:\Program Files\Epic Games\UE_5.8)
    BUILDS_DIR     output root (default: <repo>\Builds)
    STALL_MINUTES  kill BuildCookRun when its log has not grown for this long (default 25)
    MAX_MINUTES    hard limit for one BuildCookRun attempt (default 240)
    RETRIES        new attempts after a stall or time-out (default 1; the cook
                   is incremental, so a retry resumes most of the work)
    VERSION_TAG    release tag (v0.9.0-rc1): sets ProjectVersion before the build
                   (tools\qa\stamp-version.ps1), as package-mac.sh does

  Logs: Builds\<version>\package.log
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0)][string]$Version = '',
  [Parameter(Position = 1)][string]$Configuration = 'Shipping',
  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
  [Console]::Error.WriteLine("package-win: $Message")
  exit 1
}

function EnvOr([string]$Name, [string]$Default) {
  $value = [Environment]::GetEnvironmentVariable($Name)
  if ([string]::IsNullOrEmpty($value)) { return $Default }
  return $value
}

function IniValue([string]$Path, [string]$Key) {
  if (-not (Test-Path -LiteralPath $Path)) { return '' }
  foreach ($line in [IO.File]::ReadAllLines($Path)) {
    if ($line.StartsWith("$Key=")) { return $line.Substring($Key.Length + 1).Trim() }
  }
  return ''
}

# Quote one argument for a cmd.exe command line.
function Quote([string]$Arg) {
  if ($Arg -match '[\s"]') { return '"' + ($Arg -replace '"', '\"') + '"' }
  return $Arg
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$projectDir = Join-Path $repoRoot 'GranTheftPraline'
$projectFile = Join-Path $projectDir 'GranTheftPraline.uproject'
$ueRoot = EnvOr 'UE_ROOT' 'C:\Program Files\Epic Games\UE_5.8'
$runUat = Join-Path $ueRoot 'Engine\Build\BatchFiles\RunUAT.bat'

if (-not (Test-Path -LiteralPath $projectFile)) { Fail "project not found: $projectFile" }
$engineFound = Test-Path -LiteralPath $runUat
if (-not $engineFound -and -not $DryRun) { Fail "RunUAT not found: $runUat (set UE_ROOT)" }

# VERSION_TAG (e.g. v0.9.0-rc1) stamps ProjectVersion from the release tag.
$versionTag = EnvOr 'VERSION_TAG' ''
if ($versionTag -ne '') {
  $stamp = Join-Path $repoRoot 'tools\qa\stamp-version.ps1'
  if (-not (Test-Path -LiteralPath $stamp)) { Fail "VERSION_TAG set but $stamp is missing" }
  if ($DryRun) {
    Write-Output "Dry run: would stamp ProjectVersion from $versionTag ($stamp)"
  } else {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $stamp $versionTag
    if ($LASTEXITCODE -ne 0) { Fail "cannot stamp version from $versionTag" }
  }
}

$projectVersion = IniValue (Join-Path $projectDir 'Config\DefaultGame.ini') 'ProjectVersion'
if ($Version -eq '') { $Version = (($projectVersion -split '\.') | Select-Object -First 2) -join '.' }
if ($Version -eq '') { Fail 'no version given and no ProjectVersion in DefaultGame.ini' }
if (@('Shipping', 'Development', 'Test') -notcontains $Configuration) { Fail "unknown configuration '$Configuration'" }

$buildsDir = EnvOr 'BUILDS_DIR' (Join-Path $repoRoot 'Builds')
$outDir = Join-Path $buildsDir $Version
$stagingDir = Join-Path $outDir '.staging'
$gameDir = Join-Path $outDir 'GranTheftPraline'
$logFile = Join-Path $outDir 'package.log'

$stallMinutes = [int](EnvOr 'STALL_MINUTES' '25')
$maxMinutes = [int](EnvOr 'MAX_MINUTES' '240')
$retries = [int](EnvOr 'RETRIES' '1')

# The editor target is built too (no -nocompileeditor): the cook loads the
# project module from the editor binaries, and a stale one silently drops new
# classes and their assets.
$uatArgs = @(
  'BuildCookRun',
  "-project=$projectFile",
  '-platform=Win64',
  "-clientconfig=$Configuration",
  '-build', '-cook', '-stage', '-pak', '-iostore', '-package', '-archive',
  "-archivedirectory=$stagingDir",
  '-compressed', '-prereqs', '-nop4', '-utf8output', '-unattended',
  # another UnrealBuildTool may be running (compile check): wait for it instead of failing
  '-ubtargs=-WaitMutex'
)
$cmdLine = ((@($runUat) + $uatArgs) | ForEach-Object { Quote $_ }) -join ' '

# Levels opened only by name at run time (interiors) are not cooked by
# DirectoriesToAlwaysCook, which skips maps: list them in MapsToCook for this
# package only. INTERIORS=0 skips it; COOK_MAPS adds /Game/... paths (comma list).
# DefaultGame.ini is put back right after the cook.
$gameIni = Join-Path $projectDir 'Config\DefaultGame.ini'
$iniBackup = $null
$cookMaps = @()
if ((EnvOr 'INTERIORS' '1') -eq '1') {
  $interiorDir = Join-Path $projectDir 'Content\Interiors'
  if (Test-Path -LiteralPath $interiorDir) {
    $cookMaps += Get-ChildItem -LiteralPath $interiorDir -Filter 'L_Interior_*.umap' -File |
      ForEach-Object { "/Game/Interiors/$($_.BaseName)" }
  }
}
$extraMaps = EnvOr 'COOK_MAPS' ''
if ($extraMaps -ne '') { $cookMaps += ($extraMaps -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }) }

function RestoreIni {
  if ($null -ne $script:iniBackup) {
    [IO.File]::WriteAllText($gameIni, $script:iniBackup, (New-Object Text.UTF8Encoding($false)))
    $script:iniBackup = $null
  }
}

Write-Output "Packaging version $Version ($Configuration) into $outDir"
Write-Output "Engine: $ueRoot"
Write-Output "Full log: $logFile (watchdog: $stallMinutes min without output, $maxMinutes min max, $retries retry)"

if ($DryRun) {
  if (-not $engineFound) { Write-Output "Warning: RunUAT not found: $runUat (set UE_ROOT)" }
  Write-Output "Dry run, command not started:"
  Write-Output "  cmd.exe /d /s /c `"$cmdLine`""
  Write-Output "Maps added to MapsToCook: $(if ($cookMaps.Count) { $cookMaps -join ', ' } else { 'none' })"
  Write-Output "Game folder would be: $gameDir"
  exit 0
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
if (Test-Path -LiteralPath $stagingDir) { Remove-Item -LiteralPath $stagingDir -Recurse -Force }

function FileSize([string]$Path) {
  if (Test-Path -LiteralPath $Path) { return (Get-Item -LiteralPath $Path).Length }
  return 0
}

function StopTree([int]$ProcessId) {
  & taskkill.exe /T /F /PID $ProcessId 2>&1 | Out-Null
}

# One BuildCookRun attempt under a watchdog.
# Returns 0 on success, 1 on failure, 2 on a stall or time-out.
function RunBuildCook([string]$AttemptLog) {
  $errLog = "$AttemptLog.err"
  $proc = Start-Process -FilePath 'cmd.exe' -ArgumentList "/d /s /c `"$cmdLine`"" `
    -RedirectStandardOutput $AttemptLog -RedirectStandardError $errLog `
    -NoNewWindow -PassThru
  $null = $proc.Handle  # keep the handle so ExitCode stays readable
  $started = Get-Date
  $lastChange = $started
  $lastSize = -1
  $result = $null
  while (-not $proc.HasExited) {
    Start-Sleep -Seconds 30
    $now = Get-Date
    $size = (FileSize $AttemptLog) + (FileSize $errLog)
    if ($size -ne $lastSize) { $lastSize = $size; $lastChange = $now }
    if (($now - $lastChange).TotalMinutes -ge $stallMinutes) {
      $msg = "package-win: no output for $stallMinutes min, stopping BuildCookRun"
      $result = 2
    } elseif (($now - $started).TotalMinutes -ge $maxMinutes) {
      $msg = "package-win: BuildCookRun over $maxMinutes min, stopping it"
      $result = 2
    }
    if ($result -eq 2) {
      [Console]::Error.WriteLine($msg)
      StopTree $proc.Id
      $proc.WaitForExit()
      Add-Content -LiteralPath $errLog -Value $msg
      break
    }
  }
  if ($null -eq $result) {
    $proc.WaitForExit()
    if ($proc.ExitCode -eq 0) { $result = 0 } else { $result = 1 }
  }
  if (Test-Path -LiteralPath $errLog) {
    Get-Content -LiteralPath $errLog | Add-Content -LiteralPath $AttemptLog
    Remove-Item -LiteralPath $errLog -Force
  }
  return $result
}

if ($cookMaps.Count -gt 0) {
  $iniText = [IO.File]::ReadAllText($gameIni)
  $key = "[/Script/UnrealEd.ProjectPackagingSettings]"
  $at = $iniText.IndexOf($key)
  if ($at -lt 0) { Fail "no $key section in DefaultGame.ini" }
  $iniBackup = $iniText
  $lines = ($cookMaps | ForEach-Object { "+MapsToCook=(FilePath=`"$_`")" }) -join "`n"
  # keep the line ending style: insert right after the section header line
  $eol = $iniText.IndexOf("`n", $at)
  $newText = $iniText.Substring(0, $eol + 1) + $lines + "`n" + $iniText.Substring($eol + 1)
  [IO.File]::WriteAllText($gameIni, $newText, (New-Object Text.UTF8Encoding($false)))
  Write-Output "MapsToCook added for this package: $($cookMaps -join ', ')"
}
$startTime = Get-Date
Set-Content -LiteralPath $logFile -Value $null
if ($cookMaps.Count -gt 0) { Add-Content -LiteralPath $logFile -Value "package-win: MapsToCook added for this package: $($cookMaps -join ', ')" }
$attempt = 0
while ($true) {
  $attempt++
  $attemptLog = Join-Path $outDir "package-attempt$attempt.log"
  if (Test-Path -LiteralPath $stagingDir) { Remove-Item -LiteralPath $stagingDir -Recurse -Force }
  $status = RunBuildCook $attemptLog
  if (Test-Path -LiteralPath $attemptLog) {
    Get-Content -LiteralPath $attemptLog | Add-Content -LiteralPath $logFile
    Remove-Item -LiteralPath $attemptLog -Force
  }
  if ($status -eq 0) { break }
  if ($status -eq 2 -and $attempt -le $retries) {
    [Console]::Error.WriteLine("Attempt $attempt stalled; retrying (the cook resumes from what is done).")
    continue
  }
  [Console]::Error.WriteLine("BuildCookRun failed (attempt $attempt). Last errors:")
  Select-String -LiteralPath $logFile -Pattern 'Error|error:' | Select-Object -Last 30 |
    ForEach-Object { [Console]::Error.WriteLine($_.Line) }
  RestoreIni
  Fail "see $logFile"
}
RestoreIni

# BuildCookRun archives into <archivedirectory>\Windows\ (older engines: WindowsNoEditor).
$builtExe = Get-ChildItem -LiteralPath $stagingDir -Filter 'GranTheftPraline*.exe' -Recurse -Depth 2 -ErrorAction SilentlyContinue |
  Select-Object -First 1
if ($null -eq $builtExe) { Fail "no game executable found under $stagingDir" }
$builtDir = $builtExe.Directory.FullName

if (Test-Path -LiteralPath $gameDir) { Remove-Item -LiteralPath $gameDir -Recurse -Force }
Move-Item -LiteralPath $builtDir -Destination $gameDir
Remove-Item -LiteralPath $stagingDir -Recurse -Force

$elapsed = [int]((Get-Date) - $startTime).TotalSeconds
$bytes = (Get-ChildItem -LiteralPath $gameDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
$size = ($bytes / 1GB).ToString('0.0', [Globalization.CultureInfo]::InvariantCulture) + 'G'
$commit = (& git -C $repoRoot rev-parse --short HEAD 2>$null)
if (-not $commit) { $commit = 'unknown' }

$info = @(
  "version=$Version",
  "configuration=$Configuration",
  "commit=$commit",
  "attempts=$attempt",
  "built=$((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))",
  "duration_s=$elapsed",
  "size=$size"
)
[IO.File]::WriteAllLines((Join-Path $outDir 'build-info.txt'), $info)

Write-Output "Done in ${elapsed}s: $gameDir\$($builtExe.Name) ($size, commit $commit)"
