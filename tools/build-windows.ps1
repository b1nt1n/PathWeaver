param(
    [string]$SdkPath = $env:GEODE_SDK,
    [string]$BindingsPath = $env:GEODE_BINDINGS_REPO_PATH,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '../build-windows'),
    [string]$GeodeCli = 'geode',
    [int]$Jobs = 2
)
$ErrorActionPreference = 'Stop'
$pathWeaverSource = Split-Path $PSScriptRoot -Parent
$pathWeaverSdk = (Resolve-Path -LiteralPath $SdkPath).Path
$pathWeaverBindings = (Resolve-Path -LiteralPath $BindingsPath).Path
$pathWeaverBuild = [IO.Path]::GetFullPath($BuildDirectory)
if ((Get-Content -LiteralPath (Join-Path $pathWeaverSdk 'VERSION') -Raw).Trim() -ne '5.11.2') {
    throw 'This build requires Geode SDK 5.11.2'
}
$pathWeaverBindingCommit = & git -C $pathWeaverBindings rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $pathWeaverBindingCommit -ne '1605f500d053f8de122bf5c7dc6d17c24866e02e') {
    throw 'Use the documented pinned Geode bindings commit'
}
$pathWeaverCompiler = (Get-Command clang-cl -CommandType Application).Source
$pathWeaverLinker = (Get-Command lld-link -CommandType Application).Source
$pathWeaverNinja = (Get-Command ninja -CommandType Application).Source
$pathWeaverCli = (Get-Command $GeodeCli -CommandType Application).Source
$pathWeaverCodegenArgs = "-DCMAKE_C_COMPILER=$pathWeaverCompiler;-DCMAKE_CXX_COMPILER=$pathWeaverCompiler;-DCMAKE_LINKER=$pathWeaverLinker;-DCMAKE_MAKE_PROGRAM=$pathWeaverNinja"
$pathWeaverCommon = @(
    '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_C_COMPILER=$pathWeaverCompiler", "-DCMAKE_CXX_COMPILER=$pathWeaverCompiler",
    "-DCMAKE_LINKER=$pathWeaverLinker", "-DCMAKE_MAKE_PROGRAM=$pathWeaverNinja",
    "-DGEODE_CLI=$pathWeaverCli", "-DGEODE_BINDINGS_REPO_PATH=$pathWeaverBindings",
    "-DGEODE_CODEGEN_CMAKE_ARGS=$pathWeaverCodegenArgs"
)
if ($env:CPM_SOURCE_CACHE) { $pathWeaverCommon += "-DCPM_SOURCE_CACHE=$env:CPM_SOURCE_CACHE" }
$pathWeaverPreviousSdk = $env:GEODE_SDK
$pathWeaverPreviousBindings = $env:GEODE_BINDINGS_REPO_PATH
New-Item -ItemType Directory -Force -Path $pathWeaverBuild | Out-Null
try {
    $env:GEODE_SDK = $pathWeaverSdk
    $env:GEODE_BINDINGS_REPO_PATH = $pathWeaverBindings
    $pathWeaverReference = Join-Path $pathWeaverBuild 'reference'
    & cmake -S (Join-Path $pathWeaverSource 'cmake/windows/reference') -B $pathWeaverReference @pathWeaverCommon
    if ($LASTEXITCODE -ne 0) { throw 'Unmodified SDK reference configure failed' }
    $pathWeaverModBuild = Join-Path $pathWeaverBuild 'mod'
    & (Join-Path $pathWeaverSource 'cmake/windows/Prepare-Bindings.ps1') -BuildDirectory $pathWeaverModBuild `
        -ReferenceHeader (Join-Path $pathWeaverReference 'bindings/bindings/Geode/binding/GJGameState.hpp')
    & cmake -S (Join-Path $pathWeaverSource 'cmake/windows') -B $pathWeaverModBuild @pathWeaverCommon
    if ($LASTEXITCODE -ne 0) { throw 'Mod configure failed' }
    & cmake --build $pathWeaverModBuild --target pathweaver_g1_binding_abi_check gdsolver --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Build or bindings ABI proof failed' }
    & $pathWeaverCli package new (Join-Path $pathWeaverModBuild 'package') `
        --binary (Join-Path $pathWeaverModBuild 'fork/b1nt1n.pathweaver.dll') `
        --output (Join-Path $pathWeaverBuild 'b1nt1n.pathweaver.geode')
    if ($LASTEXITCODE -ne 0) { throw 'Package assembly failed' }
    Write-Output "PATHWEAVER_BUILD_PASS: $(Join-Path $pathWeaverBuild 'b1nt1n.pathweaver.geode')"
    Write-Output 'Nothing was installed or launched.'
} finally {
    $env:GEODE_SDK = $pathWeaverPreviousSdk
    $env:GEODE_BINDINGS_REPO_PATH = $pathWeaverPreviousBindings
}
