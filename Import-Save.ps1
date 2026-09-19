[CmdletBinding()]
param([string]$GameRoot,[string]$SourceFolder)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Profile-Id.ps1')
if(!$GameRoot){$GameRoot=(Read-Host 'Folder containing Fable3.exe').Trim('"')}
$config=Join-Path (Resolve-Path -LiteralPath $GameRoot).Path 'XLiveLessNess\xlln-config-1.ini'
$name=[regex]::Match((Get-Content -LiteralPath $config -Raw),'(?m)^\s*xlive_username_p1\s*=\s*([^\r\n]*)').Groups[1].Value.Trim()
$id=Get-FableProfileId $name
$saveRoot=Join-Path $env:USERPROFILE 'Saved Games\Lionhead Studios\Fable 3'
$destination=Join-Path $saveRoot $id
if(Test-Path -LiteralPath $destination){throw "This profile already has a save folder: $destination. Nothing overwritten."}
if(Get-Process Fable3,FableLauncher,F3Secu -ErrorAction SilentlyContinue){throw 'Close the game and launcher first.'}
if(!$SourceFolder){
    Get-ChildItem -LiteralPath $saveRoot -Directory | Select-Object Name,LastWriteTime | Format-Table | Out-Host
    $SourceFolder=(Read-Host 'Full path of the old save folder to copy (original will be kept)').Trim('"')
}
$source=(Resolve-Path -LiteralPath $SourceFolder).Path
if((Split-Path -Parent $source) -ne (Resolve-Path -LiteralPath $saveRoot).Path -or (Split-Path -Leaf $source) -notmatch '^[0-9A-Fa-f]{12,16}$'){throw 'Select a profile folder directly inside your Fable 3 Saved Games directory.'}
if((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked save folders are unsupported.'}
$entries=@(Get-ChildItem -LiteralPath $source -Recurse -Force)
if($entries | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Linked save entries are unsupported.'}
if(!(Get-ChildItem -LiteralPath $source -File -Filter '*mainsave.bin')){throw 'No Fable mainsave found in that folder.'}
Copy-Item -LiteralPath $source -Destination $destination -Recurse
foreach($file in $entries | Where-Object {!$_.PSIsContainer}){
    $relative=$file.FullName.Substring($source.Length+1)
    if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $destination $relative)).Hash){throw 'Copied save verification failed. Original save remains intact.'}
}
Write-Output "Copied and verified save for $name into $destination. Original preserved."
Write-Output 'Load the game and verify your progress. Some old save formats may require further migration.'
