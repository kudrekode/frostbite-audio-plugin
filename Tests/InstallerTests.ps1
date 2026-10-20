[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$fixture=Join-Path $project ('build\qa\installer-'+[Guid]::NewGuid().ToString('N'))
$scan=Join-Path $fixture 'Programs\Common\VST3'
$target=Join-Path $scan 'FROSTBITE.vst3'
$backups=Join-Path $fixture 'Frostbite\Plugin Backups'
$source=Join-Path $project 'build\Frostbite_artefacts\Release\VST3\FROSTBITE.vst3'
$installer=Join-Path $project 'scripts\install.ps1'
$previousLocalAppData=$env:LOCALAPPDATA
try {
    # Redirect this test process's user-install location to an isolated workspace fixture.
    $env:LOCALAPPDATA=$fixture
    & $installer -Scope User -SourceBundle $source -WhatIf
    if(Test-Path -LiteralPath $target) { throw 'WhatIf wrote an installation.' }
    & $installer -Scope User -SourceBundle $source
    if(!(Test-Path -LiteralPath (Join-Path $target '.frostbite-install'))) { throw 'Install marker missing.' }
    $legacy=Join-Path $scan 'FROSTBITE.backup-legacy'
    Copy-Item -LiteralPath $target -Destination $legacy -Recurse
    $unmarked=Join-Path $scan 'FROSTBITE.backup-unmarked'
    New-Item -ItemType Directory -Path $unmarked | Out-Null
    Set-Content -LiteralPath (Join-Path $unmarked 'preserve.txt') -Value 'unrelated content'
    & $installer -Scope User -SourceBundle $source -Replace
    if(Test-Path -LiteralPath $legacy) { throw 'Legacy marked backup remains in scan directory.' }
    if(!(Test-Path -LiteralPath (Join-Path $unmarked 'preserve.txt'))) { throw 'Unmarked directory was modified.' }
    $archives=@(Get-ChildItem -LiteralPath $backups -Directory)
    if($archives.Count -ne 2) { throw 'Replacement and migrated backup were not both preserved.' }
    foreach($archive in $archives) {
        if(!(Test-Path -LiteralPath (Join-Path $archive.FullName 'Contents\x86_64-win\FROSTBITE.vst3'))) { throw 'Backup bundle incomplete.' }
    }
    $scanned=@(Get-ChildItem -LiteralPath $scan -Recurse -File -Filter '*.vst3')
    if($scanned.Count -ne 1) { throw 'VST3 scan directory contains duplicate binaries.' }
    $binary='Contents\x86_64-win\FROSTBITE.vst3'
    if((Get-FileHash -LiteralPath (Join-Path $source $binary)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $target $binary)).Hash) {
        throw 'Installed binary hash mismatch.'
    }
    Write-Host 'PASS installer: WhatIf, complete copy/hash, backup outside scan tree, legacy marked-backup migration, unmarked directory preserved, single scanned binary.'
} finally { $env:LOCALAPPDATA=$previousLocalAppData }
