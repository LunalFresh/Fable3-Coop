# Changelog

## 2.5.1

- Add XInput controller navigation to Friends and Invitations: D-pad/left stick, A/B actions, visible focus, and controller prompts.
- Isolate controller input while the panel is open and require neutral input after opening, closing, focus changes, or reconnection.
- Preserve keyboard/mouse input and use the existing menu action handlers, network protocol, and join/invite/kick backend.
- Include the exact runtime accepted after in-game controller testing, matching source, seven passing automated checks, and checksum-verified installation/rollback.
- Keep keyboard text entry for new names/addresses; multiple addresses per friend remain outside this update.

## 2.5.0

- Add a complete peer friend-request workflow: one-sided send, explicit acceptance, cancellation, decline, and persisted state across restarts.
- Show pending, delivered, declined, and confirmation states in the Sanctuary-style menu, with request notifications and block/unblock controls.
- Handle retries, acknowledgments, duplicate requests, and delayed packets without creating an accepted friendship before consent.
- Replace time-seeded virtual network-ID generation with the Windows system random generator, removing the deterministic collision when two games start in the same second. IDs retain the game's required nonzero 24-bit format; the finite namespace still allows rare random collisions.
- Preserve the tested join, invitation, leave, and host-kick paths.
- Package matching modified source as ordinary directories, update installation/networking guidance, and retain checksum-verified installation and rollback.

### Existing 2.4 foundation

Sanctuary-style friends menu, direct joins, game invitations, host kicking, standalone installation, profile preservation, optional per-peer firewall helper, and save-import helper.
