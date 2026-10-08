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
  project-backed Vortex code engine was forbidden (403), so no cross-vendor
  TP/FP/FN comparison is possible in this connection.
