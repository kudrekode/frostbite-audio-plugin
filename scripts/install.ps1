[CmdletBinding(SupportsShouldProcess)]
param([ValidateSet('User','System')][string]$Scope='User',
      [string]$SourceBundle='', [switch]$Replace)
$ErrorActionPreference='Stop'
try {
    $packageRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    if(!$SourceBundle) {
        $packaged=Join-Path $packageRoot 'FROSTBITE.vst3'
        $built=Join-Path $packageRoot 'build\Frostbite_artefacts\Release\VST3\FROSTBITE.vst3'
        $SourceBundle=if(Test-Path -LiteralPath $packaged){$packaged}else{$built}
    }
    $source=(Resolve-Path -LiteralPath $SourceBundle).Path
    if((Split-Path $source -Leaf) -ne 'FROSTBITE.vst3') { throw 'Expected the FROSTBITE.vst3 bundle directory.' }
    $binary=Join-Path $source 'Contents\x86_64-win\FROSTBITE.vst3'
    if(!(Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Missing Windows x64 binary: $binary" }
    $root=if($Scope -eq 'System'){Join-Path $env:ProgramFiles 'Common Files\VST3'}
          else{Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'}
    $root=[IO.Path]::GetFullPath($root)
    $target=[IO.Path]::GetFullPath((Join-Path $root 'FROSTBITE.vst3'))
    if([IO.Path]::GetDirectoryName($target) -ne $root) { throw 'Invalid installation target.' }
    # DAWs recursively scan VST3 directories, even when the containing backup folder
    # has no .vst3 suffix. Keep backups completely outside the scanned tree.
    $backupBase=if($Scope -eq 'System'){$env:ProgramData}else{$env:LOCALAPPDATA}
    $backupRoot=[IO.Path]::GetFullPath((Join-Path $backupBase 'Frostbite\Plugin Backups'))
    if($backupRoot -eq $root -or $backupRoot.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Backup directory must be outside the VST3 scan directory.'
    }
    if(Test-Path -LiteralPath $target) {
        if(!$Replace) { throw "Already installed: $target. Close audio hosts and use -Replace to preserve a backup and replace it." }
        $marker=Join-Path $target '.frostbite-install'
        if(!(Test-Path -LiteralPath $marker) -or (Get-Content -LiteralPath $marker -Raw).Trim() -ne 'audio.frostbite.effect') {
            throw 'Existing bundle is not marked as this project installation. Refusing to overwrite it.'
        }
        if((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing to replace a linked bundle.' }
    }
    if($PSCmdlet.ShouldProcess($target,'Install complete FROSTBITE x64 bundle')) {
        if(Get-Process -Name 'Studio One','StudioOne','FROSTBITE','frostbite_render' -ErrorAction SilentlyContinue) {
            throw 'Close Studio One and FROSTBITE audio hosts before installing or replacing the plugin.'
        }
        New-Item -ItemType Directory -Path $root -Force | Out-Null
        $staging=[IO.Path]::GetFullPath((Join-Path $root ('.frostbite-stage-'+[Guid]::NewGuid().ToString('N'))))
        if([IO.Path]::GetDirectoryName($staging) -ne $root) { throw 'Invalid staging target.' }
        Copy-Item -LiteralPath $source -Destination $staging -Recurse -ErrorAction Stop
        $copied=Join-Path $staging 'Contents\x86_64-win\FROSTBITE.vst3'
        if((Get-FileHash -LiteralPath $binary).Hash -ne (Get-FileHash -LiteralPath $copied).Hash) { throw 'Copy verification failed.' }
        Set-Content -LiteralPath (Join-Path $staging '.frostbite-install') -Value 'audio.frostbite.effect' -Encoding ASCII
        New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
        # Migrate only marked backups created by earlier versions of this installer.
        foreach($legacy in @(Get-ChildItem -LiteralPath $root -Directory -Filter 'FROSTBITE.backup-*')) {
            $legacyPath=[IO.Path]::GetFullPath($legacy.FullName)
            $legacyMarker=Join-Path $legacyPath '.frostbite-install'
            if([IO.Path]::GetDirectoryName($legacyPath) -ne $root) { throw 'Invalid legacy backup source.' }
            if($legacy.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            if(!(Test-Path -LiteralPath $legacyMarker -PathType Leaf)) { continue }
            if((Get-Content -LiteralPath $legacyMarker -Raw).Trim() -ne 'audio.frostbite.effect') { continue }
            $archive=[IO.Path]::GetFullPath((Join-Path $backupRoot ($legacy.Name+'-'+[Guid]::NewGuid().ToString('N'))))
            if([IO.Path]::GetDirectoryName($archive) -ne $backupRoot) { throw 'Invalid legacy backup destination.' }
            Move-Item -LiteralPath $legacyPath -Destination $archive -ErrorAction Stop
            Write-Host "Legacy backup moved outside VST3 scan folder: $archive"
        }
        if(Test-Path -LiteralPath $target) {
            $backup=[IO.Path]::GetFullPath((Join-Path $backupRoot ('FROSTBITE.backup-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N'))))
            if([IO.Path]::GetDirectoryName($backup) -ne $backupRoot) { throw 'Invalid backup target.' }
            Move-Item -LiteralPath $target -Destination $backup -ErrorAction Stop
            Write-Host "Previous installation preserved: $backup"
        }
        Move-Item -LiteralPath $staging -Destination $target -ErrorAction Stop
        Write-Host "Installed and SHA256 verified: $target"
        Write-Host 'Restart Studio One and rescan VST3 effects to discard cached backup paths. Add the user folder in Locations if needed.'
    }
} catch {
    Write-Error ("Installation failed: "+$_.Exception.Message+" For -Scope System, run PowerShell as Administrator.")
    exit 1
}
