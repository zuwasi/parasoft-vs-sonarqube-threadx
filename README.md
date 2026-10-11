# ThreadX Static-Analysis Shootout — Parasoft C/C++test vs SonarQube CLI

This repository is an **independent, reproducible static-analysis tool comparison**,
built on the [Eclipse ThreadX RTOS](https://github.com/eclipse-threadx/threadx)
Win32 sample application (`ports/win32/vs_2019/example_build`).

It answers one engineering question: *on identical, RTOS-realistic C code with a
known ground truth, what do **Parasoft C/C++test (Standard, CLI) Flow Analysis at
maximum depth** and the **SonarQube CLI at its highest analysis level** actually
find, and what do they falsely report?*

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
| `evaluation/reports/COMPARISON_REPORT.pdf` | Printable summary report |
| `evaluation/reports/PARASOFT_VS_SONARQUBE_DECK.pptx` | 30-slide widescreen deck: each planted defect's code beside the Parasoft rule ID and message that did or did not flag it, both tools introduced, the two rule profiles and the analysis process explained, and the SonarQube code scan reported as unavailable (403, not entitled) instead of as a zero-finding result |
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
Sonar's local secrets scan completed with zero findings in both states.

## Credits

- Upstream project: **Eclipse ThreadX RTOS** by Microsoft Corporation and the
  Eclipse ThreadX contributors — MIT license, preserved in [`LICENSE.txt`](LICENSE.txt).
- Upstream README content and documentation live in the upstream repository;
  this fork's root README replaces them for evaluation purposes only.
- Defect harness, ground-truth catalog and evaluation docs: MIT, see
  [`evaluation/PLANTED_BUGS.md`](evaluation/PLANTED_BUGS.md).
