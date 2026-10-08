# ThreadX Static-Analysis Shootout — Parasoft C/C++test vs SonarQube CLI

This repository is an **independent, reproducible static-analysis tool comparison**,
built on the [Eclipse ThreadX RTOS](https://github.com/eclipse-threadx/threadx)
Win32 sample application (`ports/win32/vs_2019/example_build`).

It answers one engineering question: *on identical, RTOS-realistic C code with a
known ground truth, what do **Parasoft C/C++test (Standard, CLI) Flow Analysis at
maximum depth** and the **SonarQube CLI at its highest analysis level** actually
find, and what do they falsely report?*

> **DO NOT use this fork as production ThreadX code.**
> It contains **52 deliberate, documented defects** injected for the evaluation.
> For production use, go to the pristine upstream: [eclipse-threadx/threadx](https://github.com/eclipse-threadx/threadx).

## Method in one minute

1. **Pristine upstream** is pinned at tag `baseline-clean` (commit `93387b0a`, MIT).
2. A **clean sensor-telemetry harness** (`ports/win32/vs_2019/example_build/sample_threadx/harness/`)
   is added: 4 worker threads, byte/block pools, three queues, four mutexes, event
   flags — realistic ThreadX usage with **zero defects** (tag `harness-clean`).
3. **52 flow-analysis-grade defects** are injected as small diffs across six family
   modules (tag `bugged`).  Every defect site carries an in-source banner:
   `/* [PLANTED DEFECT BUG-nnn | CWE-x | ...] */`.
4. Both tools scan **both states** with the same machine, toolchain and settings:
   - findings on `harness-clean` measure **false positives** (plus upstream-code findings),
   - findings on `bugged` are mapped to the catalog to score **true positives / false negatives**.
5. Every defect is **runtime-benign** under documented input ranges, so the demo app
   still runs (validated by soak tests) — defects are reachable statically, not
   crashing dynamically.  Full per-defect rationale: [`evaluation/PLANTED_BUGS.md`](evaluation/PLANTED_BUGS.md).

## Key artifacts

| Path | Content |
|------|---------|
| `evaluation/PLANTED_BUGS.md` | Ground truth: all 52 defects, CWEs, exact `file:line` sites, trigger paths, difficulty rationale, runtime-safety arguments, adjudication rules |
| `evaluation/SCAN_PROTOCOL.md` | Exact tool versions, commands, settings and raw report locations (audit trail) |
| `evaluation/scripts/` | Scan runner scripts used to produce the reports |
| `evaluation/reports/` | Raw scanner outputs per tool and per state |

## Git states (the audit trail)

| Tag | Commit | Meaning |
|-----|--------|---------|
| `baseline-clean` | `93387b0a` | Pristine upstream ThreadX (v6.5.x line) |
| — | `bb0c450c` | Win32 example_build adapted to the VS2026 toolchain (v145) + build-order fix |
| `harness-clean` | `844260e1` | Clean evaluation harness, **false-positive baseline** |
| `bugged` | `9d699595` | 52 deliberate defects injected, **detection target** |
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

The comparison report (with per-defect detection matrix and false-positive
assessment for both tools) is published in `evaluation/` once all scans complete.
See `evaluation/SCAN_PROTOCOL.md` for the status of each scan.

## Credits

- Upstream project: **Eclipse ThreadX RTOS** by Microsoft Corporation and the
  Eclipse ThreadX contributors — MIT license, preserved in [`LICENSE.txt`](LICENSE.txt).
- Upstream README content and documentation live in the upstream repository;
  this fork's root README replaces them for evaluation purposes only.
- Defect harness, ground-truth catalog and evaluation docs: MIT, see
  [`evaluation/PLANTED_BUGS.md`](evaluation/PLANTED_BUGS.md).
