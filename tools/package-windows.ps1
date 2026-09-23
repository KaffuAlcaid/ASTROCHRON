param(
    [Parameter(Mandatory = $true)]
    [string]$ApplicationDirectory,
    [string]$OutputDirectory = '',
    [string]$InnoSetupRoot = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$applicationRoot = (Resolve-Path -LiteralPath $ApplicationDirectory).Path
if (!$OutputDirectory) { $OutputDirectory = Join-Path $projectRoot '.cache/packages' }
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (!$InnoSetupRoot) { $InnoSetupRoot = Join-Path $projectRoot '.cache/toolchains/inno-7.1.0' }
$compiler = Join-Path $InnoSetupRoot 'ISCC.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Inno Setup was not found. Set -InnoSetupRoot to its directory.' }

$application = Get-Item -LiteralPath (Join-Path $applicationRoot 'ASTROCHRON.exe')
$version = $application.VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$') { throw 'The application has no valid product version.' }
$fileVersion = @($application.VersionInfo.FileMajorPart, $application.VersionInfo.FileMinorPart,
    $application.VersionInfo.FileBuildPart, $application.VersionInfo.FilePrivatePart) -join '.'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

& $compiler '/Qp' "/DAppVersion=$version" "/DFileVersion=$fileVersion" "/DApplicationDirectory=$applicationRoot" "/DProjectRoot=$projectRoot" "/O$outputRoot" (Join-Path $projectRoot 'packaging/windows/astrochron.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }

$archive = Join-Path $outputRoot "ASTROCHRON-v$version-windows-x64.zip"
Compress-Archive -LiteralPath $applicationRoot -DestinationPath $archive -CompressionLevel Optimal -Force
Write-Output $archive
Write-Output (Join-Path $outputRoot "ASTROCHRON-v$version-windows-x64-setup.exe")
