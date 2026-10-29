# Fable III Co-op v2.5.1-controller.1 — Controller Support

The Sanctuary Friends and Invitations menu now supports XInput controllers. This beta update includes the exact DLL validated in-game, a ready-to-install package, and matching source.

## Newly supported

- **D-pad / left stick:** navigate friends, scroll pages, and move between available actions.
- **A / B:** select a friend or action, return to the friends list, and close the panel.
- **Visible focus and Xbox-style prompts:** see the selected control and available buttons.
- **Input isolation and held-button protection:** menu input stays out of gameplay, and opening/closing buttons must be released before they can act again.
- **Focus and reconnect recovery:** resume navigation after Alt+Tab or reconnecting the controller.
- **Controller kick confirmation:** defaults to Cancel, with directional selection and A/B confirmation or cancellation.

Keyboard and mouse remain supported. Entering new addresses and names still requires keyboard typing. Multiple addresses per friend are outside this update.

Existing friend requests, presence, invitations, joins, leaving, and host kicking remain included. The network protocol and join/invite/kick backend are unchanged.

## Download and install

Download **[Fable3-Coop-v2.5.1-controller.1.zip](https://github.com/LunalFresh/Fable3-Coop/releases/download/v2.5.1-controller.1/Fable3-Coop-v2.5.1-controller.1.zip)** from Assets, extract the complete ZIP, close Fable III and its launcher, and run **Setup.cmd**. A SHA-256 file is included as a separate download. No compilation is required.

For an upgrade, install into the same game folder and keep the backup path printed by Setup. Existing profiles, friends, and saves are preserved. Launch using Start-Fable3-LAN.bat and the original launcher.

## Validation and compatibility

The controller menu checklist passed user testing in Fable III, covering navigation, input isolation, held-input handling, and focus/reconnect recovery. All seven automated runtime checks and installer/rollback fixtures passed. See the [validation record](https://github.com/LunalFresh/Fable3-Coop/blob/v2.5.1-controller.1/VALIDATION.md).

Requires the supported **Fable3.exe 1.1.1.3 / Patch 2** checksum, matching game content, and reachable IPv4 over UDP 39000–39001 for two-player co-op. Tested on Windows 11. The [previous beta release](https://github.com/LunalFresh/Fable3-Coop/releases/tag/v2.5.0-beta.1) remains available.
