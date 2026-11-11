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

  Usage: powershell -ExecutionPolicy Bypass -File tools\build\fetch-content-win.ps1 -Url http://<mac-ip>:<port>/Content.tar -Sha256 <hex> [-Dest <dir>] [-MaxHours 12] [-DryRun]
    -Url          archive URL served by the Mac
    -Sha256       expected SHA-256 of the archive (64 hex characters)
    -Dest         extraction folder (default: <clone>\GranTheftPraline; the
                  archive holds Content\...)
    -TransferDir  download folder (default: transfer\, next to the clone)
    -MaxHours     give up after this many hours of retries (default 12)
    -PauseSeconds wait between two curl runs when the server is away (default 60)
    -MinFreeGB    free space required on the download drive (default 70)
    -DryRun       resolve paths, check disk space, print the commands, run nothing

  Exit code: 0 done, 1 error, 2 gave up (time limit).
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$Url,
  [Parameter(Mandatory = $true)][string]$Sha256,
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
if ($Sha256 -notmatch '^[0-9a-f]{64}$') { Fail 'Sha256 must be 64 hexadecimal characters' }
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
$curlArgs = @('-f', '-L', '-C', '-', '--retry', '20', '--retry-delay', '30', '--retry-all-errors',
  '--connect-timeout', '20', '--speed-limit', '1024', '--speed-time', '120', '-o', $archive, $Url)

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

function HashOk {
  if (-not (Test-Path -LiteralPath $archive)) { return $false }
  $h = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
  return ($h -eq $Sha256)
}

$started = Get-Date
$attempt = 0
$verified = $false
if ($partial -gt 0 -and (HashOk)) {
  Write-Output 'Archive already complete and verified, skipping the download.'
  $verified = $true
}
while (-not $verified) {
  $attempt++
  Write-Output "Download attempt $attempt ($(Get-Date -Format 'HH:mm:ss'))"
  & curl.exe @curlArgs
  $code = $LASTEXITCODE
  if ($code -eq 0) {
    Write-Output 'Download finished, verifying SHA-256...'
    if (HashOk) { $verified = $true; break }
    $size = (Get-Item -LiteralPath $archive).Length
    Fail "SHA-256 mismatch for $archive ($size bytes). The archive is kept; check the hash given by the Mac, or rename the file to download it again."
  }
  if ($code -eq 33) { Fail 'the server does not support resuming (HTTP range requests); serve the archive with a server that does' }
  $hours = ((Get-Date) - $started).TotalHours
  if ($hours -ge $MaxHours) { Fail "gave up after $([math]::Round($hours, 1)) h; run the script again to resume" 2 }
  $have = 0
  if (Test-Path -LiteralPath $archive) { $have = (Get-Item -LiteralPath $archive).Length }
  Write-Output "curl stopped (exit $code) with $([math]::Round($have / 1MB)) MB so far; server away? retrying in $PauseSeconds s"
  Start-Sleep -Seconds $PauseSeconds
}
Write-Output "SHA-256 OK: $Sha256"

Write-Output "Extracting into $Dest ..."
& tar.exe -xf $archive -C $Dest
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
