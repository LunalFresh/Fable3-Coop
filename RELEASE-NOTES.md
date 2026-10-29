# Fable III Co-op — v2.5.1-controller.1

This update adds XInput controller support to the Sanctuary Friends and Invitations menu: D-pad/left-stick navigation, A/B selection and back, visible focus, and controller prompts. Input is isolated from the game while the panel is open, with held-button protection and focus/reconnect recovery. Keyboard and mouse remain available; new names and addresses still use keyboard typing.

The user confirmed the controller checklist passed in Fable III and accepted this build for sharing. The distributed DLL is the exact tested candidate. All seven automated runtime checks and the existing installer/rollback fixtures passed. See [validation](VALIDATION.md).

Download the [current repository ZIP](https://github.com/LunalFresh/Fable3-Coop/archive/refs/heads/main.zip), extract it completely, close the game, and run Setup.cmd. The repository includes the tested binary and matching source, so no build is required. Existing profiles, friends, and saves are preserved. The [previous beta release](https://github.com/LunalFresh/Fable3-Coop/releases/tag/v2.5.0-beta.1) remains available.

Compatibility remains Fable3.exe 1.1.1.3 / Patch 2 with the supported checksum, Windows, two players, and reachable IPv4 over UDP 39000–39001. The network protocol, join/invite/kick backend, Fable module, and launcher are unchanged. Multiple addresses per friend are not included in this update.
