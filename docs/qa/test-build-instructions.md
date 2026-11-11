# Opening the test build (macOS and Windows)

*French version: [test-build-instructions.fr.md](test-build-instructions.fr.md).*

The test build is not signed by a paid developer account, so both systems
warn the first time. This is expected. Only open archives that Nico sent you
directly.

## 1. Download and verify

You receive one or more `GranTheftPraline-<version>-<system>[-partNofM].zip`
files and a `SHA256SUMS` file. If there are several parts, extract them all
**into the same folder**.

- macOS (Terminal, in the download folder): `shasum -a 256 -c SHA256SUMS`
- Windows (PowerShell, in the download folder):
  `Get-FileHash .\*.zip -Algorithm SHA256` and compare with `SHA256SUMS`.

Each line must match. If not, download again.

## 2. macOS (Gatekeeper)

1. Double-click the zip, then move `GranTheftPraline.app` to Applications or
   any folder.
2. Double-click it. macOS says it cannot verify the app: click **Done**.
3. Open **System Settings → Privacy & Security**, scroll to the message about
   GranTheftPraline and click **Open Anyway**, then confirm.
   (On older macOS: right-click the app, **Open**, **Open**.)
4. If the app is still blocked, run once in Terminal:
   `xattr -dr com.apple.quarantine /path/to/GranTheftPraline.app`.

Requirements: Apple silicon Mac, 16 GB of memory recommended, about 4 GB of
disk. First launch compiles shaders and can take a minute.

## 3. Windows (SmartScreen)

1. Extract the zip(s) (right-click, **Extract All**) to a folder such as
   `C:\Games\GranTheftPraline`.
2. Double-click `GranTheftPraline.exe`. If **Windows protected your PC**
   appears: click **More info**, then **Run anyway**.
3. If an antivirus quarantines the file, tell Nico; do not disable it.

Requirements: Windows 10 or 11 64-bit, a graphics card with 8 GB, 16 GB of
memory, about 6 GB of disk, an up-to-date graphics driver.

## 4. Where the logs and crash reports are

The test build writes everything locally and never sends anything.

| System | Folder |
| ---- | ---- |
| macOS | `~/Library/Application Support/Epic/GranTheftPraline/Saved/` (`Logs`, `Crashes`, `Profiling`) |
| Windows | `%LOCALAPPDATA%\GranTheftPraline\Saved\` (`Logs`, `Crashes`, `Profiling`) |

Press **F9** in game to export frame times, position and version to the
`Exports` folder next to them (see [test-build-spec.md](test-build-spec.md)).
When you report a bug, zip that folder and attach it.
