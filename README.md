# ThreadX Static-Analysis Shootout — Parasoft C/C++test vs SonarQube CLI

This repository is an **independent, reproducible static-analysis tool comparison**,
built on the [Eclipse ThreadX RTOS](https://github.com/eclipse-threadx/threadx)
Win32 sample application (`ports/win32/vs_2019/example_build`).

It answers one engineering question: *on identical, RTOS-realistic C code with a
known ground truth, what do **Parasoft C/C++test (Standard, CLI) Flow Analysis**
and its explicit Thorough follow-up, and the **SonarQube CLI at its highest
analysis level** actually find, and what do they falsely report?*

> **DO NOT use this fork as production ThreadX code.**
> It contains **52 planted candidates**, 50 valid on the Win32 target, as audited
> in the report.
> For production use, go to the pristine upstream: [eclipse-threadx/threadx](https://github.com/eclipse-threadx/threadx).

## Method in one minute

1. **Pristine upstream** is pinned at tag `baseline-clean` (commit `93387b0a`, MIT).
2. An **evaluation sensor-telemetry harness** (`ports/win32/vs_2019/example_build/sample_threadx/harness/`)
   is added: 4 worker threads, byte/block pools, three queues, four mutexes, event
   flags — realistic ThreadX usage intended as the baseline (tag `harness-clean`).
   Post-scan review found one unmarked source-buffer over-read in this baseline.
3. **52 candidates** are injected as small diffs across six family modules (tag
   `bugged`). Every candidate site carries an in-source banner:
   `/* [PLANTED DEFECT BUG-nnn | CWE-x | ...] */`.
4. Both tools scan **both states** with the same machine, toolchain and settings:
   - findings on `harness-clean` measure **false positives** (plus upstream-code findings),
   - findings on `bugged` are mapped to the catalog to score **true positives / false negatives**.
5. The bugged demo survived a 20s soak, but that does not prove every path is
   runtime-safe. The post-scan audit found two invalid candidates and the baseline
   issue; see [`evaluation/PLANTED_BUGS.md`](evaluation/PLANTED_BUGS.md).

## Key artifacts

| Path | Content |
|------|---------|
| `evaluation/PLANTED_BUGS.md` | Ground truth: 52 candidates, 50 valid on Win32, CWEs, sites, trigger paths, rationale, and adjudication |
| `evaluation/SCAN_PROTOCOL.md` | Exact tool versions, commands, settings and raw report locations (audit trail) |
| `evaluation/COMPARISON_REPORT.md` | Results, per-family metrics, false-positive review and SonarQube CLI limitation |
| `evaluation/THREADX_THOROUGH_REPORT.md` | Controlled Thorough rerun: 16/50 without TX conventions versus 20/50 with TX, per-candidate evidence, clean findings, and analysis-timeout limitations |
| `evaluation/THREADX_360_REPORT.md` | Same TX Thorough analysis with a 360-second hotspot limit: still 20/50, fewer timeouts, reviewed additional diagnostics |
| `evaluation/THREADX_ALLFLOW_MISRA_240_REPORT.md` | All 144 Flow rules plus Thorough/240s: 21/50; adding the shipped 388-rule MISRA C:2025 profile adds no strict TPs |
| `evaluation/PARASOFT_RUN_GUIDE.md` | Prepare the source worktrees, run Parasoft C/C++test, retain its HTML/XML reports, and export the adjudicated CSVs |
| `evaluation/reports/COMPARISON_REPORT.pdf` | Printable summary report |
| `evaluation/reports/PARASOFT_VS_SONARQUBE_DECK.pptx` | 30-slide widescreen deck: both tools and the analysis process explained, representative planted-defect code beside actual Parasoft rule IDs and messages, all 50 valid candidates adjudicated, and only the Flow Analysis Aggressive configuration scored; SonarQube's code scan is reported as unavailable (403, not entitled), not as zero findings |
| `evaluation/reports/PARASOFT_VS_SONARQUBE_DECK.pdf` | The same deck as PDF, for reading without PowerPoint |
| [Live HTML edition](https://zuwasi.github.io/Public-html-pages/threadx-parasoft-sonarqube.html) | The same 30 slides on the public pages site, with arrow-key, button and swipe navigation |
| `evaluation/reports/adjudication/DETECTION_MATRIX.csv` | Per-candidate outcome for each available analysis |
| `evaluation/scripts/` | Scan runner scripts used to produce the reports |
| `evaluation/reports/` | Normalized scanner findings, adjudication matrix, run logs, and report hashes; raw Parasoft XML stays local |

## Git states (the audit trail)

| Tag | Commit | Meaning |
|-----|--------|---------|
| `baseline-clean` | `93387b0a` | Pristine upstream ThreadX (v6.5.x line) |
| — | `bb0c450c` | Win32 example_build adapted to the VS2026 toolchain (v145) + build-order fix |
| `harness-clean` | `844260e1` | Intended clean evaluation harness; one baseline issue found post-scan |
| `bugged` | `9d699595` | 52 planted candidates, 50 valid on the evaluated target |
| — | `c109e5db`+ | Ground-truth catalog, protocol, reports |

`git diff harness-clean..bugged` is exactly the defect set.

## Building

Requires Visual Studio 2026 (v145 toolset) with the C++ workload:

```powershell
msbuild ports\win32\vs_2019\example_build\azure_rtos.sln /p:Configuration=Release /p:Platform=Win32 /m:1
```

The sample (`sample_threadx\Release\sample_threadx.exe`) runs the classic ThreadX
demo threads plus the harness threads.

## Defect families

| Family | Count | Emphasis |
|--------|-------|----------|
| Size arithmetic | 12 | truncation, wrap, signed/unsigned, `sizeof(pointer)`, masked checks, off-by-one |
| Memory lifecycle | 10 | UAF, double release, leaks, wrong/interior deallocation, RTOS pool contracts |
| Concurrency | 8 | races, TOCTOU, lock-order inversion, non-owner unlock, racy init |
| NULL / validation paths | 8 | wrong-checked result, unchecked lookups, out-param contracts, deep chains |
| Taint / bounds | 8 | queue-borne indices/lengths, stale bounds, format string, dispatch tables |
| Uninitialized memory | 6 | pool memory, partial init, out-params on error, partial arrays |

## Results

See [`evaluation/COMPARISON_REPORT.md`](evaluation/COMPARISON_REPORT.md) for measured
results. Parasoft produced code findings; SonarQube's DEEP code engine returned
403 Forbidden for this connection, so its per-defect result is unavailable.
The SonarQube code comparison is pending an active license; the 403 result is
not a zero-finding result. Sonar's separate local secrets scan completed with
zero findings in both states.

The 2026-10-11 controlled follow-up explicitly set **Thorough** depth and measured
**16/50** without ThreadX conventions versus **20/50 (40%)** with them. Four new
concurrency detections came from the TX model; one additional uninitialized-memory
detection was present in both Thorough runs. All runs exited 0, but reported
238–240 analysis timeouts each. See
[`evaluation/THREADX_THOROUGH_REPORT.md`](evaluation/THREADX_THOROUGH_REPORT.md).
The existing deck and PDF describe the original run and have not been refreshed.

Increasing the TX Thorough hotspot timeout to **360 seconds** left the score at
**20/50**, while reported timeouts fell to 183 (clean) and 182 (bugged).
See [`evaluation/THREADX_360_REPORT.md`](evaluation/THREADX_360_REPORT.md).

The next controlled rerun enabled all 144 installed Flow checkers, aggressive
options, Thorough depth, and a 240-second hotspot timeout. It detected **21/50**,
newly flagging BUG-023. Adding all 388 checkers in the shipped MISRA C:2025
preset kept the score at **21/50** while increasing clean harness findings from
26 to 558. Both profiles still reported analysis timeouts. This follow-up
has not refreshed the published deck. See
[`evaluation/THREADX_ALLFLOW_MISRA_240_REPORT.md`](evaluation/THREADX_ALLFLOW_MISRA_240_REPORT.md).
To reproduce the Parasoft scan and generate native HTML/XML and normalized CSV
reports, follow [`evaluation/PARASOFT_RUN_GUIDE.md`](evaluation/PARASOFT_RUN_GUIDE.md).

## Credits

- Upstream project: **Eclipse ThreadX RTOS** by Microsoft Corporation and the
  Eclipse ThreadX contributors — MIT license, preserved in [`LICENSE.txt`](LICENSE.txt).
- Upstream README content and documentation live in the upstream repository;
  this fork's root README replaces them for evaluation purposes only.
- Defect harness, ground-truth catalog and evaluation docs: MIT, see
  [`evaluation/PLANTED_BUGS.md`](evaluation/PLANTED_BUGS.md).
