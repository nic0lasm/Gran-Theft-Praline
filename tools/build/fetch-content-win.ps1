<#
.SYNOPSIS
  Download the project Content archive served by the Mac, verify it and
  extract it into the Windows clone.

.DESCRIPTION
  Most of the game content (maps, city, vehicles, sample content) is not in
  git. The Mac serves it as one tar archive over HTTP on the local network;
  this script fetches it on the PC.

  - Checks free disk space first (archive + extraction, default 70 GB).
  - Downloads into <work folder>\transfer\ with curl.exe, resuming (-C -).
    When the server goes away (the Mac stops it during a measurement window),
    curl is started again after a pause, until the archive is complete or the
    time limit is reached. The partial file is kept between attempts.
  - Verifies the SHA-256 (Get-FileHash).
  - Extracts with tar.exe (Windows bsdtar) into -Dest.
  - Checks that Content\Maps\L_City_RueDeLaRepublique.umap exists and prints
    the file count and size of Content.
  - Never deletes the archive.

  Network: the PC only opens an outgoing connection to the Mac, so Windows
  Firewall needs no inbound rule on the PC. (The Mac side must accept the
  incoming connection on its port.)

  Usage: powershell -ExecutionPolicy Bypass -File tools\build\fetch-content-win.ps1 -Url http://<mac-ip>:<port>/Content.tar [-Sha256 <hex>] [-WaitMinutes 30] [-Dest <dir>] [-MaxHours 12] [-DryRun]
    -Url          archive URL served by the Mac
    -Sha256       expected SHA-256 of the archive (64 hex characters); when
                  omitted, the size is checked against the server and the
                  whole archive is listed with tar.exe
    -WaitMinutes  wait up to this long for the server (it answers once the
                  archive is written; the size must be stable for 1 min)
    -Dest         extraction folder (default: <clone>\GranTheftPraline; the
                  archive holds Content\...)
    -TransferDir  download folder (default: transfer\, next to the clone)
    -MaxHours     give up after this many hours of retries (default 12)
    -PauseSeconds wait between two curl runs when the server is away (default 60)
    -MinFreeGB    free space required on the download drive (default 70)
    -LimitRate    curl --limit-rate value, e.g. 60m, to spare the Mac during measurements
    -DryRun       resolve paths, check disk space, print the commands, run nothing

  Exit code: 0 done, 1 error, 2 gave up (time limit).
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$Url,
  [string]$Sha256 = '',
  [int]$WaitMinutes = 0,
  [string]$LimitRate = '',
  [string]$Dest = '',
  [string]$TransferDir = '',
  [double]$MaxHours = 12,
  [int]$PauseSeconds = 60,
  [int]$MinFreeGB = 70,
  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Fail([string]$Message, [int]$Code = 1) {
  [Console]::Error.WriteLine("fetch-content-win: $Message")
  exit $Code
}

function FreeGB([string]$Path) {
  $root = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Path))
  $drive = New-Object IO.DriveInfo($root)
  return [math]::Round($drive.AvailableFreeSpace / 1GB, 1)
}

$Sha256 = $Sha256.Trim().ToLowerInvariant()
if ($Sha256 -ne '' -and $Sha256 -notmatch '^[0-9a-f]{64}$') { Fail 'Sha256 must be 64 hexadecimal characters' }

# Content-Length announced by the server, or -1 when it does not answer.
function RemoteLength {
  $head = & curl.exe -sI -f --connect-timeout 10 --max-time 30 $Url 2>$null
  if ($LASTEXITCODE -ne 0) { return [int64]-1 }
  foreach ($line in $head) {
    if ($line -match '^Content-Length:\s*(\d+)') { return [int64]$Matches[1] }
  }
  return [int64]-1
}
if ($Url -notmatch '^https?://') { Fail "Url must start with http:// or https://: $Url" }

$scriptRepo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$commonDir = (& git.exe -C $scriptRepo rev-parse --path-format=absolute --git-common-dir 2>$null)
if ($LASTEXITCODE -eq 0 -and $commonDir) {
  $repoRoot = Split-Path -Parent ([IO.Path]::GetFullPath($commonDir))
} else {
  $repoRoot = $scriptRepo
}
if ($Dest -eq '') { $Dest = Join-Path $repoRoot 'GranTheftPraline' }
if ($TransferDir -eq '') { $TransferDir = Join-Path (Split-Path -Parent $repoRoot) 'transfer' }

$fileName = [IO.Path]::GetFileName(([Uri]$Url).AbsolutePath)
if (-not $fileName) { $fileName = 'Content.tar' }
$archive = Join-Path $TransferDir $fileName
$mapCheck = Join-Path $Dest 'Content\Maps\L_City_RueDeLaRepublique.umap'
$curlArgs = @('-sS', '-f', '-L', '-C', '-', '--retry', '20', '--retry-delay', '30', '--retry-all-errors',
  '--connect-timeout', '20', '--speed-limit', '1024', '--speed-time', '120', '-o', $archive, $Url)
if ($LimitRate -ne '') { $curlArgs = @('--limit-rate', $LimitRate) + $curlArgs }

$partial = 0
if (Test-Path -LiteralPath $archive) { $partial = (Get-Item -LiteralPath $archive).Length }
$free = FreeGB $TransferDir
$needed = $MinFreeGB - [math]::Round($partial / 1GB, 1)

Write-Output "Archive: $Url"
Write-Output "Download to: $archive$(if ($partial) { " (resuming from $([math]::Round($partial / 1MB)) MB)" })"
Write-Output "Extract to: $Dest"
Write-Output "Free space: $free GB (need about $needed GB more)"

if ($DryRun) {
  if ($free -lt $needed) { Write-Output "Warning: not enough free space" }
  Write-Output 'Dry run, nothing started:'
  $shown = ($curlArgs | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
  Write-Output "  curl.exe $shown   (repeated until complete or $MaxHours h)"
  Write-Output "  Get-FileHash -Algorithm SHA256 `"$archive`"  (expect $Sha256)"
  Write-Output "  tar.exe -xf `"$archive`" -C `"$Dest`""
  Write-Output "  check: $mapCheck"
  exit 0
}

if ($free -lt $needed) { Fail "only $free GB free on the drive of $TransferDir, about $needed GB needed" }
New-Item -ItemType Directory -Force -Path $TransferDir, $Dest | Out-Null

$started = Get-Date

# Optionally wait for the server: it starts once the archive is written, and
# the size must be the same on two probes one minute apart.
$expected = [int64]-1
if ($WaitMinutes -gt 0) {
  Write-Output "Waiting up to $WaitMinutes min for the server..."
  $last = [int64]-1
  while ($true) {
    $len = RemoteLength
    if ($len -gt 0 -and $len -eq $last) { $expected = $len; break }
    if ($len -gt 0) { Write-Output "  $(Get-Date -Format 'HH:mm:ss') server answers, $([math]::Round($len / 1MB)) MB; checking again in 60 s" }
    else { Write-Output "  $(Get-Date -Format 'HH:mm:ss') no answer yet" }
    if (((Get-Date) - $started).TotalMinutes -ge $WaitMinutes) { Fail "server not reachable or size not stable after $WaitMinutes min: $Url" 2 }
    $last = $len
    Start-Sleep -Seconds $(if ($len -gt 0) { 60 } else { 30 })
  }
} else {
  $expected = RemoteLength
}
if ($expected -gt 0) { Write-Output "Archive size on the server: $expected bytes ($([math]::Round($expected / 1GB, 2)) GB)" }

function LocalLength {
  if (Test-Path -LiteralPath $archive) { return [int64](Get-Item -LiteralPath $archive).Length }
  return [int64]0
}

# Returns '' when the archive is good, else the reason.
function Verify {
  if ($Sha256 -ne '') {
    $h = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($h -ne $Sha256) { return "SHA-256 mismatch (got $h)" }
    Write-Output "SHA-256 OK: $Sha256" | Out-Host
    return ''
  }
  $size = LocalLength
  if ($expected -gt 0 -and $size -ne $expected) { return "size $size differs from the server's $expected" }
  Write-Output "No SHA-256 given: size matches the server ($size bytes); listing the whole archive..." | Out-Host
  $count = 0
  & tar.exe -tf $archive | ForEach-Object { $count++ }
  if ($LASTEXITCODE -ne 0) { return "tar.exe cannot read the whole archive (exit $LASTEXITCODE)" }
  Write-Output "Archive lists $count entries" | Out-Host
  return ''
}

$attempt = 0
$verified = $false
if ((LocalLength) -gt 0 -and ($Sha256 -ne '' -or (LocalLength) -eq $expected)) {
  $reason = Verify
  if ($reason -eq '') { Write-Output 'Archive already complete and verified, skipping the download.'; $verified = $true }
}
$lastReport = Get-Date
while (-not $verified) {
  $attempt++
  Write-Output "Download attempt $attempt ($(Get-Date -Format 'HH:mm:ss'), $([math]::Round((LocalLength) / 1MB)) MB on disk)"
  & curl.exe @curlArgs
  $code = $LASTEXITCODE
  if ($code -eq 0 -or ($expected -gt 0 -and (LocalLength) -eq $expected)) {
    Write-Output 'Download finished, verifying...'
    $reason = Verify
    if ($reason -eq '') { $verified = $true; break }
    Fail "$reason. The archive is kept at $archive; rename it to download again."
  }
  if ($code -eq 33) { Fail 'the server does not support resuming (HTTP range requests); serve the archive with a server that does' }
  $hours = ((Get-Date) - $started).TotalHours
  if ($hours -ge $MaxHours) { Fail "gave up after $([math]::Round($hours, 1)) h; run the script again to resume" 2 }
  Write-Output "curl stopped (exit $code) with $([math]::Round((LocalLength) / 1MB)) MB so far; server away? retrying in $PauseSeconds s"
  Start-Sleep -Seconds $PauseSeconds
}
Write-Output "Download done in $([int]((Get-Date) - $started).TotalMinutes) min"

# The archive may hold Content\... or GranTheftPraline\Content\...
$first = (& tar.exe -tf $archive | Select-Object -First 1)
$extractTo = $Dest
if ($first -match '^(\./)?GranTheftPraline/') { $extractTo = Split-Path -Parent $Dest }
Write-Output "Extracting into $extractTo (first entry: $first) ..."
& tar.exe -xf $archive -C $extractTo
if ($LASTEXITCODE -ne 0) { Fail "tar.exe failed (exit $LASTEXITCODE); the archive is kept at $archive" }

$content = Join-Path $Dest 'Content'
$files = @(Get-ChildItem -LiteralPath $content -Recurse -File -ErrorAction SilentlyContinue)
$bytes = ($files | Measure-Object -Property Length -Sum).Sum
if (-not $bytes) { $bytes = 0 }
Write-Output ("Content: {0} files, {1} GB" -f $files.Count, ($bytes / 1GB).ToString('0.00', [Globalization.CultureInfo]::InvariantCulture))
if (-not (Test-Path -LiteralPath $mapCheck)) { Fail "extracted, but the city map is missing: $mapCheck" }
Write-Output "City map found: $mapCheck"
Write-Output "Done in $([int]((Get-Date) - $started).TotalMinutes) min. Archive kept at $archive"
exit 0
