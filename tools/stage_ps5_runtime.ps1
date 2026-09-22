param(
    [Parameter(Mandatory=$true)][string]$EmulatorElf,
    [string]$HandoffElf
)
$Root = Split-Path -Parent $PSScriptRoot
$Runtime = Join-Path $Root "disc\LUAPSX"
$Autoload = Join-Path $Runtime "autoload"
New-Item -ItemType Directory -Force -Path $Runtime, $Autoload | Out-Null
Copy-Item -Force $EmulatorElf (Join-Path $Runtime "luapsx-ps5.elf")
Write-Host "staged emulator: disc\LUAPSX\luapsx-ps5.elf"
if ($HandoffElf) {
    Copy-Item -Force $HandoffElf (Join-Path $Autoload "luapsx-handoff.elf")
    Write-Host "staged handoff: disc\LUAPSX\autoload\luapsx-handoff.elf"
}
