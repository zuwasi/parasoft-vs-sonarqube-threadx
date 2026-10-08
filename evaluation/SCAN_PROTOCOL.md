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
| Parasoft licensing | Network license server at `https://localhost:8443` (Parasoft DTP on the same host), edition `server_compliance_edition`, user `admin` |
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
MSVC v145.  Both projects compile without errors (3 MSVC C4996 deprecation warnings
on deliberate `strcpy` sites, 2 C4700 warnings on deliberate uninit defects —
recorded as free baseline-compiler evidence in the report).

## 3. Parasoft run plan (maximum depth)

- Test configuration (primary): **`builtin://Flow Analysis Aggressive`** —
  enables all flow-analysis rule families at aggressive depth
  (`BD-PB-*`, `BD-RES-*`, `BD-SECURITY-*`, `BD-TRS-*`, `BD-CO-*`, `BD-API-*`),
  with `aggressiveMode` and `reportUnvalidatedViolations` enabled
  (source: `configs/builtin/Flow Analysis Aggressive.properties` of the install).
- Test configuration (supplementary): **`builtin://Recommended Rules`**
  (curated static + flow rule mix) to cover rule-based detections at the
  "highest scanning level".
- Command shape (per state, per config):
  ```
  cpptestcli -config <CONFIG> -compiler <COMPILER_ID> -input <worktree>\ports\win32\vs_2019\example_build\azure_rtos.sln@Release|Win32 -report <reports>\parasoft\<state> -workspace <isolated workspace>
  ```
  `<COMPILER_ID>` for MSVC v145 is resolved with `-list-compilers` on the first
  licensed run and recorded here.
- Reports: HTML report plus machine-readable findings (XML) under
  `evaluation/reports/parasoft/<state>/`; each run appends its exact command,
  UTC start/end time and exit code to `evaluation/reports/parasoft/runs.log`.

## 4. SonarQube CLI run plan (maximum capability)

- Auth: `sonar auth login` against SonarQube Cloud (public project, free tier).
  Observation already recorded: on CLI 1.9.0 **all** `sonar analyze` commands,
  including `analyze secrets`, refuse to run without authentication
  ("Not authenticated"), despite docs describing secrets detection as local.
- Runs (per state):
  1. Change-set analysis: `sonar analyze --base <tag> --depth DEEP --format json --force`
     — cross-file ("DEEP") analysis of all files changed vs the base state; for the
     bugged state the base is `harness-clean`, so the change set is exactly the
     52 defect sites.
  2. Whole-scope analysis of every harness + sample file:
     `sonar analyze --file <f1> --file <f2> ... --depth DEEP --format json --force`
  3. Secrets: `sonar analyze secrets <worktree>`
- Any server-side requirement surfaced at run time (project link, plan gating,
  Vortex/subscription availability for C) is recorded verbatim in the run log
  and reflected honestly in the comparison report.

## 5. Run log

| # | Tool | State | Config | Command summary | UTC start | UTC end | Exit | Findings file |
|---|------|-------|--------|-----------------|-----------|---------|------|----------------|
| (pending) | | | | | | | | |

## 6. Known open items

- Parasoft license server (DTP) must be running for any `cpptestcli` invocation;
  `-list-configs` on this install aborts without it.
- SonarQube CLI requires a completed `sonar auth login` for every analysis.
- Compiler id for MSVC v145 to be captured from `-list-compilers`.
