# Runs SonarQube CLI analysis on one materialized git state.
# Usage:
#   .\run-sonar.ps1 -Worktree <path-to-state-checkout> -State clean|bugged `
#                  [-BaseRef harness-clean] [-ReportsRoot ...]
#   -BaseRef   : git ref for change-set analysis (sonar analyze --base)
#   -AllFiles  : additionally analyze every harness/sample file with --depth DEEP
param(
    [Parameter(Mandatory = $true)][string]$Worktree,
    [Parameter(Mandatory = $true)][ValidateSet('clean', 'bugged')][string]$State,
    [string]$BaseRef,
    [switch]$AllFiles,
    [string]$ReportsRoot = 'C:\projects\parasoft-vs-SQ\reports'
)

$ErrorActionPreference = 'Stop'

$sonar  = Join-Path $env:LOCALAPPDATA 'sonarqube-cli\bin\sonar.exe'
$outDir = Join-Path $ReportsRoot "sonarqube\$State"
New-Item -ItemType Directory -Force $outDir | Out-Null

$stamp = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHHmmssZ')
Push-Location $Worktree
try
{
    if ($BaseRef)
    {
        Write-Host "=== sonar analyze --base $BaseRef --depth DEEP ==="
        & $sonar analyze --base $BaseRef --depth DEEP --format json --force 2>&1 |
            Tee-Object -FilePath (Join-Path $outDir "changeset-$BaseRef-$stamp.json")
        Add-Content (Join-Path $ReportsRoot 'sonarqube\runs.log') `
            "sonar | $State | --base $BaseRef DEEP | $stamp | exit=$LASTEXITCODE"
    }

    if ($AllFiles)
    {
        $files = @()
        $files += Get-ChildItem (Join-Path $Worktree 'ports\win32\vs_2019\example_build\sample_threadx') -Recurse -Include *.c |
                  ForEach-Object { $_.FullName }
        $args = @('analyze', '--depth', 'DEEP', '--format', 'json', '--force')
        foreach ($f in $files) { $args += @('--file', $f) }
        Write-Host ("=== sonar analyze --depth DEEP (" + $files.Count + " files) ===")
        & $sonar @args 2>&1 |
            Tee-Object -FilePath (Join-Path $outDir "allfiles-$stamp.json")
        Add-Content (Join-Path $ReportsRoot 'sonarqube\runs.log') `
            "sonar | $State | --file x$($files.Count) DEEP | $stamp | exit=$LASTEXITCODE"
    }

    Write-Host '=== sonar analyze secrets ==='
    & $sonar analyze secrets $Worktree 2>&1 |
        Tee-Object -FilePath (Join-Path $outDir "secrets-$stamp.log")
    Add-Content (Join-Path $ReportsRoot 'sonarqube\runs.log') `
        "sonar | $State | secrets | $stamp | exit=$LASTEXITCODE"
}
finally
{
    Pop-Location
}
