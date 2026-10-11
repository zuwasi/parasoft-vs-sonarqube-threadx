# Scan Protocol — Audit Trail

This document records the exact environment, inputs, commands and outputs of every
scanner run in the comparison.  Runs are added as they complete; nothing is edited
after the fact except appending new sections.

## 1. Machine and toolchain snapshot

| Item | Value |
|------|-------|
| OS | Windows 11 (10.0.26200), x64 |
| Machine id (Parasoft) | WIN32-2037c5fc |
| Visual Studio | Visual Studio Professional 2026, v18.6.11822.322 (VC toolset v145) |
| MSBuild | `C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe` |
| Java (ambient) | OpenJDK 21.0.9 (Temurin); ambient `JAVA_TOOL_OPTIONS` sets `-Xmx512m`, but the Parasoft launcher pins its own `-Xmx4096m` (see `etc/cpptestcli.jvm`), so deep analysis is not throttled |
| Parasoft C/C++test | **Standard 2026.1.3** (10.7.4.20260825B5553), CLI: `C:\CPPTEST-2026_all\STD-2026.1\cpptest\cpptestcli.exe` |
| Parasoft licensing | Network license server at `https://localhost:8443` (Parasoft DTP on the same host), edition `server_compliance_edition` |
| SonarQube CLI | **1.9.0** (1.9.0.15656), `C:\Users\<user>\AppData\Local\sonarqube-cli\bin\sonar.exe`, installed from the official SonarSource install script |
| Git | 2.56.0.windows.2 |

## 2. Input states

Both tools analyze identical materialized states via `git worktree` checkouts of
immutable tags:

| State | Tag | Commit |
|-------|-----|--------|
| Clean baseline | `harness-clean` | `844260e1` |
| Bugged | `bugged` | `9d699595` (docs commits after this do not touch code; scans use the tag) |

Build used for analysis ingestion: `azure_rtos.sln`, configuration `Release|Win32`,
MSVC v145. Parasoft successfully collected build data and generated reports for both
projects. The scan logs, rather than the build transcript, are the evidence for this
run; no compiler-warning counts are used in the comparison.

## 3. Parasoft run plan (maximum depth)

- Test configuration (primary): **`builtin://Flow Analysis Aggressive`** —
  enables all flow-analysis rule families at aggressive depth
  (`BD-PB-*`, `BD-RES-*`, `BD-SECURITY-*`, `BD-TRS-*`, `BD-CO-*`, `BD-API-*`),
  with `aggressiveMode` and `reportUnvalidatedViolations` enabled
  (source: `configs/builtin/Flow Analysis Aggressive.properties` of the install).
- Test configuration (supplementary): **`builtin://Recommended Rules`**
  (curated static + flow rules and metrics; this is a comparison profile, not a
  deeper flow-analysis mode than Aggressive).
- Command shape (per state, per config):
  ```
  cpptestcli -config <CONFIG> -compiler <COMPILER_ID> -input <worktree>\ports\win32\vs_2019\example_build\azure_rtos.sln@Release|Win32 -report <reports>\parasoft\<state> -workspace <isolated workspace>
  ```
  `<COMPILER_ID>` for MSVC v145 is resolved with `-list-compilers` on the first
  licensed run and recorded here.
- Raw Parasoft reports live outside the repository at
  `C:\projects\parasoft-vs-SQ\reports\parasoft\<state>-<config>/`.
  The committed `evaluation/reports/parasoft/` contains normalized XML-to-CSV
  exports; raw XML/HTML are omitted to avoid publishing machine-local paths and
  bulky report payloads. SHA-256 hashes of raw files are recorded separately.

## 4. SonarQube CLI run plan (maximum capability)

- Auth: `sonar auth login` against SonarQube Cloud, EU region, organization
  `zuwasi`; CLI status verified connected via OS keychain. The public repository
  was imported as project `zuwasi_parasoft-vs-sonarqube-threadx` after explicit
  user approval.
- Runs (per state):
  1. Change-set analysis: `sonar analyze --base <tag> --depth DEEP --format json --force`
     — cross-file ("DEEP") analysis of all files changed vs the base state; for the
     bugged state the base is `harness-clean`, so the change set is exactly the
     52 defect sites.
  2. Whole-scope analysis of every harness + sample file:
     `sonar analyze --file <f1> --file <f2> ... --depth DEEP --format json --force`
  3. Local secrets scan: `sonar analyze secrets <C-source-files...>`.
- With `--project zuwasi_parasoft-vs-sonarqube-threadx`, both change-set and
  all-file DEEP attempts returned `Vortex analysis is not available on this
  connection (403 Forbidden)`. The JSON reported all 6 changed C files or all 8
  evaluated C files as skipped. These are **not zero-issue code scans**; Sonar
  code TP/FP/FN values are unavailable.
- `sonar system status` reports authentication healthy but `VORTEX Status: Not
  entitled`. Sonar's [Vortex documentation](https://docs.sonarsource.com/sonarqube-cloud/administering-sonarcloud/ai-features/sonar-vortex)
  says Vortex is part of Sonar Agent Essentials, a separate subscription for
  SonarQube Cloud annual Team or Enterprise plans. The current `zuwasi`
  organization has no Agent Essentials entitlement. A CLI flag change cannot
  grant the entitlement.
- The local secrets scan completed on the same eight C files in each state and
  found zero secrets. An initial full-worktree secrets scan timed out at the CLI's
  30-second limit; narrowing to the eight evaluated C files completed in <1s.

## 5. Completed Parasoft runs

All four runs exited 0. The original log is retained at
`evaluation/reports/parasoft/run-log.txt`; it is also available at
`C:\projects\parasoft-vs-SQ\reports\parasoft\runs.log`.

| State | Configuration | UTC start | UTC end | XML findings | Harness findings | Raw report |
|-------|---------------|-----------|---------|--------------|------------------|------------|
| clean | Flow Analysis Aggressive | 2026-10-08 18:31:58Z | 18:34:05Z | 90 | 6 | `clean-builtin-Flow-Analysis-Aggressive/report.xml` |
| bugged | Flow Analysis Aggressive | 2026-10-08 18:34:06Z | 18:36:16Z | 111 | 27 | `bugged-builtin-Flow-Analysis-Aggressive/report.xml` |
| clean | Recommended Rules | 2026-10-08 18:36:18Z | 18:38:10Z | 20 (+12 metric violations) | 5 | `clean-builtin-Recommended-Rules/report.xml` |
| bugged | Recommended Rules | 2026-10-08 18:38:12Z | 18:40:07Z | 36 (+12 metric violations) | 21 | `bugged-builtin-Recommended-Rules/report.xml` |

The XML finding exports and summary are under `evaluation/reports/parasoft/`.
The 12 Recommended Rules metric violations are ThreadX upstream complexity /
nesting metrics, not harness defect findings.

## 6. Sonar CLI run log and outcomes

| State | Command | UTC | Exit | Outcome |
|-------|---------|-----|------|---------|
| clean | `--file x8 --depth DEEP` (no project) | 18:48:45Z | 0 | No project configured; Vortex skipped |
| clean | `analyze secrets` on worktree | 18:48:45Z | 1 | Timed out after 30s |
| clean | `--file x8 --depth DEEP` (no project) | 18:51:58Z | 0 | Vortex skipped |
| clean | `analyze secrets` on 8 C files | 18:51:58Z | 0 | 0 secrets |
| bugged | `--base harness-clean --depth DEEP` (no project) | 18:53:54Z | 0 | Vortex skipped |
| bugged | `--file x8 --depth DEEP` (no project) | 18:53:54Z | 0 | Vortex skipped |
| bugged | `analyze secrets` on 8 C files | 18:53:54Z | 0 | 0 secrets |
| clean | project-backed `--file x8 --depth DEEP` | 18:58:08Z | 1 | Vortex 403; 8 files skipped |
| clean | `analyze secrets` on 8 C files | 18:58:08Z | 0 | 0 secrets |
| bugged | project-backed `--base harness-clean --depth DEEP` | 18:58:23Z | 1 | Vortex 403; 6 files skipped |
| bugged | project-backed `--file x8 --depth DEEP` | 18:58:23Z | 1 | Vortex 403; 8 files skipped |
| bugged | `analyze secrets` on 8 C files | 18:58:23Z | 0 | 0 secrets |
| clean + bugged | `sonar system status` | after 18:58Z | 0 | Auth healthy; Vortex not entitled |

The run log is committed as `evaluation/reports/sonarqube/run-log.txt`.
Complete CLI JSON/log output and the first timeout record are also retained in
the local `C:\projects\parasoft-vs-SQ\reports\sonarqube\` directory; compact
results are committed under `evaluation/reports/sonarqube/`.

## 7. Known limitations

- Parasoft was run against a VS2026/MSVC v145 Win32 build-data scope. The results
  include many upstream ThreadX findings; the report separates them from harness
  findings. The Recommended Rules totals include metric violations.
- The intended clean harness contains an unmarked source-buffer over-read; see
  `PLANTED_BUGS.md` and its baseline finding row in the comparison report.
- Two planted candidates (BUG-005 and BUG-031) were invalid on the evaluated
  target and are excluded from valid-defect recall.
- SonarQube CLI's local secrets scanner is not a C/C++ flow analyzer. The CLI's
  project-backed Vortex code engine is not entitled (403), so no cross-vendor
  TP/FP/FN comparison is possible on the current connection. Sonar Agent
  Essentials or a supported entitled Server connection is required.

## 8. Explicit Thorough depth and ThreadX model follow-up, 2026-10-11

The original Aggressive preset did not explicitly set `depthOfAnalysis`.
Earlier descriptions of it as a verified maximum/Thorough-depth scan were not
supported by that configuration. The installed configuration schema labels
`depthOfAnalysis=4` as **Thorough (slowest)**.

Two separate configurations under `evaluation/configs/` retain Aggressive rules
and explicitly set depth `4`: `Flow Analysis Aggressive Thorough.properties`
and `TX_Flow Analysis Aggressive Thorough.properties`. Only the latter adds the
TX multithreading API. Neither changes resource conventions or source tags.

Each used the original solution/Win32 build scope, `vc_14_5`, and a new workspace.
All four runs checked 214 files and exited 0:

| Model | State | UTC start | UTC end | XML findings | Harness findings | Analysis timeouts |
|---|---|---|---|---:|---:|---:|
| Control | clean | 12:38:30Z | 12:44:16Z | 121 | 7 | 239 |
| Control | bugged | 12:44:16Z | 12:50:02Z | 143 | 29 | 238 |
| TX | clean | 12:50:02Z | 12:55:49Z | 297 | 18 | 240 |
| TX | bugged | 12:55:49Z | 13:01:43Z | 324 | 40 | 240 |

Timeout strategy remained the default **time, 60 seconds per hotspot**. Reports
are therefore completed CLI runs, not proof of exhaustive path exploration.
The control scored **16/50 strict TP**, and TX scored **20/50 strict TP**.
See `THREADX_THOROUGH_REPORT.md` and `reports/threadx-thorough/` for adjudication,
configuration/raw-report hashes, and full timestamps.

Raw evidence remains at
`C:\projects\parasoft-vs-SQ\reports\thorough-20261011T123829Z`.
The preliminary `tx-aware-20261011T123457Z` attempt before the explicit-depth
clarification is retained locally and is not scored here.

## 9. TX Thorough with 360-second hotspot limit, 2026-10-11

`evaluation/configs/TX_Flow Analysis Aggressive Thorough 360s.properties`
retains the TX model and depth `4`, explicitly sets timeout strategy `time`,
and increases `hotSpotTimeoutSeconds` from the default 60 to **360**.
Both new console logs confirm the active 360-second limit.

Same source commits, `vc_14_5`, solution scope, and 214 checked files;
separate workspaces and reports:

| State | UTC start | UTC end | Exit | XML findings | Harness findings | Timeouts |
|---|---|---|---:|---:|---:|---:|
| clean | 13:30:56Z | 13:53:30Z | 0 | 317 | 20 | 183 |
| bugged | 13:53:30Z | 14:15:53Z | 0 | 344 | 49 | 182 |

Strict detections remain **20/50**, with **30 strict FN**. The added diagnostics
were reviewed and do not detect additional planted root causes. Raw evidence is
retained at `C:\projects\parasoft-vs-SQ\reports\tx-thorough-360-20261011T133054Z`.
See `THREADX_360_REPORT.md` and `reports/threadx-thorough-360/` for the full
comparison, row-level reviews, and hashes. Remaining timeouts are disclosed,
not treated as a completed exhaustive analysis.

## 10. All Flow checkers and MISRA C:2025 at Thorough/240s, 2026-10-11

This controlled follow-up starts from the same immutable `harness-clean`
(`844260e1497fecffeb05c41cdf94e57044afee6e`) and `bugged`
(`9d6995958d8e7618e570e4724083e5a3edfe934b`) commits and retains the same
ThreadX custom multithreading API model.

The installed Flow schema contains 144 checker IDs. Both configurations enable
all 144, the 25 available aggressive/unvalidated-reporting options, and the two
additional NOTINIT reporting options
`BD-PB-NOTINIT-reportIfPartialNoninitializedCopied` and
`BD-PB-NOTINIT-reportOnPotentiallyNonInitArrayElement`. The second configuration
also enables all 388 checker IDs in the shipped `MISRA C 2025.properties`.
Checker IDs and the MISRA-to-underlying-checker map are inventoried in
`configs/ALL_FLOW_MISRA_240_INVENTORY.csv`.

Every scan explicitly sets `depthOfAnalysis=4` (Thorough),
`hotSpotTimeoutStrategy=time`, and `hotSpotTimeoutSeconds=240`. Runs use fresh
workspaces, the same `vc_14_5` compiler and
`azure_rtos.sln@Release|Win32` input; the CLI reports 214 files checked.

| Profile | State | Start UTC | End UTC | Exit | XML findings | Harness findings | Flow timeouts |
|---|---|---|---|---:|---:|---:|---:|
| All Flow | clean | 15:13:35Z | 15:29:44Z | 0 | 324 | 26 | 187 |
| All Flow | bugged | 15:29:45Z | 15:45:40Z | 0 | 354 | 49 | 183 |
| All Flow + MISRA C:2025 | clean | 15:45:41Z | 16:08:18Z | 0 | 4,661 | 558 | 210 |
| All Flow + MISRA C:2025 | bugged | 16:08:19Z | 16:29:59Z | 0 | 4,745 | 615 | 206 |

All Flow rules produce **21/50 strict planted-defect TPs**. Enabling every rule
newly identifies BUG-023 through `BD-TRS-MLOCK`; its matching
`MISRAC2025-DIR_5_1-a` diagnostic is the same root cause and is deduplicated.
The additional MISRA checkers do not raise the strict score beyond 21/50. MISRA
compliance diagnostics are not counted as planted-defect detections without a
source-level match to the defect's root cause.

The Flow-only profile has 26 clean harness findings, all individually reviewed
in `reports/threadx-allflow-misra-240/CLEAN_FLOW_REVIEW.csv`. The combined
profile reports 558 clean harness entries (505 `StdViol`, 53 `FlowViol`), many
of which are compliance findings or aliases; they are not 558 unique defects.
The bugged combined profile reports 615 harness entries. High diagnostic volume
does not imply increased defect recall.

The export script, per-candidate matrix, four normalized findings CSVs, scan
summary and raw-report hashes are in `evaluation/scripts/` and
`evaluation/reports/threadx-allflow-misra-240/`. Raw XML and console logs remain
local at `C:\projects\parasoft-vs-SQ\reports\allflow-misra240-20261011T151333Z`.
Despite exit code 0, the scans still report 183–210 hotspot timeouts; results
are not claimed exhaustive. The historical PDF and presentation have not been
refreshed.

## 11. Reproducing the Parasoft run and reports

`PARASOFT_RUN_GUIDE.md` documents the required Parasoft/Visual Studio licenses,
the immutable source worktrees, how to run the four controlled scans, and how
to export the reviewed matrix. `scripts/run-threadx-allflow-misra-240.ps1` is
the checked-in PowerShell driver; it records source commits and config hashes
and keeps each Parasoft workspace separate. Parasoft creates `report.html`,
`report.xml`, and a console log per run. The Python exporter writes normalized
finding CSVs and the adjudicated per-candidate matrix to a new output directory.

The SonarQube DEEP code comparison is intentionally pending the license
entitlement; the previously recorded 403 is not a zero-finding result. No
Sonar code result should be added to the comparison until the licensed scan has
completed on both immutable source states.
