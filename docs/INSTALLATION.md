# Installation & recovery

[Back to the overview](../README.md)

## Setup

Extract the whole release ZIP outside the game directory and run **Setup.cmd**. Do not run it inside the ZIP or copy only the payload directory. Enter the existing game folder when prompted. If Windows denies write access, rerun Setup.cmd as administrator using your normal Windows account.

Setup verifies the supported game checksum and all three runtime payload hashes before replacing files. It backs up existing installation targets under `Fable3-Coop-Backups` inside the game folder. It installs `xlive.dll`, `XLiveLessNess/modules/xlln-fable-3.dll`, and `Start-Fable3-LAN.bat`. It creates a configuration only when none exists. A new profile defaults to 60 FPS; existing settings are retained.

Keep the original game launcher. Start the installed `Start-Fable3-LAN.bat` and click Play as usual. The batch filename is retained for compatibility; connections may also cross networks when the peers are reachable.

## Existing saves

Keep the same XLLN player name, including capitalization, to retain its save identity. Setup does not delete or move saves. Saves from older GFWL/emulator profiles may have a different ID and may not appear automatically.

If needed, close the game and run **Import-Save.cmd**. Provide the game folder and select the old profile folder under `%USERPROFILE%/Saved Games/Lionhead Studios/Fable 3`. The helper copies the complete folder, verifies file hashes, preserves the original, and refuses to overwrite an existing destination. It does not decrypt or convert saves. Verify your progress in the game afterward; arbitrary older save formats have not all been tested.

## Upgrade

Close the game and launcher, extract the new package separately, and run its Setup.cmd against the same game folder. Existing friends and configuration remain intact. Keep the printed backup path and this extracted package. Both players should use the same beta.

## Uninstall / restore

With the game closed, open PowerShell in the extracted package and run, substituting your paths:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Uninstall.ps1 -GameRoot "C:\Games\Fable 3" -BackupPath "C:\Games\Fable 3\Fable3-Coop-Backups\YOUR-BACKUP"
```

This restores the backed-up installation files, or removes installed targets that did not previously exist. Saves and friends remain. A newly generated configuration is removed only if unchanged; a modified configuration is retained. The helper refuses to replace binaries changed since the recorded installation. Empty directories and backups can remain.

Restoring an older mod binary does not convert newer friend-request data to that version's format. Keep a separate copy of your current `friends.ini` before intentionally downgrading across workflow versions.

To remove the optional network rule, run `Configure-Network.ps1` as administrator with the same `-GameRoot` and `-PeerAddress`, plus `-Remove`. Uninstall does not remove unrelated firewall rules.
