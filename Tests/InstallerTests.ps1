[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$qaRoot=[IO.Path]::GetFullPath((Join-Path $project 'build\qa'))
$fixture=[IO.Path]::GetFullPath((Join-Path $qaRoot ('installer-'+[Guid]::NewGuid().ToString('N'))))
if(!$fixture.StartsWith($qaRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Installer fixture escaped build/qa.'
}
foreach($ancestor in @((Join-Path $project 'build'),$qaRoot)) {
    if((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing linked installer fixture ancestor: $ancestor"
    }
}
if(Test-Path -LiteralPath $fixture) { throw 'Installer fixture must be new.' }
function Assert-FixturePath([string]$Path) {
    $resolved=[IO.Path]::GetFullPath($Path)
    if(!$resolved.StartsWith($fixture+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Installer test write escaped its fixture: $resolved"
    }
}
$scan=Join-Path $fixture 'Programs\Common\VST3'
$target=Join-Path $scan 'FROSTBITE.vst3'
$backups=Join-Path $fixture 'Frostbite\Plugin Backups'
$source=Join-Path $project 'build\Frostbite_artefacts\Release\VST3\FROSTBITE.vst3'
$installer=Join-Path $project 'scripts\install.ps1'
$helper=Join-Path $fixture 'Invoke-IsolatedInstaller.ps1'
foreach($writePath in @($scan,$target,$backups,$helper)) { Assert-FixturePath $writePath }
New-Item -ItemType Directory -Path $fixture | Out-Null

# Run the unchanged production installer in a child process: its intentional exit
# on rejection must not exit this test runner. Only Get-Process is substituted,
# and every invocation must request the exact production host list. No running
# audio host is closed and LOCALAPPDATA is redirected only inside the child.
@'
[CmdletBinding()]
param([string]$InstallerPath,[string]$SourceBundle,[string]$FixtureRoot,[string]$GuardLog,
      [ValidateSet('Running','Idle')][string]$ProcessState,[switch]$DryRun,[switch]$ReplaceInstall)
$ErrorActionPreference='Stop'
$testProject=[IO.Path]::GetFullPath((Join-Path (Split-Path $InstallerPath) '..'))
$testQa=[IO.Path]::GetFullPath((Join-Path $testProject 'build\qa'))
$testFixture=[IO.Path]::GetFullPath($FixtureRoot)
if(!$testFixture.StartsWith($testQa+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Child installer fixture escaped build/qa.'
}
$global:FrostbiteInstallerTestLog=[IO.Path]::GetFullPath($GuardLog)
if(!$global:FrostbiteInstallerTestLog.StartsWith($testFixture+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Child process-guard log escaped its fixture.'
}
$global:FrostbiteInstallerTestState=$ProcessState
function global:Get-Process {
    [CmdletBinding()]
    param([string[]]$Name)
    $request=($Name -join '|')+';ErrorAction='+[string]$PSBoundParameters['ErrorAction']
    [IO.File]::AppendAllText($global:FrostbiteInstallerTestLog,$request+[Environment]::NewLine)
    if($request -cne 'Studio One|StudioOne|FROSTBITE|frostbite_render;ErrorAction=SilentlyContinue') {
        throw "Unexpected production process-guard query: $request"
    }
    if($global:FrostbiteInstallerTestState -eq 'Running') {
        [pscustomobject]@{ ProcessName='Studio One'; Id=12345 }
    }
}
$env:LOCALAPPDATA=$testFixture
$installArguments=@{ Scope='User'; SourceBundle=$SourceBundle }
if($DryRun) { $installArguments.WhatIf=$true }
if($ReplaceInstall) { $installArguments.Replace=$true }
& $InstallerPath @installArguments
if($LASTEXITCODE) { exit $LASTEXITCODE }
exit 0
'@ | Set-Content -LiteralPath $helper -Encoding UTF8

$shellExe=Join-Path $PSHOME 'powershell.exe'
if(!(Test-Path -LiteralPath $shellExe)) { $shellExe=Join-Path $PSHOME 'pwsh.exe' }
if(!(Test-Path -LiteralPath $shellExe)) { throw 'PowerShell child executable not found.' }
function Invoke-FixtureInstaller([string]$Case,[string]$ProcessState,[switch]$DryRun,[switch]$ReplaceInstall) {
    $guardLog=Join-Path $fixture ($Case+'-process-guard.log')
    Assert-FixturePath $guardLog
    $childArguments=@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$helper,
        '-InstallerPath',$installer,'-SourceBundle',$source,'-FixtureRoot',$fixture,
        '-GuardLog',$guardLog,'-ProcessState',$ProcessState)
    if($DryRun) { $childArguments+='-DryRun' }
    if($ReplaceInstall) { $childArguments+='-ReplaceInstall' }
    $previousPreference=$ErrorActionPreference
    try {
        # Windows PowerShell wraps redirected native stderr as error records;
        # collect expected rejection output, then check the child exit explicitly.
        $ErrorActionPreference='Continue'
        $output=& $shellExe @childArguments 2>&1
        $exitCode=$LASTEXITCODE
    } finally { $ErrorActionPreference=$previousPreference }
    $guardCalls=@()
    if(Test-Path -LiteralPath $guardLog) { $guardCalls=@(Get-Content -LiteralPath $guardLog) }
    $expectedCalls=if($DryRun){0}else{1}
    if($guardCalls.Count -ne $expectedCalls) { throw "${Case}: production host guard called $($guardCalls.Count) times; expected $expectedCalls." }
    foreach($call in $guardCalls) {
        if($call -cne 'Studio One|StudioOne|FROSTBITE|frostbite_render;ErrorAction=SilentlyContinue') {
            throw "${Case}: incorrect production host guard: $call"
        }
    }
    [pscustomobject]@{ ExitCode=$exitCode; Output=(($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine) }
}

$result=Invoke-FixtureInstaller -Case 'whatif' -ProcessState Running -DryRun
if($result.ExitCode -ne 0) { throw "WhatIf failed: $($result.Output)" }
if(Test-Path -LiteralPath $target) { throw 'WhatIf wrote an installation.' }
$result=Invoke-FixtureInstaller -Case 'host-running' -ProcessState Running
if($result.ExitCode -eq 0) { throw 'Installer accepted a running audio host.' }
if($result.Output -notmatch 'Close\s+Studio One and\s+FROSTBITE audio hosts') { throw "Wrong host-running rejection: $($result.Output)" }
if((Test-Path -LiteralPath $target) -or (Test-Path -LiteralPath $scan) -or (Test-Path -LiteralPath $backups)) {
    throw 'Host-running rejection created an installation, staging or backup tree.'
}
$result=Invoke-FixtureInstaller -Case 'host-idle-install' -ProcessState Idle
if($result.ExitCode -ne 0) { throw "Idle-host install failed: $($result.Output)" }
if(!(Test-Path -LiteralPath (Join-Path $target '.frostbite-install'))) { throw 'Install marker missing.' }
$legacy=Join-Path $scan 'FROSTBITE.backup-legacy'
$unmarked=Join-Path $scan 'FROSTBITE.backup-unmarked'
Assert-FixturePath $legacy
Assert-FixturePath $unmarked
Copy-Item -LiteralPath $target -Destination $legacy -Recurse
New-Item -ItemType Directory -Path $unmarked | Out-Null
Set-Content -LiteralPath (Join-Path $unmarked 'preserve.txt') -Value 'unrelated content'
$result=Invoke-FixtureInstaller -Case 'host-idle-replace' -ProcessState Idle -ReplaceInstall
if($result.ExitCode -ne 0) { throw "Idle-host replacement failed: $($result.Output)" }
if(Test-Path -LiteralPath $legacy) { throw 'Legacy marked backup remains in scan directory.' }
if(!(Test-Path -LiteralPath (Join-Path $unmarked 'preserve.txt'))) { throw 'Unmarked directory was modified.' }
$archives=@(Get-ChildItem -LiteralPath $backups -Directory)
if($archives.Count -ne 2) { throw 'Replacement and migrated backup were not both preserved.' }
foreach($archive in $archives) {
    Assert-FixturePath $archive.FullName
    if(!(Test-Path -LiteralPath (Join-Path $archive.FullName 'Contents\x86_64-win\FROSTBITE.vst3'))) { throw 'Backup bundle incomplete.' }
}
$scanned=@(Get-ChildItem -LiteralPath $scan -Recurse -File -Filter '*.vst3')
if($scanned.Count -ne 1) { throw 'VST3 scan directory contains duplicate binaries.' }
$binary='Contents\x86_64-win\FROSTBITE.vst3'
if((Get-FileHash -LiteralPath (Join-Path $source $binary)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $target $binary)).Hash) {
    throw 'Installed binary hash mismatch.'
}
Write-Host 'PASS installer: isolated host guard queried with exact process names; running host rejected without writes; WhatIf and idle-host copy/hash/replace, backup outside scan tree, legacy marked-backup migration, unmarked directory preserved, single scanned binary.'
