[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$GameSource,
    [Parameter(Mandatory=$true)][string]$FixtureRoot,
    [string]$PackageRoot,
    [string]$UpgradeDll
)
$ErrorActionPreference='Stop'
if(!$PackageRoot){$PackageRoot=Split-Path -Parent $PSScriptRoot}
$game=(Resolve-Path -LiteralPath $GameSource).Path
$package=(Resolve-Path -LiteralPath $PackageRoot).Path
$fixture=Join-Path ([IO.Path]::GetFullPath($FixtureRoot)) ('installer-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $game 'Fable3.exe'),(Join-Path $game 'FableLauncher.exe') -Destination $fixture
function Assert($condition,$message){if(!$condition){throw $message}}
function MustFail([scriptblock]$action,$message){$failed=$false;try{& $action}catch{$failed=$true};Assert $failed $message}
function LatestBackup {return (Get-ChildItem (Join-Path $fixture 'Fable3-Coop-Backups') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName}
& (Join-Path $package 'Setup.ps1') -GameRoot $fixture -CheckOnly
Assert (!(Test-Path (Join-Path $fixture 'xlive.dll'))) 'Check-only wrote files'
MustFail {& (Join-Path $package 'Setup.ps1') -GameRoot $fixture -PlayerName 'bad=name'} 'Invalid name accepted'
& (Join-Path $package 'Setup.ps1') -GameRoot $fixture -PlayerName 'InstallerTest'
$freshBackup=LatestBackup
$manifest=Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
$receipt=Get-Content -LiteralPath (Join-Path $freshBackup 'receipt.json') -Raw | ConvertFrom-Json
Assert ($receipt.version -eq $manifest.version) 'Receipt version mismatch'
Assert ((Get-FileHash -LiteralPath (Join-Path $fixture 'xlive.dll')).Hash -eq $manifest.dllSha256) 'Wrong runtime installed'
Assert (Test-Path (Join-Path $fixture 'XLiveLessNess\modules\xlln-fable-3.dll')) 'Fresh module missing'
& (Join-Path $fixture 'Start-Fable3-LAN.bat') --check
Assert ($LASTEXITCODE -eq 0) 'Launcher check failed'
$config=Join-Path $fixture 'XLiveLessNess\xlln-config-1.ini'
$hash=(Get-FileHash $config).Hash
& (Join-Path $package 'Setup.ps1') -GameRoot $fixture
Assert ((Get-FileHash $config).Hash -eq $hash) 'Repeat install changed profile'
MustFail {& (Join-Path $package 'Setup.ps1') -GameRoot $fixture -PlayerName 'SomeoneElse'} 'Existing identity overwritten'
& (Join-Path $package 'Configure-Network.ps1') -GameRoot $fixture -PeerAddress '100.64.0.2' -CheckOnly
MustFail {& (Join-Path $package 'Configure-Network.ps1') -GameRoot $fixture -PeerAddress '0.0.0.0/0' -CheckOnly} 'Broad peer range accepted'
& (Join-Path $package 'Uninstall.ps1') -GameRoot $fixture -BackupPath $freshBackup
Assert (!(Test-Path (Join-Path $fixture 'xlive.dll'))) 'Fresh uninstall did not remove DLL'
Assert (!(Test-Path $config)) 'Fresh unmodified config not removed'
# Existing foreign shim and profile are backed up, then restored byte-for-byte.
if($UpgradeDll){Copy-Item -LiteralPath $UpgradeDll -Destination (Join-Path $fixture 'xlive.dll')}
else{Set-Content (Join-Path $fixture 'xlive.dll') 'original shim fixture' -Encoding ASCII}
Set-Content $config "xlive_username_p1 = ExistingPlayer`r`nxlive_fps_limit = 120" -Encoding ASCII
Set-Content (Join-Path $fixture 'XLiveLessNess\friends.ini') 'personal friends sentinel' -Encoding ASCII
$friendsHash=(Get-FileHash -LiteralPath (Join-Path $fixture 'XLiveLessNess\friends.ini')).Hash
$original=(Get-FileHash (Join-Path $fixture 'xlive.dll')).Hash
$profileHash=(Get-FileHash $config).Hash
& (Join-Path $package 'Setup.ps1') -GameRoot $fixture
$upgradeBackup=LatestBackup
Assert ((Get-FileHash $config).Hash -eq $profileHash) 'Existing config was changed'
& (Join-Path $package 'Uninstall.ps1') -GameRoot $fixture -BackupPath $upgradeBackup
Assert ((Get-FileHash (Join-Path $fixture 'xlive.dll')).Hash -eq $original) 'Original shim not restored'
Assert ((Get-FileHash $config).Hash -eq $profileHash) 'Rollback changed profile'
Assert ((Get-FileHash -LiteralPath (Join-Path $fixture 'XLiveLessNess\friends.ini')).Hash -eq $friendsHash) 'Friend data changed'
# A profile edited after a fresh install is retained by uninstall.
Remove-Item -LiteralPath $config
& (Join-Path $package 'Setup.ps1') -GameRoot $fixture -PlayerName 'ModifiedProfile'
$modifiedBackup=LatestBackup
Add-Content -LiteralPath $config -Value '# setting changed after setup'
$modifiedHash=(Get-FileHash -LiteralPath $config).Hash
& (Join-Path $package 'Uninstall.ps1') -GameRoot $fixture -BackupPath $modifiedBackup
Assert ((Get-FileHash -LiteralPath $config).Hash -eq $modifiedHash) 'Modified profile was not preserved'
# Import-Save runs against isolated fake profile/save files, never the real user profile.
$originalProfile=$env:USERPROFILE
try {
    $env:USERPROFILE=Join-Path $fixture 'test-user'
    $saveRoot=Join-Path $env:USERPROFILE 'Saved Games\Lionhead Studios\Fable 3'
    $oldSave=Join-Path $saveRoot '900000000001'
    New-Item -ItemType Directory -Path $oldSave -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $oldSave 'mainsave.bin') -Value 'save sentinel'
    $saveHash=(Get-FileHash -LiteralPath (Join-Path $oldSave 'mainsave.bin')).Hash
    & (Join-Path $package 'Import-Save.ps1') -GameRoot $fixture -SourceFolder $oldSave
    . (Join-Path $package 'Profile-Id.ps1')
    $newSave=Join-Path $saveRoot (Get-FableProfileId 'ModifiedProfile')
    Assert ((Get-FileHash -LiteralPath (Join-Path $newSave 'mainsave.bin')).Hash -eq $saveHash) 'Save copy mismatch'
    Assert ((Get-FileHash -LiteralPath (Join-Path $oldSave 'mainsave.bin')).Hash -eq $saveHash) 'Original save changed'
    MustFail {& (Join-Path $package 'Import-Save.ps1') -GameRoot $fixture -SourceFolder $oldSave} 'Existing save overwritten'
} finally {$env:USERPROFILE=$originalProfile}
# Corrupted payload is rejected before any game-folder mutation.
$badPackage=Join-Path $fixture 'bad-package'
New-Item -ItemType Directory -Path (Join-Path $badPackage 'payload') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $package 'Setup.ps1'),(Join-Path $package 'manifest.json') -Destination $badPackage
Set-Content -LiteralPath (Join-Path $badPackage 'payload\xlive.dll') -Value 'bad payload'
MustFail {& (Join-Path $badPackage 'Setup.ps1') -GameRoot $fixture -CheckOnly} 'Corrupt payload accepted'
Assert ((Get-FileHash -LiteralPath (Join-Path $fixture 'xlive.dll')).Hash -eq $original) 'Rejected payload changed original shim'
# Unsupported executable fails before replacing the original shim.
Set-Content (Join-Path $fixture 'Fable3.exe') 'wrong build fixture' -Encoding ASCII
MustFail {& (Join-Path $package 'Setup.ps1') -GameRoot $fixture} 'Unsupported executable accepted'
Assert ((Get-FileHash (Join-Path $fixture 'xlive.dll')).Hash -eq $original) 'Rejection changed original shim'
. (Join-Path $package 'Profile-Id.ps1')
Assert ((Get-FableProfileId 'aj') -eq '900009058C05F') 'CRC32 profile identity mismatch'
Write-Output "PASS: fresh setup, idempotence, profile preservation, invalid name/build rejection, launcher check, exact-peer rule plan, corrupt-payload rejection, fresh uninstall, old-shim restoration, modified-config preservation, and isolated save import/no-overwrite. Fixture: $fixture"
