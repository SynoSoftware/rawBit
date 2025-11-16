param(
    [string[]]$Configuration = @('Release','Debug'),
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Architecture = 'x64'
)

$scriptRoot = $PSScriptRoot
if (-not $scriptRoot) {
    $scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
}
$repoRoot = Split-Path -Parent $scriptRoot
$libtorrentSource = Join-Path $repoRoot 'external/libtorrent'
$boostRoot = Join-Path $repoRoot 'external/boost'
$buildDir = Join-Path $repoRoot 'external/libtorrent-build'

if (-not (Test-Path $libtorrentSource)) {
    throw "libtorrent source folder not found at $libtorrentSource. Did you sync submodules?"
}
if (-not (Test-Path $boostRoot)) {
    throw "Boost checkout not found at $boostRoot. Run 'git submodule update --init --recursive'."
}
if (-not (Test-Path $buildDir)) {
    [void](New-Item -ItemType Directory -Path $buildDir)
}

$cmakeArgs = @(
    '-S', $libtorrentSource,
    '-B', $buildDir,
    '-G', $Generator,
    '-A', $Architecture,
    '-DBUILD_SHARED_LIBS=OFF',
    '-Dstatic_runtime=OFF',
    '-Dbuild_tests=OFF',
    '-Dbuild_examples=OFF',
    '-Dbuild_tools=OFF',
    '-Dpython-bindings=OFF',
    '-Dpython-egg-info=OFF',
    '-Dpython-install-system-dir=OFF',
    '-Dgnutls=OFF',
    "-DBOOST_ROOT=$boostRoot"
)

Write-Host "Configuring libtorrent with CMake..." -ForegroundColor Cyan
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed ($LASTEXITCODE)."
}

foreach ($config in $Configuration) {
    Write-Host "Building configuration '$config'..." -ForegroundColor Cyan
    cmake --build $buildDir --config $config
    if ($LASTEXITCODE -ne 0) {
        throw "Building libtorrent ($config) failed ($LASTEXITCODE)."
    }
}

Write-Host "libtorrent binaries are available under $buildDir" -ForegroundColor Green
