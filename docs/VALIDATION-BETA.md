# Beta validation

Version: **2.5.0-beta.1**. Validation date: **2026-09-19**.

## Gameplay on the distributed binaries

Two Windows PCs used matching copies of the distributed runtime DLLs. The user confirmed:

- Direct joins and game invitations with either PC hosting.
- Leaving, host kicking, and joining again.
- Friend requests sent by one player and accepted by the other, with no manual reciprocal address entry.
- Cancellation removing the incoming request; decline visible to the sender; declined status preserved after restart; removal of the declined entry and a fresh accepted request.
- Accepted friendships remaining available after restarting both games.
- Remote co-op across separate networks, a Brightwall-to-Mistpeak transition, and the guest leaving/rejoining the host successfully.

The earlier same-second startup collision was reproduced from the old PRNG seed. On that old build, restarting only one peer separated the IDs and restored joining. The distributed fix uses Windows system randomness instead of that time-seeded ID path; subsequent gameplay checks above used the corrected binary. The required 24-bit ID space remains finite, so universal collision freedom is not claimed.

## Automated runtime checks

All **six** tests passed after rebuilding the test suite from this package's exported source: social-protocol, friend-requests, network-instance-id, social-overlay, social-notify, and social-kick. The overlay test renders through an isolated Direct3D 9 device; it needs desktop/GPU access. It could not create that device in the restricted sandbox and passed outside that sandbox. No game was launched or controlled by these tests.

Tests cover packet/state handling and rendering fixtures, not every possible gameplay or network condition.

## Installer checks

The beta installer passed under Windows PowerShell 5.1 in isolated game-folder fixtures using the supported executable and original launcher. Checks covered fresh installation, repeat installation, actual v2.4 DLL upgrade and byte-exact rollback, matching payload/receipt version, existing profile and friend-file preservation, modified-configuration preservation, invalid name/build rejection, corrupted-payload rejection, launcher --check, exact-peer firewall planning, fresh uninstall, and isolated save import with unchanged originals and no-overwrite rejection.

The game executable and launcher were used only in local validation fixtures and are not included in this release. Firewall checks used CheckOnly; no live firewall rules were changed. Save-import checks used fake data in an isolated profile directory.

## Scope and limits

The distributed runtime binaries are unchanged from the gameplay-tested pair. A fresh physical-PC installation of this exact beta package has not been launched into gameplay; installation is verified in fixtures, and the previous installer generation also passed a physical second-PC clean-install/rollback check. Other game builds, arbitrary older save formats, non-AVX2 CPUs, and all possible network/firewall policies remain unverified.

The packaging pass does not change the live game installations. Source, manifests, documentation, and release checksums accompany the runtime.
