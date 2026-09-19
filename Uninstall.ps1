[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$GameRoot,[Parameter(Mandatory=$true)][string]$BackupPath)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $GameRoot).Path
$backup=(Resolve-Path -LiteralPath $BackupPath).Path
$info=Get-Content -LiteralPath (Join-Path $backup 'receipt.json') -Raw | ConvertFrom-Json
if($info.gameRoot -ne $root -or !$info.complete){throw 'This is not a completed installation receipt for the selected game folder.'}
if(Get-Process Fable3,FableLauncher,F3Secu -ErrorAction SilentlyContinue){throw 'Close the game and launcher first.'}
$allowed=@('xlive.dll','XLiveLessNess\modules\xlln-fable-3.dll','Start-Fable3-LAN.bat','XLiveLessNess\xlln-config-1.ini')
$preserve=@()
foreach($record in $info.files){
    if($record.path -notin $allowed){throw 'Unexpected path in receipt.'}
    $dest=Join-Path $root $record.path
    $walk=$dest
    while($walk -and $walk -ne $root){
        if((Test-Path -LiteralPath $walk) -and ((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Linked destination is unsupported.'}
        $walk=Split-Path -Parent $walk
    }
    if($record.existed -and (Get-FileHash -LiteralPath (Join-Path $backup ('original\'+$record.path))).Hash -ne $record.originalHash){throw 'Backup checksum mismatch.'}
    if(!(Test-Path -LiteralPath $dest -PathType Leaf)){throw 'Installed file missing; refusing a partial restore.'}
    if((Get-FileHash -LiteralPath $dest).Hash -ne $record.installedHash){
        if($record.path -eq $allowed[3]){$preserve+=$record.path}
        else{throw 'An installed binary/launcher has changed since this backup. Nothing restored.'}
    }
}
foreach($record in $info.files){
    if($record.path -in $preserve){Write-Output 'Preserving your modified profile configuration.';continue}
    $dest=Join-Path $root $record.path
    if($record.existed){
        Copy-Item -LiteralPath (Join-Path $backup ('original\'+$record.path)) -Destination $dest -Force
        if((Get-FileHash -LiteralPath $dest).Hash -ne $record.originalHash){throw 'Restored file verification failed.'}
    }else{Remove-Item -LiteralPath $dest}
}
Write-Output 'Original installation files restored. Saves, friends and profile data preserved.'
