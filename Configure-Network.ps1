[CmdletBinding()]
param([string]$GameRoot,[string]$PeerAddress,[switch]$CheckOnly,[switch]$Remove)
$ErrorActionPreference='Stop'
if(!$GameRoot){$GameRoot=(Read-Host 'Folder containing Fable3.exe').Trim('"')}
if(!$PeerAddress){$PeerAddress=Read-Host "Trusted friend's reachable IPv4 address"}
$root=(Resolve-Path -LiteralPath $GameRoot).Path
$exe=Join-Path $root 'Fable3.exe'
$expected=(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json).gameSha256
if((Get-FileHash -LiteralPath $exe).Hash -ne $expected){throw 'Unsupported game executable.'}
$ip=$null
if($PeerAddress -notmatch '^\d{1,3}(\.\d{1,3}){3}$' -or ![Net.IPAddress]::TryParse($PeerAddress,[ref]$ip) -or $ip.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork){throw 'Enter one IPv4 address, not a subnet or address range.'}
$bytes=$ip.GetAddressBytes()
if($bytes[0] -eq 0 -or $bytes[0] -eq 127 -or $bytes[0] -ge 224){throw "Enter the other PC's unicast address."}
$sha=[Security.Cryptography.SHA256]::Create()
try{$tag=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($root.ToLowerInvariant())))).Replace('-','').Substring(0,10)}finally{$sha.Dispose()}
$rule="Fable3Coop-$tag-$($ip.ToString().Replace('.','-'))"
Write-Output "Rule: $rule; program: $exe; inbound UDP 39000-39001; remote: $ip; all network profiles."
if($CheckOnly){return}
$principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Run Configure-Network.cmd as administrator to change this rule.'}
if($Remove){Get-NetFirewallRule -Name $rule -ErrorAction SilentlyContinue | Remove-NetFirewallRule;Write-Output "This helper's peer rule removed.";return}
$blocked=Get-NetFirewallApplicationFilter -Program $exe -ErrorAction SilentlyContinue | Get-NetFirewallRule | Where-Object {$_.Enabled -eq 'True' -and $_.Direction -eq 'Inbound' -and $_.Action -eq 'Block'}
if($blocked){$blocked | Select-Object Name,DisplayName | Format-Table | Out-Host;throw 'Existing inbound block rules for this game require review; an allow rule cannot override them. No rules changed.'}
Get-NetFirewallRule -Name $rule -ErrorAction SilentlyContinue | Remove-NetFirewallRule
New-NetFirewallRule -Name $rule -DisplayName "Fable III co-op: $ip" -Direction Inbound -Action Allow -Program $exe -Protocol UDP -LocalPort '39000-39001' -RemoteAddress $ip.ToString() -Profile Any | Out-Null
Write-Output "Trusted-peer rule installed. Repeat on the other PC using this PC's address."

