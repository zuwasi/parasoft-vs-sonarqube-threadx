# Runs Parasoft C/C++test static analysis on one materialized git state.
# Usage:
#   .\run-parasoft.ps1 -Worktree <path-to-state-checkout> -State clean|bugged `
#                      -Config "builtin://Flow Analysis Aggressive" -Compiler <compiler-id> `
#                      [-ReportsRoot C:\projects\parasoft-vs-SQ\reports]
param(
    [Parameter(Mandatory = $true)][string]$Worktree,
    [Parameter(Mandatory = $true)][ValidateSet('clean', 'bugged')][string]$State,
    [Parameter(Mandatory = $true)][string]$Config,
    [Parameter(Mandatory = $true)][string]$Compiler,
    [string]$ReportsRoot = 'C:\projects\parasoft-vs-SQ\reports'
)

$ErrorActionPreference = 'Stop'

$cli       = 'C:\CPPTEST-2026_all\STD-2026.1\cpptest\cpptestcli.exe'
$sln       = Join-Path $Worktree 'ports\win32\vs_2019\example_build\azure_rtos.sln'
$outDir    = Join-Path $ReportsRoot "parasoft\$State"
$workspace = Join-Path $ReportsRoot "parasoft-ws-$State"

New-Item -ItemType Directory -Force $outDir    | Out-Null
New-Item -ItemType Directory -Force $workspace | Out-Null

$configTag = ($Config -replace '[^A-Za-z0-9]+', '-').Trim('-')
$stamp  = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHHmmssZ')
$logLine = "parasoft | $State | $Config | start=$stamp"

Write-Host "=== Parasoft run: state=$State config=$Config ==="
Write-Host "worktree : $Worktree"
Write-Host "solution : $sln"
Write-Host "report   : $outDir"

& $cli -config $Config -compiler $Compiler `
       -input "$sln@Release|Win32" `
       -report $outDir `
       -workspace $workspace `
       -showdetails 2>&1 | Tee-Object -FilePath (Join-Path $outDir "console-$configTag-$stamp.log")

$exit = $LASTEXITCODE
$end  = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHHmmssZ')
$logLine = "$logLine end=$end exit=$exit"

$runsLog = Join-Path $ReportsRoot 'parasoft\runs.log'
Add-Content -Path $runsLog -Value $logLine
Write-Host "run recorded: $logLine"
exit $exit
