# Controller update validation

Version: **2.5.1-controller.1**, based on **2.5.0-beta.1** (`681785d`).

## In-game acceptance

After installing the candidate, the user confirmed the initial controller checklist passed in Fable III:

- Opening Friends and Invitations through the Sanctuary.
- D-pad/left-stick selection, visible focus, and A/B navigation.
- Controller input isolated from the game behind the panel.
- Normal movement, camera, and buttons returning after closing and releasing the controls.
- Held opening input not automatically activating an action.
- Alt+Tab and controller unplug/reconnect recovery after releasing the controls.

The user accepted the controller update for sharing. These are user-reported gameplay results. The distributed `xlive.dll` is the exact installed and tested candidate, with SHA-256 `39627848100CBE9E5C60692EA019DC2C86F93CA3E5E252204CF79BA23051B5E0`; it was not rebuilt for this repository update.

## Automated checks

All six tests passed on a fresh export of the untouched beta. All seven candidate tests passed, including after a fresh rebuild from the exported candidate package: social-protocol, friend-requests, network-instance-id, social-overlay, social-notify, social-kick, and social-controller.

The controller fixture exercises named/ordinal import interception and restoration, passthrough/error handling, all-slot capture, packet transitions, held-input release gating, directional repeat/deadzones, focus loss, and disconnect/reconnect behavior. The Direct3D 9 fixture checks rendered focus/prompts, input handling, selection/paging, disabled actions, existing backend dispatch, deferred invitation acceptance, kick confirmation/cancellation, and keyboard fallback. Fixture images were visually inspected; they are separate from the user's gameplay observations.

The runtime was clean-built with Visual Studio 2022 v143, Win32 Debug, Windows SDK 10.0.26100.0, the existing DirectX SDK, and cached Opus/RapidXML sources. All 262 export names/ordinals match the beta. The Fable-specific module and launcher are byte-identical to the beta.

## Installation and rollback

Windows PowerShell 5.1 fixtures passed fresh/repeat setup, actual beta DLL upgrade and byte-exact rollback, profile/friend preservation, modified-configuration preservation, launcher preflight, invalid name/build/payload rejection, and isolated save-import checks. Firewall validation used CheckOnly. The installer itself is unchanged.

The candidate was installed locally from the extracted package with a complete backup receipt and verified runtime hash. Existing friends, profile, module, and launcher were preserved. The original beta ZIP and pre-update installed files remain backed up locally.

## Evidence scope

This update validates controller use in the menu. Its controller actions call the existing handlers; the network protocol and join/invite/kick backend are unchanged. The prior two-PC co-op results are retained in [the beta validation record](docs/VALIDATION-BETA.md). Two-PC gameplay results are recorded for the earlier beta; this update's gameplay acceptance covers the controller menu behavior above.

New names and addresses still require keyboard typing. Multiple addresses per friend are outside this update. Supported game-build and platform requirements remain as documented in the README.
