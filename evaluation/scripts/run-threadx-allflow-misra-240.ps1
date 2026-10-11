param(
    [string]$CppTestRoot = 'C:\CPPTEST-2026_all\STD-2026.1\cpptest',
    [string]$StatesRoot = 'C:\projects\parasoft-vs-SQ\states',
    [string]$VsVars = 'C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat',
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
if (-not $gitCommand) { throw 'git.exe must be on PATH.' }
$git = $gitCommand.Source
$cli = Join-Path $CppTestRoot 'cpptestcli.exe'
if (-not (Test-Path -LiteralPath $cli)) { throw "C++test CLI not found: $cli" }
if (-not (Test-Path -LiteralPath $VsVars)) { throw "VS environment script not found: $VsVars" }
if (-not $OutputRoot) {
    $OutputRoot = Join-Path ([IO.Path]::GetTempPath()) (
        'parasoft-threadx-allflow-misra240-' + (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
    )
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "Output already exists; use a new path: $OutputRoot" }
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
Set-Location -LiteralPath $OutputRoot

# Load the Visual Studio compiler environment into this PowerShell process.
$environment = cmd /c "call `"$VsVars`" >nul 2>&1 && set"
if ($LASTEXITCODE -ne 0) { throw "Failed to initialize Visual Studio using $VsVars" }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'cl.exe is unavailable after VS setup.' }

$expectedCommits = @{
    clean  = '844260e1497fecffeb05c41cdf94e57044afee6e'
    bugged = '9d6995958d8e7618e570e4724083e5a3edfe934b'
}
$runs = @()
foreach ($variant in @('flow', 'misra')) {
    $configuration = if ($variant -eq 'flow') {
        'TX_All Flow Aggressive Thorough 240s'
    } else {
        'TX_All Flow plus MISRA C2025 Thorough 240s'
    }
    $config = Join-Path $repo "evaluation\configs\$configuration.properties"
    if (-not (Test-Path -LiteralPath $config)) { throw "Configuration not found: $config" }
    $configText = [IO.File]::ReadAllText($config)
    foreach ($setting in @(
        'com.parasoft.xtest.checker.flowanalysis.depthOfAnalysis=4',
        'com.parasoft.xtest.checker.flowanalysis.hotSpotTimeoutStrategy=time',
        'com.parasoft.xtest.checker.flowanalysis.hotSpotTimeoutSeconds=240'
    )) {
        if ($configText -notmatch ('(?m)^' + [regex]::Escape($setting) + '\s*$')) {
            throw "Required scan setting missing: $setting"
        }
    }
    $configHash = (Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash.ToLowerInvariant()

    foreach ($state in @('clean', 'bugged')) {
        $sourceRoot = Join-Path $StatesRoot $(if ($state -eq 'clean') { 'harness-clean' } else { 'bugged' })
        if (-not (Test-Path -LiteralPath $sourceRoot)) { throw "Source snapshot not found: $sourceRoot" }
        $dirty = & $git -C $sourceRoot status --porcelain
        if ($LASTEXITCODE -ne 0 -or $dirty) { throw "Source snapshot must be clean: $sourceRoot" }
        $commit = & $git -C $sourceRoot rev-parse HEAD
        if ($LASTEXITCODE -ne 0 -or $commit -ne $expectedCommits[$state]) {
            throw "Unexpected source commit in $sourceRoot (got $commit)"
        }
        if ((Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash.ToLowerInvariant() -ne $configHash) {
            throw "Configuration changed during the scan: $config"
        }

        $solution = Join-Path $sourceRoot 'ports\win32\vs_2019\example_build\azure_rtos.sln'
        $report = Join-Path $OutputRoot "$variant-$state"
        $workspace = Join-Path $OutputRoot "workspace-$variant-$state"
        New-Item -ItemType Directory -Path $report, $workspace | Out-Null
        $consoleLog = Join-Path $report 'console.log'
        $start = (Get-Date).ToUniversalTime().ToString('o')
        & $cli -config $config -compiler vc_14_5 -input "$solution@Release|Win32" `
            -report $report -workspace $workspace -showdetails *> $consoleLog
        $exitCode = $LASTEXITCODE
        $end = (Get-Date).ToUniversalTime().ToString('o')
        $runs += [pscustomobject]@{
            Variant = $variant
            State = $state
            Tag = $(if ($state -eq 'clean') { 'harness-clean' } else { 'bugged' })
            Commit = $commit
            Configuration = $configuration
            ConfigurationSHA256 = $configHash
            Compiler = 'vc_14_5'
            Depth = 'Thorough'
            DepthValue = 4
            TimeoutStrategy = 'time'
            TimeoutSeconds = 240
            FlowCheckersEnabled = 144
            MisraCheckersEnabled = $(if ($variant -eq 'misra') { 388 } else { 0 })
            StartUTC = $start
            EndUTC = $end
            Exit = $exitCode
            Report = $report
            Workspace = $workspace
            Log = $consoleLog
        }
        $runs | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $OutputRoot 'runs.json') -Encoding utf8NoBOM
        Write-Output "$variant/$state exit=$exitCode report=$report"
        if ($exitCode -ne 0) { throw "C++test failed; inspect $consoleLog" }
        if (-not (Test-Path -LiteralPath (Join-Path $report 'report.xml'))) {
            throw "C++test exited 0 but report.xml is missing: $report"
        }
        if (-not (Test-Path -LiteralPath (Join-Path $report 'report.html'))) {
            Write-Warning "HTML report was not generated by this installation: $report"
        }
    }
}
Write-Output "RUN_ROOT=$OutputRoot"
