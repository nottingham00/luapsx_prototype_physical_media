param(
    [Parameter(Mandatory=$true)]
    [string]$CorePath
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dstDir = Join-Path $root "disc\LUAPSX\cores"
$dst = Join-Path $dstDir "pcsx_rearmed_libretro.so"

if (!(Test-Path -LiteralPath $CorePath -PathType Leaf)) {
    throw "Core not found: $CorePath"
}

New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
Copy-Item -LiteralPath $CorePath -Destination $dst -Force
Write-Host "Staged core: $dst"
