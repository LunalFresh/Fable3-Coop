# Controller support

Version: **2.5.1-controller.1**, based on published beta **2.5.0-beta.1** (commit `681785d`).

This update adds controller navigation to the existing Friends and Invitations panel. The user confirmed the controller menu checks passed in Fable III and accepted the update for sharing. The distributed DLL is the exact tested build; see [validation](../VALIDATION.md).

## Controls

- Open Friends and Invitations through the Sanctuary as usual.
- D-pad or left stick moves the selection. Up/down scroll through friends and pages.
- A selects a friend, then selects the highlighted action. Right also moves from a friend to its primary action. Use directional navigation to reach other enabled actions.
- B returns from an action to the friends list; B from the list closes the panel.
- Kick player opens a confirmation defaulting to Cancel. Left/right changes the choice, A confirms, and B cancels.
- New addresses and names still use keyboard typing. A controller can focus the fields and select Send request.
- Xbox-style A/B prompts apply to XInput controllers, including an existing DS4Windows Xbox mapping. No driver or mapping changes are required by this candidate.

Release buttons/sticks when opening or closing the panel. Held inputs are intentionally suppressed until neutral so that opening A cannot select an action and closing B cannot operate the game underneath. Mouse and keyboard remain available. If the XInput hook cannot attach, controller navigation stays disabled and the existing input methods remain usable.

## In-game validation

The user confirmed the initial controller checklist passed: opening the panel, D-pad/stick selection and A/B navigation, input isolation from the underlying game, normal gameplay after closing, held-button protection, Alt+Tab recovery, and controller unplug/reconnect recovery.

Controller actions use the existing menu handlers. The network protocol and join/invite/kick backend are unchanged. Multiple addresses per friend remain outside this update.

## Runtime identity and evidence

The game log should contain both of these messages after loading the candidate and using the controller:

```text
SocialController: controller.1 XInputGetState import attached
SocialController: title called the controller.1 XInput filter.
```

The second confirms the title actually reached the input filter; the first only confirms attachment. Neither by itself proves the menu behaves correctly. Check rendered behavior as well.

The adapter changes the running game's XInputGetState import in memory and restores it at overlay teardown. It does not patch the executable on disk. Joins still wait for the game's existing UI-close notification, and the native kick bridge is unchanged.

## Rollback

Close the game and launcher. Run the candidate's Uninstall.ps1 with the same GameRoot and the backup directory printed during installation:

```powershell
.\Uninstall.ps1 -GameRoot 'D:\Games\Fable 3' -BackupPath '<backup directory printed by Setup>'
```

It checks the receipt and hashes before restoring the prior runtime/launcher files. Existing saves, friends and profile settings are preserved. A fresh Setup of the original beta package is also possible, but the recorded rollback restores the exact pre-candidate files.
