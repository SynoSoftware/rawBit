param(
    [string[]]$Configuration = @('Release','Debug'),
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Architecture = 'x64'
)

function Resolve-CMakeExe {
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCmd) {
        return $cmakeCmd.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vsCmake = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' 2>$null | Select-Object -First 1
        if ($vsCmake -and (Test-Path $vsCmake)) {
            return $vsCmake
        }
    }

    throw "Unable to locate cmake.exe. Install CMake or add it to PATH."
}

function Ensure-BoostHeaders {
    param(
        [string]$BoostRoot
    )

    $versionHeader = Join-Path $BoostRoot 'boost\version.hpp'
    if (Test-Path $versionHeader) {
        return
    }

    $bootstrap = Join-Path $BoostRoot 'bootstrap.bat'
    if (-not (Test-Path $bootstrap)) {
        throw "Boost bootstrap.bat not found at $bootstrap. Ensure the boost submodule is synced."
    }

    $buildRepo = Join-Path $BoostRoot 'tools\build'
    $buildScript = Join-Path $buildRepo 'src\engine\build.bat'
    if (-not (Test-Path $buildScript)) {
        Write-Host "Boost build scripts missing; initializing nested submodules..." -ForegroundColor Yellow
        & git -C $BoostRoot submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) {
            throw "Boost submodules are incomplete. Run 'git -C external/boost submodule update --init --recursive'."
        }

        if (-not (Test-Path $buildScript) -and (Test-Path $buildRepo)) {
            Write-Host "Resetting tools/build working tree..." -ForegroundColor Yellow
            & git -C $buildRepo reset --hard HEAD
        }

        if (-not (Test-Path $buildScript)) {
            throw "Boost submodules are incomplete. Run 'git -C external/boost submodule update --init --recursive'."
        }
    }

    Write-Host "Bootstrapping Boost (running bootstrap.bat)..." -ForegroundColor Cyan
    Push-Location $BoostRoot
    try {
        & $bootstrap
        if ($LASTEXITCODE -ne 0) {
            throw "Boost bootstrap failed ($LASTEXITCODE)."
        }

        $b2 = Join-Path $BoostRoot 'b2.exe'
        if (-not (Test-Path $b2)) {
            throw "b2.exe was not generated in $BoostRoot."
        }

        Write-Host "Generating Boost headers (b2 headers)..." -ForegroundColor Cyan
        & $b2 headers
        if ($LASTEXITCODE -ne 0) {
            throw "Generating Boost headers failed ($LASTEXITCODE)."
        }
    }
    finally {
        Pop-Location
    }
}

$cmakeExe = Resolve-CMakeExe

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

Ensure-BoostHeaders -BoostRoot $boostRoot

$trySignalSource = Join-Path $libtorrentSource 'deps\try_signal\try_signal.cpp'
if (-not (Test-Path $trySignalSource)) {
    Write-Host "Initializing libtorrent submodules..." -ForegroundColor Yellow
    & git -C $libtorrentSource submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to sync libtorrent submodules."
    }
}

$cmakeArgs = @(
    '-S', $libtorrentSource,
    '-B', $buildDir,
    '-G', $Generator,
    '-A', $Architecture,
    '-DBUILD_SHARED_LIBS=OFF',
    '-Dstatic_runtime=ON',
    '-Dbuild_tests=OFF',
    '-Dbuild_examples=OFF',
    '-Dbuild_tools=OFF',
    '-Dpython-bindings=OFF',
    '-Dpython-egg-info=OFF',
    '-Dpython-install-system-dir=OFF',
    '-Dgnutls=OFF',
    "-DBOOST_ROOT=$boostRoot",
    "-DBoost_ROOT=$boostRoot",
    "-DBoost_INCLUDE_DIR=$boostRoot",
    '-DBoost_NO_SYSTEM_PATHS=ON',
    '-DCMAKE_POLICY_DEFAULT_CMP0144=OLD',
    '-DCMAKE_POLICY_DEFAULT_CMP0167=OLD',
    '-DCMAKE_CXX_FLAGS=/FS',
    '-DCMAKE_C_FLAGS=/FS'
)

$previousCL = $env:CL
if ([string]::IsNullOrWhiteSpace($previousCL)) {
    $env:CL = '/FS'
} elseif ($previousCL -notmatch '(^| )/FS($| )') {
    $env:CL = "$previousCL /FS"
}

try {
    Write-Host "Configuring libtorrent with CMake..." -ForegroundColor Cyan
    & $cmakeExe @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed ($LASTEXITCODE)."
    }

    foreach ($config in $Configuration) {
        Write-Host "Building configuration '$config'..." -ForegroundColor Cyan
        & $cmakeExe --build $buildDir --config $config -- /m:1
        if ($LASTEXITCODE -ne 0) {
            throw "Building libtorrent ($config) failed ($LASTEXITCODE)."
        }
    }

    Write-Host "libtorrent binaries are available under $buildDir" -ForegroundColor Green
}
finally {
    $env:CL = $previousCL
}
