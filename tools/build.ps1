param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$QtRoot = '',
    [string]$DeployDirectory = '',
    [switch]$Deploy
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolchainRoot = Join-Path $projectRoot '.cache/toolchains'
$buildDirectory = Join-Path $projectRoot ".cache/build/$Configuration"
$localTools = @(
    (Join-Path $toolchainRoot 'python/Scripts'),
    (Join-Path $toolchainRoot 'python/Lib/site-packages/cmake/data/bin')
)
foreach ($directory in $localTools) {
    if (Test-Path -LiteralPath $directory) { $env:PATH = "$directory;$env:PATH" }
}

$compilerSetup = Join-Path $toolchainRoot 'msvc/setup_x64.bat'
if (Test-Path -LiteralPath $compilerSetup) {
    $environment = & cmd.exe /d /s /c "`"`"$compilerSetup`" >nul && set`""
    if ($LASTEXITCODE -ne 0) { throw 'Unable to load the MSVC environment.' }
    foreach ($line in $environment) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable($line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
        }
    }
}

if (!$QtRoot) { $QtRoot = Join-Path $toolchainRoot 'qt/6.11.2/msvc2022_64' }
if (!(Test-Path -LiteralPath (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    throw 'Qt 6.11 SDK not found. Set -QtRoot to its installation directory.'
}
if (!(Test-Path -LiteralPath (Join-Path $projectRoot 'node_modules/mapshaper/package.json'))) {
    throw 'Map tools are missing. Run npm ci --ignore-scripts first.'
}

$ninja = (Get-Command ninja -ErrorAction Stop).Source
& cmake -S $projectRoot -B $buildDirectory -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildDirectory --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

if ($Deploy) {
    $destination = if ($DeployDirectory) { [IO.Path]::GetFullPath($DeployDirectory) }
        else { Join-Path $projectRoot ('.cache/dist/ASTROCHRON-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
    if (Test-Path -LiteralPath $destination) { throw 'Deployment directory already exists.' }
    & cmake --install $buildDirectory --prefix $destination
    if ($LASTEXITCODE -ne 0) { throw 'Application installation failed.' }
    $deploymentMode = if ($Configuration -eq 'Debug') { '--debug' } else { '--release' }
    $runtimeDirectory = Join-Path $toolchainRoot 'msvc-runtime/x64'
    if (!(Test-Path -LiteralPath $runtimeDirectory) -and $env:VCToolsRedistDir) {
        $runtimeDirectory = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
    }
    if ($Configuration -eq 'Release' -and !(Test-Path -LiteralPath (Join-Path $runtimeDirectory 'vcruntime140.dll'))) {
        throw 'The x64 VC runtime DLLs were not found. Load the MSVC x64 environment before deploying.'
    }
    $deploymentArguments = @($deploymentMode, '--verbose', '0', '--qmldir', (Join-Path $projectRoot 'qml'),
        '--translations', 'zh_CN', '--no-system-d3d-compiler', '--no-system-dxc-compiler', '--no-opengl-sw',
        '--skip-plugin-types', 'qmltooling,generic',
        '--exclude-plugins', 'qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql')
    if ($Configuration -eq 'Release' -and (Test-Path -LiteralPath $runtimeDirectory)) {
        $deploymentArguments += '--no-compiler-runtime'
    }
    $deploymentArguments += (Join-Path $destination 'ASTROCHRON.exe')
    & (Join-Path $QtRoot 'bin/windeployqt.exe') @deploymentArguments
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
    # The application fixes Qt Quick Controls to Basic; other styles are never selected.
    $unused = foreach ($style in @('FluentWinUI3', 'Fusion', 'Imagine', 'Material', 'Universal', 'Windows')) {
        "qml/QtQuick/Controls/$style"
        "qml/QtQuick/Dialogs/quickimpl/qml/+$style"
        "Qt6QuickControls2$style.dll"
        "Qt6QuickControls2${style}StyleImpl.dll"
    }
    $destinationRoot = [IO.Path]::GetFullPath($destination) + [IO.Path]::DirectorySeparatorChar
    foreach ($relativePath in $unused) {
        $path = [IO.Path]::GetFullPath((Join-Path $destination $relativePath))
        if (!$path.StartsWith($destinationRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid deployment path.' }
        if (Test-Path -LiteralPath $path) {
            $item = Get-Item -LiteralPath $path
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Unexpected deployment link.' }
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
    Get-ChildItem -LiteralPath (Join-Path $destination 'qml') -Recurse -File -Filter '*.qmltypes' | Remove-Item -Force
    if ($Configuration -eq 'Release' -and (Test-Path -LiteralPath $runtimeDirectory)) {
        Get-ChildItem -LiteralPath $runtimeDirectory -Filter '*.dll' | Copy-Item -Destination $destination
    }
    Write-Output (Join-Path $destination 'ASTROCHRON.exe')
}
