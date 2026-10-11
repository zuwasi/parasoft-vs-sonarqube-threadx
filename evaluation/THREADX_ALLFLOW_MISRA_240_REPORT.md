# ThreadX all-Flow and MISRA C:2025 rerun

**Run date:** 2026-10-11
**Purpose:** Determine whether enabling every installed Flow Analysis checker, then
adding the shipped MISRA C:2025 preset, detects more of the 50 valid planted defects.
**Scope:** The same ThreadX Win32 solution, compiler, source commits, TX concurrency
model, Thorough depth, and 240-second per-hotspot timeout in all four runs.

## Result

| Profile | Valid planted defects detected | Strict misses | Clean harness findings |
|---|---:|---:|---:|
| All 144 Flow checkers, Aggressive options, Thorough | **21/50 (42%)** | 29 | 26 |
| All 144 Flow checkers plus all 388 shipped MISRA C:2025 checkers | **21/50 (42%)** | 29 | 558 |

The all-Flow profile improves the earlier TX-aware 360-second result by one
strict TP: enabling all installed Flow rules newly flags **BUG-023** with
`BD-TRS-MLOCK` at `harness_concurrency.c:39`, the unlocked update of `g_stats`.
The finding is also emitted as MISRA C:2025 `DIR_5_1-a` in the combined run.
That is the same location and root cause, not a second detection.

Adding MISRA C:2025 did **not** increase the planted-defect score beyond the
all-Flow profile. Its standard checker emits many compliance findings on the
existing harness; those are not credited as defect detections merely because
they occur near a planted-code marker. The strict score counts only a diagnostic
that identifies the planted root cause.

Two catalog candidates remain invalid on Win32 and are excluded. The 29 strict
misses include 27 direct misses, one partial signal, and one indirect signal.

## Controlled run details

| Run | XML findings, all files | Harness findings | Files checked | Flow timeouts | Exit |
|---|---:|---:|---:|---:|---:|
| All Flow, clean | 324 | 26 | 214 | 187 | 0 |
| All Flow, bugged | 354 | 49 | 214 | 183 | 0 |
| Flow + MISRA C:2025, clean | 4,661 | 558 | 214 | 210 | 0 |
| Flow + MISRA C:2025, bugged | 4,745 | 615 | 214 | 206 | 0 |

Each configuration set `depthOfAnalysis=4` (Thorough),
`hotSpotTimeoutStrategy=time`, and `hotSpotTimeoutSeconds=240`. Both profiles
enabled all 144 installed Flow checker IDs, 25 aggressive/unvalidated-reporting
options, and two extra NOTINIT reporting options. The MISRA profile additionally
enabled all 388 rules in the installed `MISRA C 2025.properties` preset. Both
retained the same custom TX multithreading API model.

All four CLI runs exited 0 and checked the same 214 files. The reports still
record 183–210 hotspot timeouts per run. Exit 0 and the increased timeout limit
do not mean every path was analyzed.

The full shipped MISRA profile increased clean harness diagnostics from 26 to
558 (505 `StdViol` and 53 `FlowViol` entries); in the bugged state it increased
the harness total from 49 to 615 (469 `StdViol` and 146 `FlowViol`). These are
diagnostic records, not unique defects. Flow/MISRA aliases can report the same
source issue under multiple rule IDs. The clean-state MISRA findings are
preserved as raw normalized evidence, but are not all individually classified
as false positives: many are MISRA compliance findings in the pre-existing
codebase and require standards-oriented triage.

## Clean-state review

The 26 all-Flow clean harness findings are listed in
[`reports/threadx-allflow-misra-240/CLEAN_FLOW_REVIEW.csv`](reports/threadx-allflow-misra-240/CLEAN_FLOW_REVIEW.csv).
Compared with the prior 360-second clean review, the new configuration added six
diagnostic records:

- An unlocked `g_stats` update in the existing clean `telemetry_notify` callback.
  This is a separate unplanted clean-state concurrency concern; it is not counted
  as a false positive or conflated with BUG-023's different source location.
- One warning on `harness_cmd_queue` used through `tx_queue_receive`, which
  ThreadX synchronizes as part of its queue API.
- Three warnings on the file-static `log_ring`, accessed only by the logger thread.
- An additional `BD-API-STRSIZE` message for the known clean-baseline
  `sa_src[128]` over-read at line 161. `BD-PB-OVERFRD` already reports that same
  defect there, so it is duplicate confirmation rather than a new defect.

## Reproducibility and evidence

- [`DETECTION_MATRIX.csv`](reports/threadx-allflow-misra-240/DETECTION_MATRIX.csv):
  per-candidate results for both profiles.
- [`run-summary.csv`](reports/threadx-allflow-misra-240/run-summary.csv):
  commits, configuration hashes, timestamps, counts, timeouts, and raw-report hashes.
- [`CLEAN_FLOW_REVIEW.csv`](reports/threadx-allflow-misra-240/CLEAN_FLOW_REVIEW.csv):
  reviewed clean-state Flow findings.
- `*-findings.csv`: normalized full findings from each of the four runs.
- [`ALL_FLOW_MISRA_240_INVENTORY.csv`](configs/ALL_FLOW_MISRA_240_INVENTORY.csv):
  enabled Flow IDs and MISRA IDs mapped to their underlying checker where known.
- [`TX_All Flow Aggressive Thorough 240s.properties`](configs/TX_All%20Flow%20Aggressive%20Thorough%20240s.properties)
  and [`TX_All Flow plus MISRA C2025 Thorough 240s.properties`](configs/TX_All%20Flow%20plus%20MISRA%20C2025%20Thorough%20240s.properties):
  actual configurations used.
- [`export-threadx-allflow-misra-240.py`](scripts/export-threadx-allflow-misra-240.py)
  exports findings and asserts settings, source scope, and the adjudicated score.
- [`PARASOFT_RUN_GUIDE.md`](PARASOFT_RUN_GUIDE.md) documents worktree preparation,
  the four-run driver, Parasoft HTML/XML output, and CSV export.
- Raw XML and console logs remain locally at
  `C:\projects\parasoft-vs-SQ\reports\allflow-misra240-20261011T151333Z`.

The published presentation and PDF still describe the earlier experiment and
have not been changed by this rerun.
