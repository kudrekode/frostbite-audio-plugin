[CmdletBinding()]
param([ValidatePattern('^FROSTBITE-[A-Za-z0-9.-]+$')][string]$Name='FROSTBITE-1.0.0-Windows-x64')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$release=Join-Path $root 'build\Frostbite_artefacts\Release'
$destination=Join-Path $root ('dist\'+$Name)
$bundle=Join-Path $release 'VST3\FROSTBITE.vst3'
if(!(Test-Path -LiteralPath (Join-Path $bundle 'Contents\x86_64-win\FROSTBITE.vst3'))) { throw 'Build Release first.' }
if(Test-Path -LiteralPath $destination) { throw 'Package directory already exists. Preserve or rename it before packaging again.' }
New-Item -ItemType Directory -Path $destination | Out-Null
Copy-Item -LiteralPath $bundle -Destination $destination -Recurse
Copy-Item -LiteralPath (Join-Path $release 'Standalone\FROSTBITE.exe') -Destination $destination
Copy-Item -LiteralPath (Join-Path $root 'build\frostbite_render_artefacts\Release\frostbite_render.exe') -Destination $destination
foreach($file in @('readme.md','THIRD_PARTY_NOTICES.md','docs','scripts','Source','Tests','Tools','Presets','CMakeLists.txt','CMakePresets.json','.gitignore')) {
    Copy-Item -LiteralPath (Join-Path $root $file) -Destination $destination -Recurse
}
New-Item -ItemType Directory -Path (Join-Path $destination 'validation') | Out-Null
foreach($evidence in @('final-validation.log','render-metrics.txt','host-results.txt','ui-results.txt','installer-results.txt','install-results.txt','studio-one-frostbite-scan.txt','studio-one-postinstall-scan.txt','dsp-render-comparison.txt')) {
    Copy-Item -LiteralPath (Join-Path $root ('build\'+$evidence)) -Destination (Join-Path $destination 'validation')
}
Copy-Item -LiteralPath (Join-Path $root 'build\Testing\Temporary\LastTest.log') -Destination (Join-Path $destination 'validation\ctest-results.txt')
Copy-Item -LiteralPath (Join-Path $root 'build\qa\ui') -Destination (Join-Path $destination 'validation\ui') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'build\qa\studio-one') -Destination (Join-Path $destination 'validation\studio-one') -Recurse
Copy-Item -LiteralPath (Join-Path $root 'build\qa\Factory Presets') -Destination $destination -Recurse
if(Test-Path -LiteralPath (Join-Path $root 'renders')) {
    Copy-Item -LiteralPath (Join-Path $root 'renders') -Destination $destination -Recurse
}
$notices=Join-Path $destination 'licenses'
New-Item -ItemType Directory -Path $notices | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'external\JUCE\LICENSE.md') -Destination (Join-Path $notices 'JUCE-LICENSE.md')
Copy-Item -LiteralPath (Join-Path $root 'external\JUCE\JUCE.spdx.json') -Destination $notices
$vst=Join-Path $root 'external\JUCE\modules\juce_audio_processors_headless\format_types\VST3_SDK\LICENSE.txt'
if(Test-Path -LiteralPath $vst) { Copy-Item -LiteralPath $vst -Destination (Join-Path $notices 'VST3-LICENSE.txt') }
$filesToHash=@(Get-ChildItem -LiteralPath $destination -Recurse -File | Where-Object Name -ne 'SHA256.csv')
$manifest=$filesToHash | Get-FileHash -Algorithm SHA256 |
    Select-Object @{N='File';E={$_.Path.Substring($destination.Length+1)}},Hash |
    ConvertTo-Csv -NoTypeInformation
$manifest | Set-Content -LiteralPath (Join-Path $destination 'SHA256.csv') -Encoding UTF8
Compress-Archive -LiteralPath $destination -DestinationPath ($destination+'.zip')
Write-Host "Packaged: $destination.zip"
