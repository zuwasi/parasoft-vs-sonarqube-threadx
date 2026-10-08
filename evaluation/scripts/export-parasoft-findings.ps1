# Export normalized Parasoft XML findings from a pair of clean/bugged runs.
# Usage:
#   .\export-parasoft-findings.ps1 `
#     -InputRoot C:\projects\parasoft-vs-SQ\reports\parasoft
param(
    [string]$InputRoot = 'C:\projects\parasoft-vs-SQ\reports\parasoft',
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\reports\parasoft')
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$summary = foreach ($run in Get-ChildItem -LiteralPath $InputRoot -Directory)
{
    $reportPath = Join-Path $run.FullName 'report.xml'
    if (-not (Test-Path -LiteralPath $reportPath)) { continue }

    [xml]$report = Get-Content -LiteralPath $reportPath -Raw
    $findings = @($report.SelectNodes('//FlowViol | //StdViol'))
    $state = if ($run.Name.StartsWith('clean-')) { 'clean' } else { 'bugged' }
    $config = $run.Name.Substring($state.Length + 1)
    $csvPath = Join-Path $OutputRoot "$($run.Name)-findings.csv"

    $rows = foreach ($finding in $findings)
    {
        $a = $finding.Attributes
        [pscustomobject]@{
            Run             = $run.Name
            State           = $state
            Configuration   = $config
            FindingType     = $finding.LocalName
            Rule            = $a['rule'].Value
            Severity        = $a['sev'].Value
            File            = $a['locFile'].Value
            Line            = $a['locStartln'].Value
            Message         = ($a['msg'].Value -replace '\s+', ' ').Trim()
            PathStartFile   = $a['FirstElSrcRngFile'].Value
            PathStartLine   = $a['FirstElSrcRngStartln'].Value
            PathEndLine     = $a['FirstElSrcRngEndLn'].Value
            Analyzer        = $a['analyzer'].Value
            ViolationId     = $a['violId'].Value
        }
    }

    $rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding utf8

    $harness = @($findings | Where-Object {
        $_.Attributes['locFile'].Value -match '/sample_threadx/harness/'
    })
    $flow = @($findings | Where-Object { $_.LocalName -eq 'FlowViol' })
    $standard = @($findings | Where-Object { $_.LocalName -eq 'StdViol' })

    [pscustomobject]@{
        Run                 = $run.Name
        State               = $state
        Configuration       = $config
        XmlFindings         = $findings.Count
        FlowViolations      = $flow.Count
        StandardViolations  = $standard.Count
        HarnessFindings     = $harness.Count
        OtherFindings       = $findings.Count - $harness.Count
        FindingsCsv         = Split-Path -Leaf $csvPath
    }
}

$summary | Export-Csv -LiteralPath (Join-Path $OutputRoot 'run-summary.csv') `
    -NoTypeInformation -Encoding utf8
