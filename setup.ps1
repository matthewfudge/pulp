#!/usr/bin/env pwsh
# Windows bootstrap wrapper for setup.sh.
#
# This imports the Visual Studio C++ environment into the current process,
# maps the repo to a short temporary drive letter to avoid MAX_PATH failures,
# and then delegates to the existing Bash-based setup flow.

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$SetupArgs
)

$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

function Resolve-Bash {
    $fallbacks = @(
        "C:\Program Files\Git\bin\bash.exe",
        "C:\Program Files\Git\usr\bin\bash.exe"
    )
    foreach ($candidate in $fallbacks) {
        if (Test-Path $candidate) { return $candidate }
    }

    $cmd = Get-Command bash -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    throw "bash not found on PATH and no Git Bash fallback was found"
}

function Resolve-VcVars {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return $null
    }

    $isArm64Host = $env:PROCESSOR_ARCHITECTURE -eq "ARM64"
    $toolComponent = if ($isArm64Host) {
        "Microsoft.VisualStudio.Component.VC.Tools.ARM64"
    } else {
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
    }
    $vcvarsName = if ($isArm64Host) { "vcvarsarm64.bat" } else { "vcvars64.bat" }

    $vswhere = Get-Command vswhere.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue
    if (-not $vswhere) {
        $bundledVsWhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $bundledVsWhere) { $vswhere = $bundledVsWhere }
    }

    if ($vswhere) {
        $installPath = & $vswhere -latest -products * -requires $toolComponent -property installationPath 2>$null
        if ($LASTEXITCODE -eq 0 -and $installPath) {
            $candidate = Join-Path $installPath ("VC\Auxiliary\Build\" + $vcvarsName)
            if (Test-Path $candidate) { return $candidate }
        }
    }

    $fallbacks = @(
        ("C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\" + $vcvarsName),
        ("C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\" + $vcvarsName),
        ("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\" + $vcvarsName),
        ("C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\" + $vcvarsName)
    )
    if ($isArm64Host) {
        $fallbacks += @(
            "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
            "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
            "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
            "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        )
    }
    foreach ($candidate in $fallbacks) {
        if (Test-Path $candidate) { return $candidate }
    }

    return $null
}

function Import-VcVarsEnvironment {
    param([string]$BatchFile)

    if (-not $BatchFile) { return }

    $wrapper = Join-Path $env:TEMP ("pulp-setup-vcvars-" + [Guid]::NewGuid().ToString("N") + ".cmd")
    try {
        @(
            "@echo off",
            ('call "{0}" >nul' -f $BatchFile),
            "set"
        ) | Set-Content -Path $wrapper -Encoding ASCII
        $lines = & cmd.exe /d /c $wrapper
        if ($LASTEXITCODE -ne 0) {
            throw "failed to import MSVC environment from $BatchFile"
        }
    } finally {
        Remove-Item -Force -ErrorAction SilentlyContinue $wrapper
    }

    foreach ($line in $lines) {
        if ($line -match '^(.*?)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
}

function Convert-ToBashPath {
    param([string]$PathValue)

    $normalized = $PathValue -replace '\\', '/'
    if ($normalized -match '^([A-Za-z]):/(.*)$') {
        return "/" + $matches[1].ToLowerInvariant() + "/" + $matches[2]
    }
    return $normalized
}

function Quote-BashArg {
    param([string]$Value)
    return '"' + (($Value -replace '\\', '\\') -replace '"', '\\"') + '"'
}

function New-TemporaryRepoDrive {
    param([string]$PathValue)

    $resolved = (Resolve-Path -LiteralPath $PathValue).ProviderPath
    $letters = @("P", "R", "S", "T", "U", "V", "W", "X", "Y", "Z")
    $used = @((Get-PSDrive -PSProvider FileSystem | ForEach-Object { $_.Name.ToUpperInvariant() }))

    foreach ($letter in $letters) {
        if ($used -contains $letter) { continue }
        & cmd.exe /d /c ('subst {0}: "{1}"' -f $letter, $resolved) | Out-Null
        if ($LASTEXITCODE -eq 0) {
            return @{
                Drive = $letter + ":\"
                Mapped = $true
            }
        }
    }

    return @{
        Drive = $resolved
        Mapped = $false
    }
}

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BashExe = Resolve-Bash
$VcVarsBat = Resolve-VcVars
Import-VcVarsEnvironment -BatchFile $VcVarsBat

$RepoDrive = New-TemporaryRepoDrive -PathValue $RepoRoot
$RepoRootForBash = if ($RepoDrive.Mapped) { $RepoDrive.Drive } else { $RepoRoot }
$RepoRootBash = Convert-ToBashPath -PathValue $RepoRootForBash
$ArgString = (($SetupArgs | ForEach-Object { Quote-BashArg $_ }) -join " ").Trim()
$BashCommand = if ($ArgString) {
    "cd $(Quote-BashArg $RepoRootBash) && ./setup.sh $ArgString"
} else {
    "cd $(Quote-BashArg $RepoRootBash) && ./setup.sh"
}

if ($RepoDrive.Mapped) {
    Write-Host ("Using temporary drive alias {0} for Windows bootstrap to avoid long-path failures." -f $RepoDrive.Drive.TrimEnd('\'))
}

try {
    & $BashExe -lc $BashCommand
    exit $LASTEXITCODE
} finally {
    if ($RepoDrive.Mapped) {
        & cmd.exe /d /c ('subst {0} /d' -f $RepoDrive.Drive.TrimEnd('\')) | Out-Null
    }
}
