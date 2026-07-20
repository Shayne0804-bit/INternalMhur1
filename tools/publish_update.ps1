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

# Short server-side changelog line shown in the client's download UI.
# One line, kept in release_notes.txt at the repo root. Empty/missing => no note.
$notesFile = Join-Path $Root 'release_notes.txt'
$notes = ''
if (Test-Path $notesFile) { $notes = (Get-Content $notesFile -Raw).Trim() }
$notesJson = $notes -replace '\\','\\' -replace '"','\"' -replace "`r`n",'\n' -replace "`n",'\n' -replace "`r",'\n'

if (!(Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }

Copy-Item -Path $Dll -Destination $destDll -Force
if ([string]::IsNullOrEmpty($notes)) {
  Set-Content -Path $manifest -Value "{`n  `"version`": `"$v`"`n}" -Encoding ascii
} else {
  Set-Content -Path $manifest -Value "{`n  `"version`": `"$v`",`n  `"notes`": `"$notesJson`"`n}" -Encoding ascii
}

Write-Host "[publish] $Dll -> $destDll (version $v, notes: '$notes')"
