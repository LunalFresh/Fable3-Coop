[CmdletBinding()]
param([string]$GameRoot,[string]$PlayerName,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
if(!$GameRoot){$GameRoot=(Read-Host 'Folder containing Fable3.exe').Trim('"')}
$root=(Resolve-Path -LiteralPath $GameRoot).Path
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
if((Get-FileHash -LiteralPath (Join-Path $root 'Fable3.exe')).Hash -ne $manifest.gameSha256){throw 'Unsupported Fable3.exe. This package requires the verified 1.1.1.3 / Patch 2 executable.'}
if(!(Test-Path -LiteralPath (Join-Path $root 'FableLauncher.exe') -PathType Leaf)){throw 'FableLauncher.exe is missing. Keep the original launcher from your game installation.'}
$files=@('xlive.dll','XLiveLessNess\modules\xlln-fable-3.dll','Start-Fable3-LAN.bat')
$hashes=@($manifest.dllSha256,$manifest.moduleSha256,$manifest.launcherScriptSha256)
foreach($file in $files){
    $index=[array]::IndexOf($files,$file)
    if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot ('payload\'+$file))).Hash -ne $hashes[$index]){throw "Package checksum failed: $file. Extract the complete ZIP again."}
    $walk=Join-Path $root $file
    if(Test-Path -LiteralPath $walk -PathType Container){throw "Expected a file, found a directory: $walk"}
    while($walk -and $walk -ne $root){
        if((Test-Path -LiteralPath $walk) -and ((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Linked install target is unsupported: $walk"}
        $walk=Split-Path -Parent $walk
    }
}
$configRelative='XLiveLessNess\xlln-config-1.ini'
$config=Join-Path $root $configRelative
if(Test-Path -LiteralPath $config -PathType Container){throw 'Expected a configuration file, found a directory.'}
$createConfig=!(Test-Path -LiteralPath $config)
if(!$createConfig){
    if((Get-Item -LiteralPath $config -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked configuration is unsupported.'}
    $existing=[regex]::Match((Get-Content -LiteralPath $config -Raw),'(?m)^\s*xlive_username_p1\s*=\s*([^\r\n]*)').Groups[1].Value.Trim()
    if($PlayerName -and $PlayerName -cne $existing){throw 'An existing profile configuration was found. Omit -PlayerName to preserve it; changing names also changes save identity.'}
    $PlayerName=$existing
} elseif(!$PlayerName -and !$CheckOnly){
    $PlayerName=Read-Host 'Your player name (1-15 characters; use a different name from your friends)'
}
if($createConfig -and !$CheckOnly -and ($PlayerName -notmatch '^[A-Za-z0-9][A-Za-z0-9 _-]{0,14}$' -or $PlayerName.EndsWith(' '))){throw 'Use 1-15 letters, numbers, spaces, underscores or hyphens; start with a letter/number and do not end with a space.'}
if($CheckOnly){Write-Output 'PASS: game build, original launcher, payload hashes and destination paths.';return}
if(Get-Process Fable3,FableLauncher,F3Secu -ErrorAction SilentlyContinue){throw 'Close Fable III and its launcher before setup.'}
# Only installation targets are backed up/replaced. Saves, friend lists and other
# mod/profile files never enter the distributable and are never removed here.
$targets=@($files)
if($createConfig){$targets+=$configRelative}
$same=!$createConfig
for($i=0;$i -lt $files.Count;$i++){
    $path=Join-Path $root $files[$i]
    if(!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $hashes[$i]){$same=$false}
}
if($same){Write-Output "Already installed and verified. Existing profile/configuration preserved: $PlayerName";return}
$backupBase=Join-Path $root 'Fable3-Coop-Backups'
if((Test-Path -LiteralPath $backupBase) -and ((Get-Item -LiteralPath $backupBase -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Linked backup directory is unsupported.'}
$backup=Join-Path $backupBase (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
New-Item -ItemType Directory -Path (Join-Path $backup 'original') -Force | Out-Null
$records=@()
foreach($file in $targets){
    $path=Join-Path $root $file
    $existed=Test-Path -LiteralPath $path -PathType Leaf
    $oldHash=$null
    if($existed){
        $oldHash=(Get-FileHash -LiteralPath $path).Hash
        $copy=Join-Path $backup ('original\'+$file)
        New-Item -ItemType Directory -Path (Split-Path -Parent $copy) -Force | Out-Null
        Copy-Item -LiteralPath $path -Destination $copy
        if((Get-FileHash -LiteralPath $copy).Hash -ne $oldHash){throw 'Backup verification failed.'}
    }
    $records+=@{path=$file;existed=$existed;originalHash=$oldHash;installedHash=$null}
}
$receipt=@{gameRoot=$root;version=$manifest.version;profile=$PlayerName;files=$records;complete=$false}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $backup 'receipt.json') -Encoding UTF8
try{
    for($i=0;$i -lt $files.Count;$i++){
        $dest=Join-Path $root $files[$i]
        New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('payload\'+$files[$i])) -Destination $dest -Force
        if((Get-FileHash -LiteralPath $dest).Hash -ne $hashes[$i]){throw 'Installed payload verification failed.'}
        $records[$i].installedHash=$hashes[$i]
    }
    if($createConfig){
        @"
# Fable III co-op configuration. Keep this name to retain your save identity.
xlln_debug_log_level = 0x0000c73e
xlive_hotkey_id_guide = 0x0024
xlive_fps_limit = 60
xlln_network_instance_base_port = 39000
xlln_network_instance_port = 39001
xlive_net_disable = 0
xlive_xhv_engine_enabled = 1
xlive_xhv_engine_voice_encoded_bitrate = 2400
xlive_specific_network_adapter =
xlive_ignore_title_network_adapter = 1
xlive_broadcast_address =
xlln_direct_ip_connect_password =
xlln_direct_ip_connect_ip_port =
xlive_username_p1 = $PlayerName
xlive_user_live_enabled_p1 = 1
xlive_user_online_enabled_p1 = 1
xlive_user_auto_login_p1 = 1
xlive_user_auto_login_p2 = 0
xlive_user_auto_login_p3 = 0
xlive_user_auto_login_p4 = 0
"@ | Set-Content -LiteralPath $config -Encoding ASCII
        $records[-1].installedHash=(Get-FileHash -LiteralPath $config).Hash
    }
    $receipt.complete=$true
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $backup 'receipt.json') -Encoding UTF8
}catch{
    # Fixed allowlisted files only; never recursively delete a directory.
    foreach($record in $records){
        $dest=Join-Path $root $record.path
        if($record.existed){Copy-Item -LiteralPath (Join-Path $backup ('original\'+$record.path)) -Destination $dest -Force}
        elseif(Test-Path -LiteralPath $dest -PathType Leaf){Remove-Item -LiteralPath $dest -Force}
    }
    throw
}
Write-Output "Installed and verified Fable III co-op. Player: $PlayerName"
Write-Output "Backup / uninstall receipt: $backup"
Write-Output 'Start with Start-Fable3-LAN.bat in your game folder. Configure trusted-peer firewall access as described in README.md.'
Write-Output 'Existing saves are preserved. If an older save is not listed, use Import-Save.ps1; do not overwrite a save folder.'
