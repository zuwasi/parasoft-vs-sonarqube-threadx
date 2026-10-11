# Running Parasoft C/C++test and exporting the ThreadX results

This guide reproduces the Flow-only and Flow-plus-MISRA C:2025 CLI runs and
generates both Parasoft's native reports and the reviewed benchmark matrix.
**SonarQube code analysis is pending the required license entitlement.** Do not
treat the earlier SonarQube 403 as a zero-finding result; rerun that comparison
only after the license is active.

## Requirements

- Windows with Visual Studio 2026 C++ tools and the `vc_14_5` compiler installed.
- Parasoft C/C++test Standard CLI 2026.1.3 (tested with the 2026.1 installation),
  a valid Flow Analysis license, and the MISRA Rules license for the combined
  profile.
- Git, Python 3, and PowerShell 7.
- This repository and the two immutable source states listed below. Keep source
  snapshots clean; the driver refuses dirty or unexpected revisions.

Default paths match the original test environment. Override the script
parameters when the tools or source worktrees live elsewhere.

## Prepare the source worktrees

The benchmark tags are in this repository:

| Worktree directory | Tag | Required commit |
|---|---|---|
| `harness-clean` | `harness-clean` | `844260e1497fecffeb05c41cdf94e57044afee6e` |
| `bugged` | `bugged` | `9d6995958d8e7618e570e4724083e5a3edfe934b` |

From the repository root, create detached worktrees in a separate directory.
Use new, empty paths; do not check out these revisions over a working tree with
changes.

```powershell
$repo = (Get-Location).Path
$states = 'C:\projects\parasoft-vs-SQ\states'
New-Item -ItemType Directory -Force -Path $states | Out-Null
git -C $repo worktree add --detach "$states\harness-clean" harness-clean
git -C $repo worktree add --detach "$states\bugged" bugged
git -C "$states\harness-clean" rev-parse HEAD
git -C "$states\bugged" rev-parse HEAD
```

If the tags are missing from a fresh clone, fetch tags from the comparison
repository's `origin`, then repeat the commands. Do not use the unrelated
Eclipse ThreadX remote as the benchmark source.

## Run the four controlled scans

The checked-in driver uses the two versioned profiles in `evaluation/configs/`,
sets up the Visual Studio compiler environment, checks source cleanliness and
commit IDs, and creates a separate Parasoft report directory and workspace for
each profile/state pair.

```powershell
$repo = (Get-Location).Path
& "$repo\evaluation\scripts\run-threadx-allflow-misra-240.ps1" `
  -CppTestRoot 'C:\CPPTEST-2026_all\STD-2026.1\cpptest' `
  -StatesRoot 'C:\projects\parasoft-vs-SQ\states' `
  -VsVars 'C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars32.bat' `
  -OutputRoot 'D:\ParasoftReports\threadx-allflow-misra-240-rerun1'
```

`-OutputRoot` must not already exist. Choose a unique path on storage with
enough space for four XML/HTML reports and four workspaces. If omitted, output
goes to a unique directory under the current user's temporary directory; copy
it to durable storage before clearing temporary files.

The driver runs the same `azure_rtos.sln@Release|Win32` input with compiler
`vc_14_5`, first using all 144 Flow checker IDs, then adding all 388 checkers in
the shipped MISRA C:2025 preset. Each profile runs against both immutable source
states. The profiles also enable Thorough depth, aggressive/unvalidated
reporting options, the two extra NOTINIT reporting options, the same ThreadX
multithreading model, and a 240-second hotspot limit.

The driver prints `RUN_ROOT=...` when complete. Each run folder contains:

- `report.html`: Parasoft's browsable report.
- `report.xml`: machine-readable Parasoft findings.
- `console.log`: CLI output, license status, checked-file count, and timeout
  summary.
- Parasoft logo/summary assets used by the HTML report.

`runs.json` at the root records the source commit, profile hash, timestamps,
exit code, and report/workspace locations. All scans must exit 0, but also check
the `files checked` line and every `N timeouts occurred during Flow Analysis`
message. A successful CLI exit does not imply that no analysis paths timed out.

## Run a single Parasoft scan directly

To run just one profile and source state instead, use a fresh output directory
and workspace. For example:

```powershell
$repo = (Get-Location).Path
$cli = 'C:\CPPTEST-2026_all\STD-2026.1\cpptest\cpptestcli.exe'
$source = 'C:\projects\parasoft-vs-SQ\states\bugged'
$solution = Join-Path $source 'ports\win32\vs_2019\example_build\azure_rtos.sln'
$config = Join-Path $repo 'evaluation\configs\TX_All Flow Aggressive Thorough 240s.properties'
$report = 'D:\ParasoftReports\threadx-one-run'
$workspace = 'D:\ParasoftWorkspaces\threadx-one-run'
New-Item -ItemType Directory -Path $report, $workspace | Out-Null

& $cli -config $config -compiler vc_14_5 `
  -input "$solution@Release|Win32" `
  -report $report -workspace $workspace -showdetails *> (Join-Path $report 'console.log')
if ($LASTEXITCODE -ne 0) { throw "C++test failed; inspect $report\console.log" }
```

Use `TX_All Flow plus MISRA C2025 Thorough 240s.properties` for the combined
profile. Initialize the Visual Studio environment first if `cl.exe` is not
available in the current shell. Do not reuse a C++test workspace between runs.

## Export an adjudicated comparison

To export normalized CSV findings and the per-defect matrix after the four-run
driver completes, pass its printed output root to the exporter. The destination
must be new. The exporter checks the recorded settings, report hashes, and
source scope; it preserves prior detections and credits a TP only for a
source-level match to the planted root cause.

```powershell
$repo = (Get-Location).Path
$raw = 'D:\ParasoftReports\threadx-allflow-misra-240-rerun1'
$out = Join-Path $repo ('evaluation\reports\threadx-allflow-misra-240-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
python "$repo\evaluation\scripts\export-threadx-allflow-misra-240.py" `
  $raw --states-root 'C:\projects\parasoft-vs-SQ\states' --output $out
```

The output includes `DETECTION_MATRIX.csv`, `run-summary.csv`, a clean Flow
review, and normalized findings for all four runs. Keep the native XML/HTML
reports as the Parasoft source evidence; the CSV export supplements rather than
replaces them. The committed reference export is in
`evaluation/reports/threadx-allflow-misra-240/`.

For auditability, verify the profile hashes and expected source commit IDs in
the run summary before comparing a rerun. Record any intentional change in
profile, compiler, target, source state, or timeout as a new experiment. Do not
silently merge its results into the existing 21/50 score.
