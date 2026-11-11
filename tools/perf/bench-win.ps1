<#
.SYNOPSIS
  Windows counterpart of tools/perf/bench.sh: fixed scenarios on a packaged
  Win64 build, one table of GPU/frame p50/p95, share of frames <= 33.3 ms
  and hitches > 100 ms.

.DESCRIPTION
  Same scenarios, arguments and table as bench.sh, so Mac and PC numbers sit
  side by side. Per run it also samples the NVIDIA GPU with nvidia-smi once
  per second (utilization, VRAM used, clocks, power, temperature) into
  nvidia-smi.csv, and with -CsvProfile asks the game for Unreal's CSV
  profiler (GPU stats) and copies that CSV into the run folder.
  No screenshot is taken during a measurement.

  Usage: powershell -ExecutionPolicy Bypass -File tools\perf\bench-win.ps1 <build_dir> [out_dir] [-DryRun]
    build_dir  folder holding GranTheftPraline.exe (e.g. Builds\0.3\GranTheftPraline)
    out_dir    default <build_dir>\bench\<stamp>

  Environment (same meaning as in bench.sh):
    SCENARIOS  space-separated, default "street-fixed street-demo"
                 street-fixed : player standing still at the start
                 street-demo  : -DemoRoute (walk, get in the car, drive)
                 night, rain  : standing still with -Ambience=night|rain
    AMBIENCE   ambience for street-* (e.g. noon), empty by default
    DURATION   seconds per run, default 90
    VARIANTS   ';'-separated "label=cvar=v,cvar=v" entries passed as
               -GfxCVars, default "base=" (no override)
    EXTRA_ARGS extra game arguments for every run
    REFERENCE  results.csv of an earlier bench: fail on a GPU or frame p95
               regression above REGRESSION_MS (default 2) or more hitches
    MAX_CPU    refuse to start while CPU load stays above this % (default 30)
    NVSMI_MS   nvidia-smi sampling period in ms (default 1000)

  Switches:
    -CsvProfile  add -csvGpuStats -csvCaptureFrames=<n> and copy the CSV
                 profile into the run (needs a Development or Test build)
    -DryRun      print every command, run nothing

  Exit code: 0 done, 1 error or regression.
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0, Mandatory = $true)][string]$BuildDir,
  [Parameter(Position = 1)][string]$OutDir = '',
  [switch]$CsvProfile,
  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Inv = [Globalization.CultureInfo]::InvariantCulture

function Fail([string]$Message) {
  [Console]::Error.WriteLine("bench-win: $Message")
  exit 1
}

function EnvOr([string]$Name, [string]$Default) {
  $value = [Environment]::GetEnvironmentVariable($Name)
  if ([string]::IsNullOrEmpty($value)) { return $Default }
  return $value
}

function Quote([string]$Arg) {
  if ($Arg -match '[\s"]') { return '"' + ($Arg -replace '"', '\"') + '"' }
  return $Arg
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$exe = Join-Path $BuildDir 'GranTheftPraline.exe'
if (-not (Test-Path -LiteralPath $exe) -and -not $DryRun) { Fail "no build at $exe" }
if ($OutDir -eq '') { $OutDir = Join-Path $BuildDir "bench\$stamp" }
$duration = [int](EnvOr 'DURATION' '90')
$scenarios = (EnvOr 'SCENARIOS' 'street-fixed street-demo') -split '\s+' | Where-Object { $_ }
$variants = (EnvOr 'VARIANTS' 'base=') -split ';' | Where-Object { $_ -ne '' }
$extraArgs = (EnvOr 'EXTRA_ARGS' '') -split '\s+' | Where-Object { $_ }
$ambience = EnvOr 'AMBIENCE' ''
$maxCpu = [int](EnvOr 'MAX_CPU' '30')
$nvsmiMs = [int](EnvOr 'NVSMI_MS' '1000')
$nvsmi = Join-Path $env:SystemRoot 'System32\nvidia-smi.exe'
$hasNvsmi = Test-Path -LiteralPath $nvsmi

function ScenarioArgs([string]$Name) {
  $amb = @(); if ($ambience) { $amb = @("-Ambience=$ambience") }
  switch ($Name) {
    'street-fixed' { return $amb }
    'street-demo' { return $amb + @('-DemoRoute') }
    'night' { return @('-Ambience=night') }
    'rain' { return @('-Ambience=rain') }
    default { Fail "unknown scenario $Name" }
  }
}

function Pct([double[]]$Values, [double]$P) {
  $v = $Values | Sort-Object
  $k = ($v.Count - 1) * $P / 100.0
  $lo = [int][math]::Floor($k)
  $hi = [math]::Min($lo + 1, $v.Count - 1)
  return $v[$lo] + ($v[$hi] - $v[$lo]) * ($k - $lo)
}

# Same numbers as summarize.py: one Markdown row, one results.csv row.
function Summarize([string]$Csv, [string]$Scenario, [string]$Variant, [string]$Results) {
  if (-not (Test-Path -LiteralPath $Csv)) { return "| $Scenario | $Variant | no data | | | | | |" }
  $frame = New-Object Collections.Generic.List[double]
  $gpu = New-Object Collections.Generic.List[double]
  foreach ($r in (Import-Csv -LiteralPath $Csv)) {
    $f = 0.0; $g = 0.0
    if (-not [double]::TryParse($r.frame_ms, [Globalization.NumberStyles]::Float, $Inv, [ref]$f)) { continue }
    $frame.Add($f)
    if ([double]::TryParse($r.gpu_ms, [Globalization.NumberStyles]::Float, $Inv, [ref]$g) -and $g -ge 0 -and $g -le 1000) { $gpu.Add($g) }
  }
  if ($frame.Count -eq 0) { return "| $Scenario | $Variant | no data | | | | | |" }
  $fa = $frame.ToArray(); $ga = $gpu.ToArray()
  $within = 100.0 * @($fa | Where-Object { $_ -le 33.3 }).Count / $fa.Count
  $hitches = @($fa | Where-Object { $_ -gt 100.0 }).Count
  $g50 = ''; $g95 = ''
  if ($ga.Count) { $g50 = [math]::Round((Pct $ga 50), 2); $g95 = [math]::Round((Pct $ga 95), 2) }
  $f50 = [math]::Round((Pct $fa 50), 2); $f95 = [math]::Round((Pct $fa 95), 2)
  if (-not (Test-Path -LiteralPath $Results)) {
    'scenario,variant,frames,gpu_p50,gpu_p95,frame_p50,frame_p95,within_budget_pct,hitches' | Set-Content -LiteralPath $Results -Encoding ascii
  }
  ((@($Scenario, $Variant, $fa.Count, $g50, $g95, $f50, $f95, [math]::Round($within, 1), $hitches) |
    ForEach-Object { [string]::Format($Inv, '{0}', $_) }) -join ',') | Add-Content -LiteralPath $Results -Encoding ascii
  $fmt = { param($x) if ($x -eq '') { 'nan' } else { ([double]$x).ToString('0.0', $Inv) } }
  return "| $Scenario | $Variant | $(& $fmt $g50) | $(& $fmt $g95) | $(& $fmt $f50) | $(& $fmt $f95) | $($within.ToString('0', $Inv)) % | $hitches |"
}

# Same gate as compare.py.
function CompareResults([string]$Reference, [string]$Current, [double]$MaxMs) {
  $ref = @{}; foreach ($r in (Import-Csv -LiteralPath $Reference)) { $ref["$($r.scenario)/$($r.variant)"] = $r }
  $bad = 0
  foreach ($r in (Import-Csv -LiteralPath $Current)) {
    $key = "$($r.scenario)/$($r.variant)"
    if (-not $ref.ContainsKey($key)) { continue }
    $o = $ref[$key]
    foreach ($col in 'gpu_p95', 'frame_p95') {
      if ($o.$col -eq '' -or $r.$col -eq '') { continue }
      $d = [double]::Parse($r.$col, $Inv) - [double]::Parse($o.$col, $Inv)
      if ($d -gt $MaxMs) { Write-Host "REGRESSION $key ${col}: $($o.$col) -> $($r.$col) (+$($d.ToString('0.00', $Inv)) ms)"; $bad++ }
    }
    if ([int]$r.hitches -gt [int]$o.hitches) { Write-Host "REGRESSION $key hitches: $($o.hitches) -> $($r.hitches)"; $bad++ }
  }
  if ($bad -eq 0) { Write-Host 'Non-regression gate: OK' }
  return $bad
}

function WaitForCalm {
  for ($i = 0; $i -lt 40; $i++) {
    $load = (Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average
    if ($load -le $maxCpu) { return }
    Write-Output "  CPU load $load % > $maxCpu %, waiting..."
    Start-Sleep -Seconds 15
  }
  Fail "CPU load stays above $maxCpu %, aborting"
}

# Run the game with a watchdog (limit + 60 s), like run_game in bench.sh.
function RunGame([int]$Limit, [string]$Log, [string[]]$GameArgs) {
  $argLine = ($GameArgs | ForEach-Object { Quote $_ }) -join ' '
  $p = Start-Process -FilePath $exe -ArgumentList $argLine -RedirectStandardOutput $Log -RedirectStandardError "$Log.err" -PassThru -WindowStyle Normal
  $null = $p.Handle
  if (-not $p.WaitForExit(($Limit + 60) * 1000)) {
    "  watchdog: still running after $($Limit + 60) s, killed" | Tee-Object -FilePath $Log -Append | Write-Output
    & taskkill.exe /T /F /PID $p.Id 2>&1 | Out-Null
  } elseif ($p.ExitCode -ne 0) { Write-Output "  exit status $($p.ExitCode)" }
}

Write-Output "Build: $exe"
Write-Output "Output: $OutDir ($duration s per run; scenarios: $($scenarios -join ' '); variants: $($variants -join ' ; '))"
if (-not $hasNvsmi) { Write-Output 'Note: nvidia-smi not found, no GPU sampling' }

if (-not $DryRun) {
  if (Get-Process -Name 'GranTheftPraline*', 'UnrealEditor*' -ErrorAction SilentlyContinue) { Fail 'an Unreal or game process is already running' }
  New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
  $gpuName = ''
  if ($hasNvsmi) { $gpuName = (& $nvsmi '--query-gpu=name,driver_version,memory.total' '--format=csv,noheader') -join ' ' }
  $summary = Join-Path $OutDir 'summary.md'
  @(
    "# Bench $stamp", '',
    "Build: $BuildDir ; $duration s per run ; host $env:COMPUTERNAME ; GPU: $gpuName", '',
    '| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |',
    '| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |'
  ) | Set-Content -LiteralPath $summary -Encoding ascii
}
$results = Join-Path $OutDir 'results.csv'

foreach ($scenario in $scenarios) {
  $sargs = @(ScenarioArgs $scenario)
  foreach ($v in $variants) {
    $label = $v.Split('=')[0]
    $cvars = $v.Substring($label.Length + 1)
    $run = Join-Path $OutDir "${scenario}__$label"
    $gameArgs = @() + $sargs + $extraArgs
    if ($cvars) { $gameArgs += "-GfxCVars=$cvars" }
    if ($CsvProfile) {
      $gameArgs += @('-csvGpuStats', "-CsvMetadata=scenario=$scenario,variant=$label", "-csvCaptureFrames=$($duration * 60)", "-CsvCopyTo=$run\csvprofile")
    }
    $gameArgs += @('-SkipTitle', "-FrameLogFile=$run\frametimes.csv", "-ExitAfter=$duration", '-log', "-abslog=$run\game.log")
    $smiArgs = @('--query-gpu=timestamp,utilization.gpu,utilization.memory,memory.used,memory.total,clocks.gr,clocks.mem,power.draw,temperature.gpu,pstate', '--format=csv', "-lms", "$nvsmiMs", '-f', "$run\nvidia-smi.csv")

    Write-Output "== $scenario / $label ($duration s)"
    if ($DryRun) {
      Write-Output "  $(Quote $exe) $(($gameArgs | ForEach-Object { Quote $_ }) -join ' ')"
      if ($hasNvsmi) { Write-Output "  nvidia-smi $(($smiArgs | ForEach-Object { Quote $_ }) -join ' ')   (stopped when the game exits)" }
      continue
    }
    New-Item -ItemType Directory -Force -Path $run | Out-Null
    WaitForCalm
    $smi = $null
    if ($hasNvsmi) { $smi = Start-Process -FilePath $nvsmi -ArgumentList (($smiArgs | ForEach-Object { Quote $_ }) -join ' ') -PassThru -WindowStyle Hidden }
    try { RunGame $duration (Join-Path $run 'stdout.log') $gameArgs }
    finally { if ($smi -and -not $smi.HasExited) { Stop-Process -Id $smi.Id -Force } }
    $sumTxt = Join-Path $run 'frametimes.summary.txt'
    if (Test-Path -LiteralPath $sumTxt) { Copy-Item -LiteralPath $sumTxt -Destination (Join-Path $run 'summary.txt') }
    Summarize (Join-Path $run 'frametimes.csv') $scenario $label $results | Add-Content -LiteralPath $summary -Encoding ascii
    Start-Sleep -Seconds 5
  }
}

if ($DryRun) { Write-Output 'Dry run: nothing started.'; exit 0 }
Write-Output "Bench done: $summary"
Get-Content -LiteralPath $summary
$reference = EnvOr 'REFERENCE' ''
if ($reference) {
  $bad = CompareResults $reference $results ([double](EnvOr 'REGRESSION_MS' '2'))
  if ($bad -gt 0) { exit 1 }
}
exit 0
