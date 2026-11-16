param(
    [ValidateSet('primary','experimental')][string]$Variant = 'primary',
    [string]$Python = 'python',
    [switch]$RunTests,
    [string]$ReferenceOG,
    [string]$ReferenceNG,
    [string]$ReferenceAE,
    [string]$AddressLibrary,
    [string]$FixtureDirectory
)
# Builds one variant with xmake + CommonLibF4 (MSVC). Note: CommonLibF4's plugin
# rule also installs the DLL when XSE_FO4_MODS_PATH or XSE_FO4_GAME_PATH is set.
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    $experimental = if ($Variant -eq 'experimental') { 'y' } else { 'n' }
    & xmake f -m release "--experimental=$experimental" -y
    if ($LASTEXITCODE) { throw 'xmake configuration failed' }
    & xmake build HighPolyDismembermentFix
    if ($LASTEXITCODE) { throw 'DLL build failed' }
    $out = 'build/windows/x64/release'
    New-Item -ItemType Directory -Force -Path "build/$Variant",'validation' | Out-Null
    Copy-Item -LiteralPath "$out/HighPolyDismembermentFix.dll" -Destination "build/$Variant/" -Force
    Copy-Item -LiteralPath "$out/HighPolyDismembermentFix.pdb" -Destination "build/$Variant/" -Force -ErrorAction SilentlyContinue
    $verify = @('scripts/verify_dll.py',"build/$Variant/HighPolyDismembermentFix.dll",'--output',"validation/$Variant-dll.json")
    if ($Variant -eq 'experimental') { $verify += '--experimental' }
    & $Python @verify
    if ($LASTEXITCODE) { throw 'DLL validation failed' }

    if ($RunTests) {
        & xmake build CoreTests
        if ($LASTEXITCODE) { throw 'Core test build failed' }
        & "./$out/CoreTests.exe"
        if ($LASTEXITCODE) { throw 'Core tests failed' }

        if (-not $ReferenceOG -or -not $ReferenceNG -or -not $ReferenceAE -or -not $AddressLibrary) {
            throw 'Site tests need -ReferenceOG/-ReferenceNG/-ReferenceAE (1.10.163/1.10.984/1.11.240 EXEs) and -AddressLibrary (folder with version-*.bin)'
        }
        & xmake build SiteTests
        if ($LASTEXITCODE) { throw 'Site test build failed' }
        & "./$out/SiteTests.exe" og $ReferenceOG (Join-Path $AddressLibrary 'version-1-10-163-0.bin') `
            ngae $ReferenceNG (Join-Path $AddressLibrary 'version-1-10-984-0.bin') `
            ngae $ReferenceAE (Join-Path $AddressLibrary 'version-1-11-240-0.bin')
        if ($LASTEXITCODE) { throw 'Site tests failed' }

        if (-not $FixtureDirectory) { throw 'Provide private replay fixtures to run the complete GPU regression suite' }
        New-Item -ItemType Directory -Force -Path validation/shader | Out-Null
        foreach ($fixture in @('native.dxbc','indices.bin','vertices.bin','arm-constants.bin','thigh-constants.bin','right-thigh-constants.bin')) {
            Copy-Item -LiteralPath (Join-Path $FixtureDirectory $fixture) -Destination (Join-Path 'validation/shader' $fixture) -Force
        }
        & xmake build CorrectionTests
        if ($LASTEXITCODE) { throw 'Correction test build failed' }
        & "./$out/CorrectionTests.exe"
        if ($LASTEXITCODE) { throw 'Correction tests failed' }
        if ($Variant -eq 'experimental') {
            & xmake build ShaderIdentityTests
            if ($LASTEXITCODE) { throw 'Shader identity test build failed' }
            & "./$out/ShaderIdentityTests.exe"
            if ($LASTEXITCODE) { throw 'Shader identity tests failed' }
        }
    }
} finally { Pop-Location }
