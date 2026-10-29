# Fable III Co-op v2.5.1

A complete, ready-to-use release of Fable III Co-op for the supported PC game build.

## Features

- Two-player co-op with either player hosting, direct joins, invitations, and online/joinable presence.
- Saved friend requests with acceptance, cancellation, decline, removal, and blocking.
- Sanctuary-style Friends and Invitations menu with XInput controller, keyboard, and mouse support.
- D-pad/left-stick navigation, A/B selection and back, visible focus, and Xbox-style button prompts.
- Controller input isolation, held-button protection, and recovery after focus changes or reconnecting a controller.
- Leave, host kick with confirmation, and rejoin controls.
- Checksum-verified setup, installation backups, profile/friend/save preservation, rollback, and optional networking/save-import helpers.

## Download and install

Download **[Fable3-Coop-v2.5.1.zip](https://github.com/LunalFresh/Fable3-Coop/releases/download/v2.5.1/Fable3-Coop-v2.5.1.zip)** from Assets, extract the complete ZIP, close Fable III and its launcher, and run **Setup.cmd**. The package includes the tested binaries and matching source; no compilation is required. A SHA-256 file is provided alongside the ZIP.

Install over an existing version in the same game folder and retain the backup path printed by Setup. Existing friends, profiles, settings, and saves are preserved. Launch through Start-Fable3-LAN.bat and the original launcher.

## Compatibility and validation

Requires the supported **Fable3.exe 1.1.1.3 / Patch 2** checksum, matching game content and reachable IPv4 over UDP 39000–39001. Tested on Windows 11. Names and addresses use keyboard typing; each friend entry stores one address.

The distributed runtime is the exact build accepted after in-game controller testing. All seven automated runtime checks and installer/rollback fixtures passed. See the [validation record](https://github.com/LunalFresh/Fable3-Coop/blob/v2.5.1/VALIDATION.md), [complete mod guide](https://github.com/LunalFresh/Fable3-Coop/blob/v2.5.1/README.md), and [changelog](https://github.com/LunalFresh/Fable3-Coop/blob/v2.5.1/CHANGELOG.md).
