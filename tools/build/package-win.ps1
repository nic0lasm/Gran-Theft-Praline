<#
.SYNOPSIS
  Package Gran Theft Praline for Windows into Builds\<version>\GranTheftPraline\.

.DESCRIPTION
  Windows counterpart of tools/build/package-mac.sh. Runs BuildCookRun for
  Win64 under a watchdog, moves the staged game into Builds\<version>\ and
  writes build-info.txt next to it.

  Usage: powershell -ExecutionPolicy Bypass -File tools\build\package-win.ps1 [version] [configuration] [-DryRun] [-NoInteriors]
    version        default: ProjectVersion from Config\DefaultGame.ini, as major.minor (0.1.0 -> 0.1)
    configuration  Shipping (default), Development or Test
    -DryRun        resolve paths and print the BuildCookRun command, run nothing
    -NoInteriors   skip the interiors step (also INTERIORS=0)

  Interiors (on by default, as INTERIORS=1 in tools/perf/candidate-build-mac.sh): before the
  package, every interior declared in tools\interiors\*.json (all but catalog.json) is rebuilt
  headless by tools\interiors\unreal\build_interior_level.py with its anchor in the city map
  (GTP_INTERIOR_ANCHOR=1), and its level is added to MapsToCook for this package only
  (DirectoriesToAlwaysCook skips maps). The script stops in clear if a level is missing or its
  report has errors. Afterwards, whatever happens: DefaultGame.ini is put back byte for byte,
  Content\Maps is put back as found and its md5 checked, a generated interior level that was
  there before is restored and one the step created is removed. Needs the editor built for this
  commit (the step builds GranTheftPralineEditor first, quick when it is up to date).

  Environment:
    UE_ROOT        Unreal Engine install (default: C:\Program Files\Epic Games\UE_5.8)
    BUILDS_DIR     output root (default: <repo>\Builds)
    STALL_MINUTES  kill BuildCookRun when its log has not grown for this long (default 25)
    MAX_MINUTES    hard limit for one BuildCookRun attempt (default 240)
    RETRIES        new attempts after a stall or time-out (default 1; the cook
                   is incremental, so a retry resumes most of the work)
    VERSION_TAG    release tag (v0.9.0-rc1): sets ProjectVersion before the build
                   (tools\qa\stamp-version.ps1), as package-mac.sh does
    INTERIORS      1 (default) or 0
    COOK_MAPS      extra maps for this package only, comma separated (/Game/...): added to
                   MapsToCook like the interior levels, DefaultGame.ini put back afterwards
    INTERIOR_MINUTES  time limit of one interior build (default 20)

  Logs: Builds\<version>\package.log
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0)][string]$Version = '',
  [Parameter(Position = 1)][string]$Configuration = 'Shipping',
  [switch]$DryRun,
  [switch]$NoInteriors
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Put back what the interiors step changed; set once the step has saved its arrival state.
$script:putBack = $null
$script:putBackFailed = $false

function Fail([string]$Message) {
  [Console]::Error.WriteLine("package-win: $Message")
  if ($null -ne $script:putBack) { & $script:putBack }
  exit 1
}

# Any terminating error after the interiors step started: put the project back, then stop.
trap {
  if ($null -ne $script:putBack) { & $script:putBack }
  break
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
  '-ubtargs=-WaitMutex'   # wait for another UnrealBuildTool instead of failing (packages queued behind a compile)
)
$cmdLine = ((@($runUat) + $uatArgs) | ForEach-Object { Quote $_ }) -join ' '

# --- interiors ------------------------------------------------------------------------------
$interiors = (-not $NoInteriors) -and ((EnvOr 'INTERIORS' '1') -ne '0')
$contentDir = Join-Path $projectDir 'Content'
$gameIni = Join-Path $projectDir 'Config\DefaultGame.ini'
$mapsDir = Join-Path $contentDir 'Maps'
$cityMap = Join-Path $mapsDir 'L_City_RueDeLaRepublique.umap'
$editorCmd = Join-Path $ueRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$buildBat = Join-Path $ueRoot 'Engine\Build\BatchFiles\Build.bat'
$interiorScript = Join-Path $repoRoot 'tools\interiors\unreal\build_interior_level.py'
$interiorIds = @()
if ($interiors) {
  $interiorIds = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'tools\interiors') -Filter '*.json' -File |
    Where-Object { $_.Name -ne 'catalog.json' } | Sort-Object Name | ForEach-Object { $_.BaseName })
  if ($interiorIds.Count -eq 0) { Fail 'interiors step on but no interior declared in tools\interiors\*.json' }
  if (-not (Test-Path -LiteralPath $interiorScript)) { Fail "missing $interiorScript" }
  if (-not (Test-Path -LiteralPath (Join-Path $contentDir 'Python\gtp_run.py'))) { Fail 'Content\Python\gtp_run.py missing' }
}

# The files build_interior_level.py writes for one interior, relative to Content.
function InteriorFiles([string]$Id) {
  return @("Interiors\L_Interior_$Id.umap", "__ExternalActors__\Interiors\L_Interior_$Id", "__ExternalObjects__\Interiors\L_Interior_$Id")
}

function Md5Of([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) { return 'missing' }
  return (Get-FileHash -LiteralPath $Path -Algorithm MD5).Hash.ToLowerInvariant()
}

function RunInterior([string]$Id, [string]$LogDir) {
  $report = Join-Path $projectDir "Saved\InteriorBuild\$Id.json"
  if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }
  $log = Join-Path $LogDir "interior-$Id.log"
  [Environment]::SetEnvironmentVariable('GTP_PY_SCRIPT', $interiorScript, 'Process')
  [Environment]::SetEnvironmentVariable('GTP_INTERIOR_SPEC', (Join-Path $repoRoot "tools\interiors\$Id.json"), 'Process')
  [Environment]::SetEnvironmentVariable('GTP_INTERIOR_ANCHOR', '1', 'Process')
  $done = Join-Path $LogDir "interior-$Id.done"
  if (Test-Path -LiteralPath $done) { Remove-Item -LiteralPath $done -Force }
  [Environment]::SetEnvironmentVariable('GTP_PY_DONE', $done, 'Process')
  # A full editor, headless: editor subsystems (level, actors, static meshes) are None in the
  # pythonscript commandlet. gtp_run writes $done when the script returns; the editor can then
  # hang on exit, so it is stopped after a grace period. The value is quoted after '=', the form
  # Unreal reads whole (as -ExecCmds="...").
  $editorArgs = (Quote $projectFile) + ' -ExecutePythonScript="__import__(''gtp_run'').main()"' +
    ' -nullrhi -unattended -nosplash -nosound -stdout -FullStdOutLogOutput -abslog="' + $log + '"'
  Write-Output "Interior ${Id}: building the level headless (log $log)"
  $proc = Start-Process -FilePath $editorCmd -ArgumentList $editorArgs -RedirectStandardOutput "$log.out" `
    -RedirectStandardError "$log.err" -NoNewWindow -PassThru
  $null = $proc.Handle
  $limit = (Get-Date).AddMinutes([int](EnvOr 'INTERIOR_MINUTES' '20'))
  while (-not $proc.HasExited -and -not (Test-Path -LiteralPath $done)) {
    if ((Get-Date) -gt $limit) {
      StopTree $proc.Id
      $proc.WaitForExit()
      Fail "interior ${Id}: build over $(EnvOr 'INTERIOR_MINUTES' '20') min, stopped (see $log)"
    }
    Start-Sleep -Seconds 5
  }
  if (-not $proc.WaitForExit(20000)) {
    Write-Output "Interior ${Id}: script done, editor still open after 20 s: stopped"
    StopTree $proc.Id
  }
  $proc.WaitForExit()
  # The exit code alone is not the verdict (an unrelated asset that fails to load gives 1): the
  # report and the level are. It is always printed.
  Write-Output "Interior ${Id}: editor exit code $($proc.ExitCode)"
  if (-not (Test-Path -LiteralPath $report)) { Fail "interior ${Id}: no report $report (see $log)" }
  $parsed = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
  $errors = @($parsed.errors)
  if ($errors.Count -gt 0) {
    $errors | ForEach-Object { [Console]::Error.WriteLine("interior ${Id}: $_") }
    Fail "interior ${Id}: $($errors.Count) errors in $report"
  }
  if (-not (Test-Path -LiteralPath (Join-Path $contentDir "Interiors\L_Interior_$Id.umap"))) {
    Fail "interior ${Id}: L_Interior_$Id.umap missing after the build"
  }
  Copy-Item -LiteralPath $report -Destination (Join-Path $LogDir "interior-$Id.json") -Force
}

function AddMapsToCook([string[]]$Maps) {
  $text = [IO.File]::ReadAllText($gameIni)
  $eol = "`n"
  if ($text.Contains("`r`n")) { $eol = "`r`n" }
  $key = '[/Script/UnrealEd.ProjectPackagingSettings]' + $eol
  $at = $text.IndexOf($key)
  if ($at -lt 0) { Fail 'no [/Script/UnrealEd.ProjectPackagingSettings] section in DefaultGame.ini' }
  $lines = ($Maps | ForEach-Object { "+MapsToCook=(FilePath=`"$_`")" + $eol }) -join ''
  $text = $text.Insert($at + $key.Length, $lines)
  [IO.File]::WriteAllText($gameIni, $text, (New-Object Text.UTF8Encoding($false)))
}

# Maps added to MapsToCook for this package only (interior levels, COOK_MAPS): logged in package.log.
$cookMapsAdded = @()
$extraMaps = @((EnvOr 'COOK_MAPS' '') -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })

if ($interiors -and $DryRun) {
  Write-Output "Dry run, interiors step: $($interiorIds -join ', ') rebuilt with $editorCmd, then MapsToCook +"
  $interiorIds | ForEach-Object { Write-Output "  /Game/Interiors/L_Interior_$_" }
  Write-Output "  DefaultGame.ini md5 $(Md5Of $gameIni), city map md5 $(Md5Of $cityMap): both put back and checked after the package"
}

if ($interiors -and -not $DryRun) {
  if (-not (Test-Path -LiteralPath $editorCmd)) { Fail "UnrealEditor-Cmd.exe not found: $editorCmd (set UE_ROOT)" }
  if (-not (Test-Path -LiteralPath $cityMap)) { Fail "city map not found: $cityMap" }
  $stepDir = Join-Path $projectDir ('Saved\package-win-interiors-' + (Get-Date).ToString('yyyyMMdd-HHmmss'))
  New-Item -ItemType Directory -Force -Path $stepDir | Out-Null
  $mapMd5 = Md5Of $cityMap
  $iniMd5 = Md5Of $gameIni
  $iniBytes = [IO.File]::ReadAllBytes($gameIni)
  $mapsReal = (Get-Item -LiteralPath $mapsDir).FullName
  $snapshot = Join-Path $stepDir 'snapshot'
  Copy-Item -LiteralPath $mapsReal -Destination (Join-Path $snapshot 'Maps') -Recurse -Force
  $created = @()
  foreach ($id in $interiorIds) {
    foreach ($rel in (InteriorFiles $id)) {
      $created += $rel
      $src = Join-Path $contentDir $rel
      if (Test-Path -LiteralPath $src) {
        $dest = Join-Path $snapshot $rel
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
        Copy-Item -LiteralPath $src -Destination $dest -Recurse -Force
      }
    }
  }
  Write-Output "Interiors: arrival state saved in $stepDir (city map md5 $mapMd5, DefaultGame.ini md5 $iniMd5)"
  $script:putBack = {
    $script:putBack = $null   # once
    $problems = 0
    try { [IO.File]::WriteAllBytes($gameIni, $iniBytes) } catch { [Console]::Error.WriteLine("put back DefaultGame.ini failed: $_"); $problems++ }
    foreach ($rel in $created) {
      $target = Join-Path $contentDir $rel
      $saved = Join-Path $snapshot $rel
      try {
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
        if (Test-Path -LiteralPath $saved) { Copy-Item -LiteralPath $saved -Destination $target -Recurse -Force }
      } catch { [Console]::Error.WriteLine("put back $rel failed: $_"); $problems++ }
    }
    try {
      Get-ChildItem -LiteralPath $mapsReal -Force | Remove-Item -Recurse -Force
      Get-ChildItem -LiteralPath (Join-Path $snapshot 'Maps') -Force | Copy-Item -Destination $mapsReal -Recurse -Force
    } catch { [Console]::Error.WriteLine("put back Content\Maps failed: $_"); $problems++ }
    $nowMap = Md5Of $cityMap
    $nowIni = Md5Of $gameIni
    Write-Output "Put back: city map md5 $nowMap ($(if ($nowMap -eq $mapMd5) { 'unchanged' } else { "CHANGED from $mapMd5" })), DefaultGame.ini md5 $nowIni ($(if ($nowIni -eq $iniMd5) { 'unchanged' } else { "CHANGED from $iniMd5" }))"
    if ($nowMap -ne $mapMd5 -or $nowIni -ne $iniMd5) { $problems++ }
    if ($problems -eq 0) {
      Remove-Item -LiteralPath $snapshot -Recurse -Force
    } else {
      [Console]::Error.WriteLine("package-win: put back incomplete, the arrival state is kept in $snapshot")
      $script:putBackFailed = $true
    }
  }
  Write-Output 'Interiors: building the editor for this commit'
  $buildLine = (Quote $buildBat) + ' GranTheftPralineEditor Win64 Development ' + (Quote "-project=$projectFile") + ' -WaitMutex'
  $build = Start-Process -FilePath 'cmd.exe' -ArgumentList "/d /s /c `"$buildLine`"" -NoNewWindow -PassThru `
    -RedirectStandardOutput (Join-Path $stepDir 'editor-build.log') -RedirectStandardError (Join-Path $stepDir 'editor-build.err')
  $null = $build.Handle
  $build.WaitForExit()
  if ($build.ExitCode -ne 0) { Fail "editor build failed (exit $($build.ExitCode), see $stepDir\editor-build.log)" }
  foreach ($id in $interiorIds) { RunInterior $id $stepDir }
  AddMapsToCook ($interiorIds | ForEach-Object { "/Game/Interiors/L_Interior_$_" })
  $cookMapsAdded += @($interiorIds | ForEach-Object { "/Game/Interiors/L_Interior_$_" })
  Write-Output "Interiors: $($interiorIds -join ', ') built, added to MapsToCook for this package only"
}
if ($extraMaps.Count -gt 0 -and -not $DryRun) {
  if ($null -eq $script:putBack) {
    # No interiors step: save DefaultGame.ini here and put it back after the package.
    $extraIniMd5 = Md5Of $gameIni
    $extraIniBytes = [IO.File]::ReadAllBytes($gameIni)
    $script:putBack = {
      $script:putBack = $null   # once
      try { [IO.File]::WriteAllBytes($gameIni, $extraIniBytes) } catch { [Console]::Error.WriteLine("put back DefaultGame.ini failed: $_"); $script:putBackFailed = $true }
      $nowIni = Md5Of $gameIni
      Write-Output "Put back: DefaultGame.ini md5 $nowIni ($(if ($nowIni -eq $extraIniMd5) { 'unchanged' } else { "CHANGED from $extraIniMd5" }))"
      if ($nowIni -ne $extraIniMd5) { $script:putBackFailed = $true }
    }
  }
  $extraMaps = @($extraMaps | Where-Object { $cookMapsAdded -notcontains $_ } | Select-Object -Unique)
  if ($extraMaps.Count -gt 0) { AddMapsToCook $extraMaps; $cookMapsAdded += $extraMaps }
}
$cookMapsLine = "package-win: MapsToCook added for this package: $(if ($cookMapsAdded.Count -gt 0) { $cookMapsAdded -join ', ' } elseif ($DryRun) { @(@(@(if ($interiors) { $interiorIds | ForEach-Object { "/Game/Interiors/L_Interior_$_" } }) + $extraMaps) | Select-Object -Unique) -join ', ' } else { 'none' })"
if ($DryRun -and $cookMapsLine.EndsWith(': ')) { $cookMapsLine += 'none' }
Write-Output $cookMapsLine
Write-Output "Packaging version $Version ($Configuration) into $outDir"
Write-Output "Engine: $ueRoot"
Write-Output "Full log: $logFile (watchdog: $stallMinutes min without output, $maxMinutes min max, $retries retry)"

if ($DryRun) {
  if (-not $engineFound) { Write-Output "Warning: RunUAT not found: $runUat (set UE_ROOT)" }
  Write-Output "Dry run, command not started:"
  Write-Output "  cmd.exe /d /s /c `"$cmdLine`""
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

$startTime = Get-Date
Set-Content -LiteralPath $logFile -Value $null
Add-Content -LiteralPath $logFile -Value $cookMapsLine
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
  Fail "see $logFile"
}

if ($null -ne $script:putBack) { & $script:putBack }
if ($script:putBackFailed) { Fail 'the interiors step could not put the project back as found (see above)' }

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
