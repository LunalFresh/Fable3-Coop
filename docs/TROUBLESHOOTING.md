# Troubleshooting

[Back to the overview](../README.md)

| Symptom | What to check |
| :--- | :--- |
| Setup rejects the executable | Compare its SHA-256 with the [supported build](../README.md#before-you-install). Setup intentionally refuses other builds. |
| Friend stays offline | Both games/profiles must be running and online. Check the peer address, network route, and UDP firewall access on both PCs. |
| Request says queued | The request has not been acknowledged by the peer. There is no hosted offline inbox; the sender must be running when the reachable recipient receives it. |
| Request says declined after restarting | Expected on the sender's side. Select **Remove** to clear it, or send a fresh request. It is not a pending or accepted friendship. |
| A new request cannot be sent immediately | There is a request cooldown. Wait about a minute; also check that the peer is not blocked and a request is not already pending. |
| Join returns to the main menu and stays there | Returning there briefly is normal; remaining there is a failure. Confirm the host has loaded a save, keep the host game active, and check UDP access and matching game/mod versions. Record both sides' logs around the attempt. |
| Friends menu closes after Join | The game must consume the menu-close event before joining. Watch for the connection and host-world load; closing alone does not mean success. |
| Save is missing | Preserve the original profile name and capitalization. See the [save-import instructions](INSTALLATION.md#existing-saves). |
| Setup says to close the game | Close Fable III, its launcher, and the activation launcher process normally, then retry. |

## Friend requests and game invitations are different

A **friend request** establishes a saved friendship: send, accept, decline, or cancel. Accepting it keeps you in the menu. A **game invitation** asks an existing friend to join the current session: accepting starts the game join process.

Canceling a pending friend request removes it from the recipient. Declining removes it from the recipient's list and leaves a declined status for the sender. Accepted friends remain across restarts; presence and joinability depend on their current game state.

## Diagnostic information

The mod's seven automated checks and gameplay coverage are documented in [VALIDATION.md](../VALIDATION.md). For a new failure, report the mod version, game checksum, host/guest direction, last working action, and exact visible result. When logging is enabled, XLLN logs are under the game's `XLiveLessNess/logs` directory. Share only a relevant excerpt and remove names, addresses, and other personal information.
