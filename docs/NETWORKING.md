# Connecting two PCs

[Back to the overview](../README.md)

Each player needs the other PC's **reachable IPv4 address**, with **UDP ports 39000–39001** available in both directions. The mod uses a peer connection; no particular networking provider is required.

On a local network, use the address of the other PC's active network adapter. Across separate networks, establish a route between the PCs first. Firewalls and router/NAT settings must allow the game traffic; the mod does not supply a relay, open router ports, or configure that route for you. Being able to ping an address alone does not prove that game UDP traffic is allowed.

## Optional Windows firewall helper

1. Right-click **Configure-Network.cmd** in the extracted package and run it as administrator.
2. Enter the game folder and the other PC's reachable IPv4 address.
3. Repeat on the other PC, using the address by which it reaches the first PC.

The helper allows inbound UDP 39000–39001 for the selected Fable3.exe from **that one peer**, across Windows network profiles. It accepts one address, not a subnet. It reports existing game-specific inbound block rules for review; it does not disable them. An allow rule does not override a block rule.

To inspect the planned rule without changing anything:

```powershell
.\Configure-Network.ps1 -GameRoot "C:\Games\Fable 3" -PeerAddress "192.168.1.20" -CheckOnly
```

The address above is an example: replace it with your friend's actual reachable address. Firewall setup and friend requests are separate steps. After the network is ready, only **one** player needs to send the friend request; the other accepts.

Saved peer entries are associated with an address. If that address changes, use the new reachable address. A display name is not a directory lookup. Prefer a consistent route and address for each friendship.
