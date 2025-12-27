$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $repoRoot 'patches\vadpcm-msvc.patch'
$vadpcmDir = Join-Path $repoRoot 'vendor\vadpcm'

if (!(Test-Path $vadpcmDir)) {
    throw "vadpcm submodule not found at $vadpcmDir"
}

if (!(Test-Path $patchPath)) {
    throw "Patch file not found at $patchPath"
}

Push-Location $vadpcmDir
try {
    git apply -p1 --ignore-space-change --ignore-whitespace $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw "git apply failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

Write-Host "Applied vadpcm MSVC patch."
