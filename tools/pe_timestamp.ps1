# Dump the PE TimeDateStamp of an executable — this is the "gameBuild" id the
# self-update client sends (GameCompat::GameTimeDateStamp). Prints it as the
# 0xXXXXXXXX hex string used as the index.json key.
#   powershell -ExecutionPolicy Bypass -File tools\pe_timestamp.ps1 -Exe "C:\path\to\Game.exe"
param([Parameter(Mandatory=$true)][string]$Exe)

if (!(Test-Path $Exe)) { Write-Error "not found: $Exe"; exit 1 }

$bytes = [System.IO.File]::ReadAllBytes($Exe)
# e_lfanew at 0x3C -> offset of PE header. FileHeader starts at PE+4,
# TimeDateStamp is at FileHeader+4 => PE + 8.
$peOff = [BitConverter]::ToInt32($bytes, 0x3C)
if ($bytes[$peOff] -ne 0x50 -or $bytes[$peOff+1] -ne 0x45) {
  Write-Error "bad PE signature"; exit 1
}
$tds = [BitConverter]::ToUInt32($bytes, $peOff + 8)
$hex = "0x{0:X8}" -f $tds
Write-Host $hex
