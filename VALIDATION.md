# Version 2.5.1 validation

Version: **2.5.1**. Runtime baseline: version **2.5.0** (original commit `681785d`).

## In-game acceptance

After installing this runtime, the user confirmed the initial controller checklist passed in Fable III:

- Opening Friends and Invitations through the Sanctuary.
- D-pad/left-stick selection, visible focus, and A/B navigation.
- Controller input isolated from the game behind the panel.
- Normal movement, camera, and buttons returning after closing and releasing the controls.
- Held opening input not automatically activating an action.
- Alt+Tab and controller unplug/reconnect recovery after releasing the controls.

The user accepted this version for release. These are user-reported gameplay results. The distributed `xlive.dll` is the exact installed and tested build, with SHA-256 `39627848100CBE9E5C60692EA019DC2C86F93CA3E5E252204CF79BA23051B5E0`; it was not rebuilt for the release.

## Automated checks

All six tests passed on a fresh export of the untouched 2.5.0 runtime. All seven 2.5.1 tests passed, including after a fresh rebuild from the exported package: social-protocol, friend-requests, network-instance-id, social-overlay, social-notify, social-kick, and social-controller.

The controller fixture exercises named/ordinal import interception and restoration, passthrough/error handling, all-slot capture, packet transitions, held-input release gating, directional repeat/deadzones, focus loss, and disconnect/reconnect behavior. The Direct3D 9 fixture checks rendered focus/prompts, input handling, selection/paging, disabled actions, existing backend dispatch, deferred invitation acceptance, kick confirmation/cancellation, and keyboard fallback. Fixture images were visually inspected; they are separate from the user's gameplay observations.

The runtime was clean-built with Visual Studio 2022 v143, Win32 Debug, Windows SDK 10.0.26100.0, the existing DirectX SDK, and cached Opus/RapidXML sources. All 262 export names/ordinals match 2.5.0. The Fable-specific module and launcher are byte-identical to 2.5.0.

## Installation and rollback

Windows PowerShell 5.1 fixtures passed fresh/repeat setup, actual 2.5.0 DLL upgrade and byte-exact rollback, profile/friend preservation, modified-configuration preservation, launcher preflight, invalid name/build/payload rejection, and isolated save-import checks. Firewall validation used CheckOnly. The installer itself is unchanged.

The tested runtime was installed locally from the extracted package with a complete backup receipt and verified runtime hash. Existing friends, profile, module, and launcher were preserved. The original 2.5.0 ZIP and pre-update installed files remain backed up locally.

## Evidence scope

Version 2.5.0's two-PC co-op and friend-request results are retained in [its validation record](docs/VALIDATION-2.5.0.md). Version 2.5.1's gameplay testing covers controller menu behavior. Controller actions use the existing handlers; the network protocol and join/invite/kick backend are unchanged.

New names and addresses still require keyboard typing. Each friend entry stores one address. Supported game-build and platform requirements remain as documented in the README.
