# Publie une nouvelle version d'Aurora Launcher.
#
# Exemple :
#   .\tools\publish_update.ps1 -Version 2.0.1 -Notes "Correction du lancement" -Push
#
# Le script :
#   1. vérifie que src/version.h et le numéro demandé correspondent ;
#   2. copie les exécutables dans dist/ ;
#   3. calcule taille et SHA-256 ;
#   4. écrit update-version.json à la racine du dépôt ;
#   5. crée le commit et, avec -Push, le pousse sur GitHub.
#
# Le lanceur télécharge ensuite les fichiers via raw.githubusercontent.com et
# vérifie le SHA-256 avant de lancer AuroraUpdater.exe.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$Notes,

    [string]$Launcher = "build\AuroraLauncher.exe",
    [string]$Updater = "build\AuroraUpdater.exe",
    [string]$Repo = "xenoxre/Aurora-laucnher",
    [string]$Branch = "main",
    [switch]$Push
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if ($Version -notmatch '^\d+(\.\d+)*(-[0-9A-Za-z.-]+)?$') {
    throw "Version invalide : '$Version'. Exemple attendu : 2.0.1"
}

$versionHeader = Get-Content "src\version.h" -Raw
$match = [regex]::Match($versionHeader, 'AURORA_VERSION\s+"([^"]+)"')
if (-not $match.Success) {
    throw "Impossible de lire AURORA_VERSION dans src\version.h"
}
$sourceVersion = $match.Groups[1].Value
if ($sourceVersion -ne $Version) {
    throw "src/version.h indique $sourceVersion, pas $Version. Modifiez la version et reconstruisez avant de publier."
}

$cmakeText = Get-Content "CMakeLists.txt" -Raw
$cmakeVersions = @([regex]::Matches($cmakeText, 'AURORA_VERSION="([^"]+)"') |
    ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
if ($cmakeVersions.Length -ne 1 -or $cmakeVersions[0] -ne $Version) {
    throw ('CMakeLists.txt doit definir AURORA_VERSION="' + $Version + '" pour toutes les cibles.')
}

foreach ($file in @($Launcher, $Updater)) {
    if (-not (Test-Path $file)) {
        throw "Fichier introuvable : $file (compilez d'abord)."
    }
}

$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Copy-Item $Launcher (Join-Path $dist "AuroraLauncher.exe") -Force
Copy-Item $Updater  (Join-Path $dist "AuroraUpdater.exe")  -Force

function Get-Asset([string]$path, [string]$name) {
    $item = Get-Item $path
    $hash = (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()
    return [ordered]@{
        url    = "https://raw.githubusercontent.com/$Repo/$Branch/dist/$name`?v=$Version"
        size   = [long]$item.Length
        sha256 = $hash
    }
}

$manifest = [ordered]@{
    schema      = 1
    version     = $Version
    channel     = "stable"
    publishedAt = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd")
    notes       = $Notes
    launcher    = Get-Asset (Join-Path $dist "AuroraLauncher.exe") "AuroraLauncher.exe"
    updater     = Get-Asset (Join-Path $dist "AuroraUpdater.exe") "AuroraUpdater.exe"
}

$json = $manifest | ConvertTo-Json -Depth 6
$manifestPath = Join-Path $root "update-version.json"
[IO.File]::WriteAllText($manifestPath, $json + "`n", (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Manifeste écrit : $manifestPath"
Write-Host ("Lanceur  : {0:N0} octets  SHA-256 {1}" -f $manifest.launcher.size, $manifest.launcher.sha256)
Write-Host ("Updater  : {0:N0} octets  SHA-256 {1}" -f $manifest.updater.size, $manifest.updater.sha256)

git add -A
git commit -m "Version $Version"

if ($Push) {
    git push origin $Branch
    Write-Host "Poussé sur $Repo ($Branch)."
} else {
    Write-Host "Commit local créé. Relancez avec -Push pour publier."
}
