<#
.SYNOPSIS
  Compile one branch for Win64 (Development, game and editor targets) in a
  temporary git worktree and print a short verdict.

.DESCRIPTION
  For branches written without a Windows machine. The branch is fetched from
  origin and checked out, detached, in a worktree next to the clone; Build.bat
  compiles the editor target, then the game target, from the command line
  (the editor is never launched). Errors are listed as file:line: code and
  classified:
    msvc  MSVC-only diagnostics that clang on macOS accepts (narrowing,
          shadowing, implicit conversions...): fix them, they are real on Windows
    code  any other compiler error
    link  linker errors
  The worktree is removed afterwards unless -KeepWorktree is given.

  Usage: powershell -ExecutionPolicy Bypass -File tools\build\compile-check-win.ps1 <branch> [-CherryPick <commit>] [-KeepWorktree] [-DryRun]
    branch         remote branch name, e.g. feat/street-details-10
    -CherryPick    commit applied (without committing) before compiling,
                   e.g. a fix not yet in the branch
    -KeepWorktree  keep the worktree (faster incremental reruns)
    -DryRun        resolve paths and print the commands, run nothing

  Environment:
    UE_ROOT        Unreal Engine install (default: C:\Program Files\Epic Games\UE_5.8)
    WORKTREES_DIR  worktree root (default: GTAL-worktrees, next to the main clone)
    MAX_MINUTES    time limit per target (default 40)

  Logs: Builds\logs\check-<branch>-<target>.log
  Exit code: 0 OK, 1 compile errors, 2 setup error or time-out.
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0, Mandatory = $true)][string]$Branch,
  [string]$CherryPick = '',
  [switch]$KeepWorktree,
  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
  [Console]::Error.WriteLine("compile-check-win: $Message")
  exit 2
}

function EnvOr([string]$Name, [string]$Default) {
  $value = [Environment]::GetEnvironmentVariable($Name)
  if ([string]::IsNullOrEmpty($value)) { return $Default }
  return $value
}

# Run git, stop on failure. Arguments are passed one by one (paths may hold spaces).
function Invoke-Git {
  $out = & git.exe @args 2>&1
  if ($LASTEXITCODE -ne 0) { Fail "git $($args -join ' ') failed: $out" }
  return $out
}

# MSVC diagnostics that clang (macOS) accepts by default or does not raise.
$MsvcOnly = @{
  'C2397' = 'narrowing conversion in braced init'
  'C4244' = 'possible loss of data in conversion'
  'C4267' = 'size_t to smaller type'
  'C4305' = 'truncation (double to float)'
  'C4456' = 'local hides previous local'
  'C4457' = 'local hides function parameter'
  'C4458' = 'local hides class member'
  'C4459' = 'local hides global'
  'C4701' = 'possibly uninitialized local'
  'C4703' = 'possibly uninitialized pointer'
  'C4018' = 'signed/unsigned comparison'
  'C4389' = 'signed/unsigned equality'
  'C4146' = 'unary minus on unsigned'
  'C4800' = 'implicit conversion to bool'
  'C4834' = 'discarded nodiscard result'
  'C4125' = 'decimal digit ends octal escape'
}

$scriptRepo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
# Work from the main clone even when this script runs from a worktree.
$commonDir = (& git -C $scriptRepo rev-parse --path-format=absolute --git-common-dir 2>$null)
if ($LASTEXITCODE -ne 0 -or -not $commonDir) { Fail "not a git clone: $scriptRepo" }
$repoRoot = Split-Path -Parent ([IO.Path]::GetFullPath($commonDir))
$ueRoot = EnvOr 'UE_ROOT' 'C:\Program Files\Epic Games\UE_5.8'
$buildBat = Join-Path $ueRoot 'Engine\Build\BatchFiles\Build.bat'
$worktreesDir = EnvOr 'WORKTREES_DIR' (Join-Path (Split-Path -Parent $repoRoot) 'GTAL-worktrees')
$maxMinutes = [int](EnvOr 'MAX_MINUTES' '40')
$safeName = ($Branch -replace '^(origin/)', '') -replace '[^A-Za-z0-9._-]', '-'
$worktree = Join-Path $worktreesDir "check-$safeName"
$project = Join-Path $worktree 'GranTheftPraline\GranTheftPraline.uproject'
$logsDir = Join-Path $repoRoot 'Builds\logs'
$targets = @('GranTheftPralineEditor', 'GranTheftPraline')
$remoteBranch = $Branch -replace '^origin/', ''

function TargetCmd([string]$Target, [string]$Log) {
  return "`"`"$buildBat`" $Target Win64 Development `"-project=$project`" -waitmutex -NoHotReloadFromIDE -utf8output > `"$Log`" 2>&1`""
}

Write-Output "Branch: $remoteBranch"
Write-Output "Worktree: $worktree"
Write-Output "Engine: $ueRoot (limit $maxMinutes min per target)"

if ($DryRun) {
  if (-not (Test-Path -LiteralPath $buildBat)) { Write-Output "Warning: Build.bat not found: $buildBat (set UE_ROOT)" }
  Write-Output 'Dry run, nothing started:'
  Write-Output "  git -C `"$repoRoot`" fetch origin $remoteBranch"
  Write-Output "  git -C `"$repoRoot`" worktree add --detach `"$worktree`" origin/$remoteBranch"
  if ($CherryPick) { Write-Output "  git -C `"$worktree`" cherry-pick -n $CherryPick" }
  foreach ($t in $targets) {
    Write-Output "  cmd.exe /d /s /c $(TargetCmd $t (Join-Path $logsDir "check-$safeName-$t.log"))"
  }
  if (-not $KeepWorktree) { Write-Output "  git -C `"$repoRoot`" worktree remove --force `"$worktree`"" }
  exit 0
}

if (-not (Test-Path -LiteralPath $buildBat)) { Fail "Build.bat not found: $buildBat (set UE_ROOT)" }
New-Item -ItemType Directory -Force -Path $logsDir, $worktreesDir | Out-Null

# Source only: assets are not needed to compile, skip the LFS download.
$env:GIT_LFS_SKIP_SMUDGE = '1'
Invoke-Git -C $repoRoot fetch -q origin $remoteBranch | Out-Null
if (Test-Path -LiteralPath $worktree) {
  Invoke-Git -C $worktree reset -q --hard | Out-Null
  Invoke-Git -C $worktree checkout -q --detach "origin/$remoteBranch" | Out-Null
} else {
  Invoke-Git -C $repoRoot worktree add -q --detach $worktree "origin/$remoteBranch" | Out-Null
}
$commit = (Invoke-Git -C $worktree rev-parse --short HEAD | Select-Object -First 1)
if ($CherryPick) { Invoke-Git -C $worktree cherry-pick -n $CherryPick | Out-Null }
Write-Output "Commit: $commit$(if ($CherryPick) { " + $CherryPick (cherry-picked)" })"

$started = Get-Date
$status = 0
$failedLog = ''
foreach ($t in $targets) {
  $log = Join-Path $logsDir "check-$safeName-$t.log"
  $t0 = Get-Date
  $proc = Start-Process -FilePath 'cmd.exe' -ArgumentList "/d /s /c $(TargetCmd $t $log)" -NoNewWindow -PassThru
  $null = $proc.Handle
  if (-not $proc.WaitForExit($maxMinutes * 60 * 1000)) {
    & taskkill.exe /T /F /PID $proc.Id 2>&1 | Out-Null
    Write-Output "  $t : time-out after $maxMinutes min"
    $status = 2
    break
  }
  Write-Output ("  {0} : {1} in {2}s" -f $t, $(if ($proc.ExitCode -eq 0) { 'OK' } else { "failed (exit $($proc.ExitCode))" }), [int]((Get-Date) - $t0).TotalSeconds)
  if ($proc.ExitCode -ne 0) { $status = 1; $failedLog = $log; break }
}
$elapsed = [int]((Get-Date) - $started).TotalSeconds

if ($failedLog) {
  $seen = @{}
  $errors = @()
  foreach ($m in (Select-String -LiteralPath $failedLog -Encoding UTF8 -Pattern '^(.+?)\((\d+)(?:,\d+)?\)\s*:\s*(?:fatal )?error ([A-Z]+\d+)\s*:\s*(.*)$')) {
    $g = $m.Matches[0].Groups
    $file = ($g[1].Value -replace '^.*?[\\/](Source|Plugins)[\\/]', '$1/') -replace '\\', '/'
    $code = $g[3].Value
    $key = "${file}:$($g[2].Value):$code"
    if ($seen.ContainsKey($key)) { continue }
    $seen[$key] = $true
    $kind = 'code'
    if ($code -like 'LNK*') { $kind = 'link' } elseif ($MsvcOnly.ContainsKey($code)) { $kind = 'msvc' }
    $errors += [pscustomobject]@{ Kind = $kind; Where = $key; Text = $g[4].Value.Trim() }
  }
  foreach ($m in (Select-String -LiteralPath $failedLog -Encoding UTF8 -Pattern 'error (LNK\d+)\s*:\s*(.*)$')) {
    $key = "link:$($m.Matches[0].Groups[1].Value):$($m.Matches[0].Groups[2].Value)"
    if ($seen.ContainsKey($key)) { continue }
    $seen[$key] = $true
    $errors += [pscustomobject]@{ Kind = 'link'; Where = $m.Matches[0].Groups[1].Value; Text = $m.Matches[0].Groups[2].Value.Trim() }
  }
  Write-Output 'Errors:'
  foreach ($e in $errors) {
    $hint = ''
    if ($e.Kind -eq 'msvc') { $hint = " [$($MsvcOnly[$e.Where.Split(':')[-1]])]" }
    Write-Output "  $($e.Kind)  $($e.Where)$hint  $($e.Text)"
  }
  if ($errors.Count -eq 0) { Write-Output "  (no compiler error line found, see $failedLog)" }
  $real = @($errors | Where-Object { $_.Kind -ne 'msvc' }).Count
  $msvc = @($errors | Where-Object { $_.Kind -eq 'msvc' }).Count
  Write-Output "Log: $failedLog"
}

if (-not $KeepWorktree) {
  & git -C $repoRoot worktree remove --force $worktree 2>&1 | Out-Null
  if (Test-Path -LiteralPath $worktree) { Remove-Item -LiteralPath $worktree -Recurse -Force -ErrorAction SilentlyContinue }
  & git -C $repoRoot worktree prune 2>&1 | Out-Null
}

switch ($status) {
  0 { Write-Output "VERDICT: OK  $remoteBranch @$commit  (${elapsed}s)" }
  1 { Write-Output "VERDICT: FAILED  $remoteBranch @$commit  (${elapsed}s, $msvc MSVC-only, $real other)" }
  default { Write-Output "VERDICT: TIME-OUT  $remoteBranch @$commit  (${elapsed}s)" }
}
exit $status
