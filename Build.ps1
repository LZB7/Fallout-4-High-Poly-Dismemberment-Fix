param(
    [Parameter(Mandatory=$true)][string]$Compiler,
    [string]$Python = 'python',
    [ValidateSet('primary','experimental')][string]$Variant = 'primary',
    [switch]$RunTests,
    [string]$Reference240,
    [string]$Reference221,
    [string]$FixtureDirectory
)
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    New-Item -ItemType Directory -Force -Path "build/$Variant",'validation' | Out-Null
    $argsCommon = @('-std=c++20','-O2','-Wall','-Wextra','-Werror','-DNDEBUG','-static','-Isrc')
    if ($Variant -eq 'experimental') { $argsCommon += '-DHPDF_EXPERIMENTAL=1' }
    $sdk = @('-include','src/SdkPrefix.h','-Ivendor/f4se')
    $sources = @('vendor/f4se/f4se_common/BranchTrampoline.cpp','vendor/f4se/f4se_common/SafeWrite.cpp','vendor/dxbc-checksum/DXBCChecksum.cpp')
    $libs = @('-ld3d11','-ld3dcompiler','-lbcrypt','-ldxguid')
    if ($RunTests) {
        & $Compiler @argsCommon tests/CoreTests.cpp -o "build/$Variant/CoreTests.exe"
        if ($LASTEXITCODE) { throw 'Core test build failed' }
        & "./build/$Variant/CoreTests.exe"
        if ($LASTEXITCODE) { throw 'Core tests failed' }
        if (-not $Reference240 -or -not $Reference221) { throw 'Both reference EXEs are required for static hook tests' }
        & $Python scripts/verify_profiles.py --reference240 $Reference240 --reference221 $Reference221
        if ($LASTEXITCODE) { throw 'Runtime profile provenance/signature check failed' }
        & $Compiler @argsCommon @sdk tests/ProductionHarness.cpp @sources @libs -o "build/$Variant/ProductionHarness.exe"
        if ($LASTEXITCODE) { throw 'Production harness build failed' }
        & "./build/$Variant/ProductionHarness.exe" $Reference240 $Reference221
        if ($LASTEXITCODE) { throw 'Production harness failed' }
        if (-not $FixtureDirectory) { throw 'Provide private replay fixtures to run the complete GPU regression suite' }
        New-Item -ItemType Directory -Force -Path validation/shader | Out-Null
        foreach ($fixture in @('native.dxbc','indices.bin','vertices.bin','arm-constants.bin','thigh-constants.bin','right-thigh-constants.bin')) {
            Copy-Item -LiteralPath (Join-Path $FixtureDirectory $fixture) -Destination (Join-Path 'validation/shader' $fixture) -Force
        }
        & $Compiler @argsCommon tests/CorrectionTests.cpp vendor/dxbc-checksum/DXBCChecksum.cpp @libs -o "build/$Variant/CorrectionTests.exe"
        if ($LASTEXITCODE) { throw 'Correction test build failed' }
        & "./build/$Variant/CorrectionTests.exe"
        if ($LASTEXITCODE) { throw 'Correction tests failed' }
        if ($Variant -eq 'experimental') {
            & $Compiler @argsCommon tests/ShaderIdentityTests.cpp vendor/dxbc-checksum/DXBCChecksum.cpp @libs -o build/experimental/ShaderIdentityTests.exe
            if ($LASTEXITCODE) { throw 'Shader identity test build failed' }
            & ./build/experimental/ShaderIdentityTests.exe
            if ($LASTEXITCODE) { throw 'Shader identity tests failed' }
        }
    }
    & $Compiler @argsCommon @sdk -shared src/Plugin.cpp @sources @libs -o "build/$Variant/HighPolyDismembermentFix.dll"
    if ($LASTEXITCODE) { throw 'DLL build failed' }
    $verify = @('scripts/verify_dll.py',"build/$Variant/HighPolyDismembermentFix.dll",'--output',"validation/$Variant-dll.json")
    if ($Variant -eq 'experimental') { $verify += '--experimental' }
    & $Python @verify
    if ($LASTEXITCODE) { throw 'DLL validation failed' }
} finally { Pop-Location }
