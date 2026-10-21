[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release',
      [int]$Jobs=3, [switch]$SkipRenders, [string]$RenderDirectory='')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cmakeCommand=Get-Command cmake -ErrorAction SilentlyContinue
if($cmakeCommand) { $cmakeExe=$cmakeCommand.Source }
else {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 C++ Build Tools and CMake.' }
    $vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmakeExe=Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if(!(Test-Path -LiteralPath $cmakeExe)) { throw 'CMake not found. Add the C++ CMake tools component in Visual Studio Installer.' }
$ctestExe=Join-Path (Split-Path $cmakeExe) 'ctest.exe'
Push-Location $projectRoot
try {
    & $cmakeExe -S . -B build -G 'Visual Studio 17 2022' -A x64
    if($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & $cmakeExe --build build --config $Configuration --parallel $Jobs
    if($LASTEXITCODE) { throw 'Build failed.' }
    & $ctestExe --test-dir build -C $Configuration --output-on-failure
    if($LASTEXITCODE) { throw 'Automated tests failed.' }
    $bundle=Join-Path $projectRoot "build\Frostbite_artefacts\$Configuration\VST3\FROSTBITE.vst3"
    & ".\build\frostbite_host_check_artefacts\$Configuration\frostbite_host_check.exe" $bundle |
        Tee-Object -FilePath 'build\host-results.txt'
    if($LASTEXITCODE) { throw 'VST3 host check failed.' }
    & (Join-Path $projectRoot 'Tests\InstallerTests.ps1') 6>&1 | Tee-Object -FilePath 'build\installer-results.txt'
    if(!$SkipRenders) {
        # A fresh destination preserves audio that may be open/locked in a DAW.
        $diagnosticDirectory=if($RenderDirectory){[IO.Path]::GetFullPath($RenderDirectory)}else{Join-Path $projectRoot 'renders/v2/diagnostics'}
        & ".\build\frostbite_render_artefacts\$Configuration\frostbite_render.exe" --diagnostics $diagnosticDirectory |
            Tee-Object -FilePath 'build\render-metrics.txt'
        if($LASTEXITCODE) { throw 'Diagnostic render failed.' }
    }
    Write-Host "Built and validated: $bundle"
} finally { Pop-Location }
