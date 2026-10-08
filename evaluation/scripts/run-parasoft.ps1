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

# cpptestcli re-invokes cl.exe for preprocessing, so the VS x86 developer
# environment must be present.  vcvars32.bat is started from a minimal PATH:
# entries in the long user PATH break its batch parsing, which silently drops
# the Windows SDK / UCRT include directories (stdlib.h not found).
$vcvars = 'C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat'
foreach ($l in (cmd /c "set `"PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem`" && call `"$vcvars`" >nul 2>&1 && set"))
{
    if ($l -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'cl.exe not found after vcvars32' }

$cli       = 'C:\CPPTEST-2026_all\STD-2026.1\cpptest\cpptestcli.exe'
$sln       = Join-Path $Worktree 'ports\win32\vs_2019\example_build\azure_rtos.sln'
$configTag = ($Config -replace '[^A-Za-z0-9]+', '-').Trim('-')
$outDir    = Join-Path $ReportsRoot "parasoft\$State-$configTag"
$workspace = Join-Path $ReportsRoot "parasoft-ws-$State-$configTag"

New-Item -ItemType Directory -Force $outDir    | Out-Null
New-Item -ItemType Directory -Force $workspace | Out-Null
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
