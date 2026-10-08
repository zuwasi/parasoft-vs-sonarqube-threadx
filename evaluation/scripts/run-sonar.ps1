# Runs SonarQube CLI analysis on one materialized git state.
# Usage:
#   .\run-sonar.ps1 -Worktree <path-to-state-checkout> -State clean|bugged `
#                  [-ProjectKey <SonarQube project key>] `
#                  [-BaseRef harness-clean] [-ReportsRoot ...]
#   -BaseRef   : git ref for change-set analysis (sonar analyze --base)
#   -AllFiles  : additionally analyze every harness/sample file with --depth DEEP
#   -ProjectKey: SonarQube project required for server-side code analysis
param(
    [Parameter(Mandatory = $true)][string]$Worktree,
    [Parameter(Mandatory = $true)][ValidateSet('clean', 'bugged')][string]$State,
    [string]$ProjectKey,
    [string]$BaseRef,
    [switch]$AllFiles,
    [string]$ReportsRoot = 'C:\projects\parasoft-vs-SQ\reports'
)

$ErrorActionPreference = 'Stop'

$sonar  = Join-Path $env:LOCALAPPDATA 'sonarqube-cli\bin\sonar.exe'
$outDir = Join-Path $ReportsRoot "sonarqube\$State"
New-Item -ItemType Directory -Force $outDir | Out-Null
$sourceDir = Join-Path $Worktree 'ports\win32\vs_2019\example_build\sample_threadx'
$files = @(Get-ChildItem $sourceDir -Recurse -Include *.c |
           ForEach-Object { $_.FullName })

$stamp = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHHmmssZ')
Push-Location $Worktree
try
{
    if ($BaseRef)
    {
        Write-Host "=== sonar analyze --base $BaseRef --depth DEEP ==="
        $cliArgs = @('analyze')
        if ($ProjectKey) { $cliArgs += @('--project', $ProjectKey) }
        $cliArgs += @('--base', $BaseRef, '--depth', 'DEEP', '--format', 'json', '--force')
        & $sonar @cliArgs 2>&1 |
            Tee-Object -FilePath (Join-Path $outDir "changeset-$BaseRef-$stamp.json")
        Add-Content (Join-Path $ReportsRoot 'sonarqube\runs.log') `
            "sonar | $State | --base $BaseRef DEEP | $stamp | exit=$LASTEXITCODE"
    }

    if ($AllFiles)
    {
        $cliArgs = @('analyze')
        if ($ProjectKey) { $cliArgs += @('--project', $ProjectKey) }
        $cliArgs += @('--depth', 'DEEP', '--format', 'json', '--force')
        foreach ($f in $files) { $cliArgs += @('--file', $f) }
        Write-Host ("=== sonar analyze --depth DEEP (" + $files.Count + " files) ===")
        & $sonar @cliArgs 2>&1 |
            Tee-Object -FilePath (Join-Path $outDir "allfiles-$stamp.json")
        Add-Content (Join-Path $ReportsRoot 'sonarqube\runs.log') `
            "sonar | $State | --file x$($files.Count) DEEP | $stamp | exit=$LASTEXITCODE"
    }

    Write-Host '=== sonar analyze secrets ==='
    # Keep the secrets scan on the exact C source scope under evaluation.
    # Scanning the full upstream ThreadX checkout exceeds the CLI's 30s limit.
    & $sonar analyze secrets @files 2>&1 |
        Tee-Object -FilePath (Join-Path $outDir "secrets-$stamp.log")
    Add-Content (Join-Path $ReportsRoot 'sonarqube\runs.log') `
        "sonar | $State | secrets | $stamp | exit=$LASTEXITCODE"
}
finally
{
    Pop-Location
}
