<p align="center">
  <img src="docs/assets/banner.svg" alt="Fable III Co-op — Return to Albion, together" width="100%">
</p>

<p align="center"><strong>Two players. Your saves. One Albion.</strong><br>
Friends, invitations, and online co-op for the supported PC version of Fable III.</p>

<p align="center"><strong>v2.5.0</strong> · Windows · Two-player co-op · Powered by XLiveLessNess</p>

<p align="center">
  <a href="https://github.com/LunalFresh/Fable3-Coop/releases/tag/v2.5.0"><strong>Download v2.5.0</strong></a> ·
  <a href="#installation">Install</a> ·
  <a href="#play-together">Play together</a> ·
  <a href="docs/NETWORKING.md">Networking</a> ·
  <a href="docs/TROUBLESHOOTING.md">Troubleshooting</a> ·
  <a href="BUILDING.md">Build from source</a>
</p>

---

## Return to Albion, together

This community mod connects Fable III's co-op controls to an XLLN peer backend. Open the Friends and Invitations menu through the Sanctuary, send a friend request, and join each other's worlds using a menu styled to fit the game.

| Feature | What you can do |
| :--- | :--- |
| **Friends** | Send, accept, decline, or cancel requests. Accepted friends stay saved between launches. |
| **Join & invite** | See when a friend is online and joinable, join their world, or send a game invitation. |
| **Session controls** | Leave a session, remove a guest as host, and reconnect. |
| **Across networks** | Connect using reachable IPv4 addresses over UDP. No particular networking service is required. |
| **Reversible setup** | Install with checksum checks and backups while retaining existing profiles and settings. |

## Before you install

- Both players need their own working Fable III installation, matching game content/DLC, and this release on both PCs.
- Supported executable: **Fable3.exe 1.1.1.3 / Patch 2**, with the exact checksum below. Other builds are rejected by setup.
- Tested on Windows 11. Other Windows versions are not verified. Setup uses Windows PowerShell 5.1; the packaged runtime's Opus build enables AVX2.
- Each PC must be reachable by the other over **IPv4, UDP 39000–39001**. An address alone does not establish a network route or open a firewall.

<details>
<summary><strong>Supported executable checksum</strong></summary>

```text
AAC98798E77CA32282D663AF54FC185C3B5256B5317ED8568D7F12A0EF42DA42
```

Use `Get-FileHash .\Fable3.exe -Algorithm SHA256` in PowerShell to compare your file. Matching the executable does not check every game-data or DLC file.

</details>

## Installation

1. Download **[Fable3-Coop-v2.5.0.zip](https://github.com/LunalFresh/Fable3-Coop/releases/download/v2.5.0/Fable3-Coop-v2.5.0.zip)** from the release release. Use this prepared package; GitHub's automatically generated source archives are intended for development. Extract the **entire ZIP** into a separate folder, such as Downloads.
2. Close Fable III and its launcher. Run **Setup.cmd** from the extracted folder.
3. Enter the folder containing **Fable3.exe**. On a fresh installation, choose a player name different from your friend's. Existing XLLN profiles and settings are preserved.
4. If peer access is not already configured, follow the [network setup](docs/NETWORKING.md).
5. Launch **Start-Fable3-LAN.bat** from your game folder, then click **Play** in the original launcher.

Keep your player name's spelling and capitalization: it determines your local save identity. Setup does not include or modify game executables, activation files, DLC, or controller DLLs. See [installation, save import, and uninstall](docs/INSTALLATION.md) for details.

> **Upgrading?** Close the game and run the new package's Setup.cmd against the same folder. Keep the backup path printed by setup. You do not need to remove your friends or create a new profile.

## Play together

1. Both players launch the game and load their saves.
2. Open **Friends and Invitations** through the Sanctuary. One player enters the other's reachable **IPv4 address** and sends a **friend request**. A name is optional; a name alone cannot locate another PC.
3. The other player accepts the request. They do **not** need to add the sender's address manually.
4. Select a joinable friend and choose **Join**, or send a game **Invite** and accept it on the other PC.

Returning to the main menu briefly is part of joining; a successful join continues into the host's world. The host can use **Kick player** for the connected guest. **F6** opens an incoming request/invitation, **Esc** closes the panel, and **Home** opens the XLLN guide.

Declined requests remain labeled **Request declined** on the sender's side until removed or replaced with a new request. They are not accepted friends or pending requests. See [request states and connection help](docs/TROUBLESHOOTING.md).

## Supported play

The distributed DLLs have been tested on two PCs: joining and invitations with either player hosting; leave/kick; friend request acceptance, cancellation, decline and retry; friendship persistence after restarting; and remote play across separate networks, including a Brightwall-to-Mistpeak transition and leaving/rejoining. Six automated runtime checks passed. See [validation scope](VALIDATION.md).

This release supports **two-player, peer-hosted co-op**. It does not provide a public username directory, central matchmaking service, dedicated server, or hosted offline request inbox. Both games must eventually be running and reachable to exchange requests. Internet routing and firewall setup remain the players' responsibility.

## Support

When reporting an issue, include the mod version, executable checksum, Windows version, which player was hosting, and the exact action/result. Say whether the peers were on the same or separate networks. Include only the relevant log excerpt, with personal names and addresses removed. Please do not upload saves, game files, or your entire game folder.

## Credits & source

Built on **GlitchyScripts' XLiveLessNess and XLLN Modules**, with Fable III integration, the Sanctuary-style friends menu, and the peer friend-request workflow. Matching modified source is included under [source/](source/); build instructions are in [BUILDING.md](BUILDING.md).

The runtime projects retain their **LGPL-2.1** licenses; installer scripts use **MIT**. See [license overview](LICENSE.md) and [third-party notices](THIRD-PARTY-NOTICES.md) for component details. This is an independent community project, unaffiliated with Microsoft or Lionhead Studios.
