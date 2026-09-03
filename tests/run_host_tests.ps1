param(
    [string]$CMake = 'cmake',
    [string]$CTest = 'ctest',
    [string]$DoctestDir = ''
)

$ErrorActionPreference = 'Stop'
$testsDir = $PSScriptRoot
$buildDir = Join-Path $testsDir 'build-host'

$configure = @('-S', $testsDir, '-B', $buildDir)
if ($DoctestDir) {
    $configure += "-DDOCTEST_DIR=$DoctestDir"
}

& $CMake @configure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $CMake --build $buildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $CTest --test-dir $buildDir --output-on-failure --no-tests=error
exit $LASTEXITCODE
