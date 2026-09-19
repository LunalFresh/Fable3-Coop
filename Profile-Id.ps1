function Get-FableProfileId([string]$Name){
    if($Name -notmatch '^[A-Za-z0-9][A-Za-z0-9 _-]{0,14}$'){throw 'Unsupported player-name characters.'}
    [uint64]$crc=4294967295
    foreach($byte in [Text.Encoding]::ASCII.GetBytes($Name)){
        $crc=$crc -bxor $byte
        for($bit=0;$bit -lt 8;$bit++){
            if($crc -band 1){$crc=($crc -shr 1) -bxor [uint64]3988292384}else{$crc=$crc -shr 1}
        }
    }
    $crc=$crc -bxor [uint64]4294967295
    # XLLNLogin CRC32(name) + XUID_LIVE_ENABLED_FLAG, online, guest zero.
    return ([uint64]2533274790395904+$crc).ToString('X')
}
