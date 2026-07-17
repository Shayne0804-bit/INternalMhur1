# Publish the freshly built Agency DLL as the server update payload.
# Called by the PublishAgencyUpdate MSBuild target AFTER a successful build.
#   - copies the built DLL to server\updates\RUGIR.dll
#   - rewrites server\updates\manifest.json with the current version.txt value
# The server serves this DLL + manifest to clients, so one Agency compile makes
# a ready-to-push update. (The server computes the SHA-256 live from the file.)
param(
  [Parameter(Mandatory=$true)][string]$Root,
  [Parameter(Mandatory=$true)][string]$Dll
)

$verFile  = Join-Path $Root 'version.txt'
$destDir  = Join-Path $Root 'server\updates'
$destDll  = Join-Path $destDir 'RUGIR.dll'
$manifest = Join-Path $destDir 'manifest.json'

if (Test-Path $verFile) { $v = (Get-Content $verFile -Raw).Trim() } else { $v = '1.0.0' }

if (!(Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }

Copy-Item -Path $Dll -Destination $destDll -Force
Set-Content -Path $manifest -Value "{`n  `"version`": `"$v`"`n}" -Encoding ascii

Write-Host "[publish] $Dll -> $destDll (version $v)"
