param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$ReferenceHeader
)
$ErrorActionPreference = 'Stop'
$bindingInput = Join-Path $env:GEODE_BINDINGS_REPO_PATH 'bindings\2.2081'
$bindingOverlay = Join-Path $BuildDirectory 'compat-bindings-22081'
if (-not (Test-Path -LiteralPath $ReferenceHeader)) {
    throw 'The unmodified GJGameState reference header was not generated'
}

foreach ($bindingFile in Get-ChildItem -LiteralPath $bindingInput -Recurse -File) {
    $relative = [System.IO.Path]::GetRelativePath($bindingInput, $bindingFile.FullName)
    $destination = Join-Path $bindingOverlay $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath $bindingFile.FullName -Destination $destination -Force
}
$broPath = Join-Path $bindingOverlay 'GeometryDash.bro'
$bro = [System.IO.File]::ReadAllText($broPath)
foreach ($rename in @(@('m_unkPoint4', 'm_cameraDeltaPosition'), @('m_cameraPosition2', 'm_cameraLastPosition'))) {
    $pattern = '    \[\[renamed_from\(' + $rename[0] + '\)\]\]\r?\n(?=    cocos2d::CCPoint ' + $rename[1] + ';)'
    if ([regex]::Matches($bro, $pattern).Count -ne 1) { throw "Unexpected declaration for $($rename[1])" }
    $bro = [regex]::Replace($bro, $pattern, '')
}
[System.IO.File]::WriteAllText($broPath, $bro)

$abiDirectory = Join-Path $BuildDirectory 'abi-check'
New-Item -ItemType Directory -Force -Path $abiDirectory | Out-Null
$originalHeader = [System.IO.File]::ReadAllText($ReferenceHeader)
$reference = $originalHeader.Replace('class GJGameState {', 'class GJGameStateOriginal {')
$reference = [regex]::Replace($reference, '#include "([^"\r\n]+)"', '#include <Geode/binding/$1>')
[System.IO.File]::WriteAllText((Join-Path $abiDirectory 'GJGameStateOriginal.hpp'), $reference)

$fields = [regex]::Matches($originalHeader, '(?m)^\s+[^();\r\n]+\s+(m_[A-Za-z0-9_]+)(?:\{\})?(?:\[[^\]\r\n]+\])?;') |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
$removedAliases = @('m_unkPoint4', 'm_cameraPosition2')
$checks = @(
    '#include "GJGameStateOriginal.hpp"',
    '#include <Geode/binding/GJGameState.hpp>',
    '#include <cstddef>', '#include <type_traits>',
    'static_assert(sizeof(GJGameState) == sizeof(GJGameStateOriginal));',
    'static_assert(alignof(GJGameState) == alignof(GJGameStateOriginal));',
    'static_assert(!std::is_copy_assignable_v<GJGameStateOriginal>);',
    'static_assert(std::is_copy_assignable_v<GJGameState>);'
)
foreach ($field in $fields | Where-Object { $_ -notin $removedAliases }) {
    $checks += "static_assert(offsetof(GJGameState, $field) == offsetof(GJGameStateOriginal, $field));"
}
[System.IO.File]::WriteAllLines((Join-Path $abiDirectory 'check.cpp'), $checks)
Write-Host "Bindings overlay: two alias annotations removed; ABI proof covers $($fields.Count - 2) retained fields."
