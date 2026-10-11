# ThreadX Thorough rerun with 360-second hotspot timeout

**2026-10-11 result: 20/50 strict true positives (40%), unchanged.**
Increasing the hotspot timeout from 60 to 360 seconds produced no additional
planted-defect detections. All 20 previously credited detections remain present;
the same 30 candidates remain strict false negatives (28 direct misses, one
partial and one indirect result).

## Controlled change

The new configuration is
`configs/TX_Flow Analysis Aggressive Thorough 360s.properties`.
It copies the previously tested TX Thorough profile and explicitly sets:

```properties
com.parasoft.xtest.checker.flowanalysis.depthOfAnalysis=4
com.parasoft.xtest.checker.flowanalysis.hotSpotTimeoutStrategy=time
com.parasoft.xtest.checker.flowanalysis.hotSpotTimeoutSeconds=360
```

Rules, TX conventions, compiler `vc_14_5`, solution scope `Release|Win32`,
source tags, and checked-file scope remain unchanged. The only behavioral setting
changed is the per-hotspot time allowance. This is not a total scan timeout.
Both runs use fresh workspaces and retain separate reports.

## Measured results

| State | Hotspot limit | XML findings | Harness findings | Strict TP | Strict FN | Reported timeouts |
|---|---:|---:|---:|---:|---:|---:|
| Clean | 60 seconds | 297 | 18 | N/A | N/A | 240 |
| Clean | **360 seconds** | **317** | **20** | N/A | N/A | **183** |
| Bugged | 60 seconds | 324 | 40 | 20 | 30 | 240 |
| Bugged | **360 seconds** | **344** | **49** | **20** | **30** | **182** |

Both new runs exited **0**, checked **214 files**, and their console logs confirm
`Current timeout strategy is "time" (360 seconds)`.
The clean run took **22 minutes 33 seconds**, and the bugged run took
**22 minutes 23 seconds**, versus about six minutes each with the 60-second limit.

Timeout counts fell by 57 and 58 respectively, about 24%, but did not disappear.
The results remain timeout-limited; they do not prove exhaustive exploration.
Aggregate timeout counts do not identify which missed candidate, if any, was
affected by an individual timeout.

## Additional diagnostics, not additional planted detections

No previous harness finding disappeared.

The clean run adds:

- `BD-TRS-THRDR`, `harness_init.c:238`, another unlocked access to the already
  disclosed simulated input `g_sensor_raw`.
- `BD-TRS-THRDR`, `harness_sizearith.c:66`, unlocked access to `log_ring`.
  In the scanned harness only `logger_entry` calls `sa_log_write` and
  `sa_log_flush`, and the ring is file-static. This is single-thread-context
  noise, not a planted race detection.

The bugged run adds nine diagnostics:

- The same `g_sensor_raw` access at `harness_init.c:238`.
- Two `log_ring` alerts at `harness_sizearith.c:68` and `:69`. These do not
  improve detection of BUG-003, whose bounds defect was already reported.
- Five `BD-PB-CC` constant-condition reports in `harness_memory.c` at
  lines 40, 66, 71, 80 and 82. None describes a newly detected cataloged root cause.
- One `BD-TRS-REVLOCK` at `harness_concurrency.c:97`: the detailed path assumes
  a failed mutex acquisition followed by an unconditional put. This does not
  create another detection of BUG-025 or BUG-028.

The per-candidate matrix retains **20 TP**, including **4/8 concurrency TP**.
The unresolved concurrency candidates remain BUG-023, BUG-024, BUG-027 and BUG-030.

## Clean-state review

Of the 20 clean-harness diagnostics:

- **13 contextual/model false positives**: nine code-shape reports, three
  synchronized ThreadX control-object alerts, and one single-thread ring alert.
- **4 assumption-dependent unlock warnings**, not confirmed unconditional FPs.
- **1 confirmed pre-existing source-buffer over-read**.
- **2 diagnostics on the same unmarked shared-input concurrency concern**,
  not two additional planted defects.

The 60-second run had 12 contextual/model FPs. The extra allowance therefore
added one such FP and another diagnostic on an existing baseline concern,
without improving strict planted-defect recall.

## Evidence

- [`reports/threadx-thorough-360/run-summary.csv`](reports/threadx-thorough-360/run-summary.csv):
  exact UTC times, unchanged commits, exits, configuration hash, raw-report/log
  hashes, checked files, and timeout counts.
- [`reports/threadx-thorough-360/DETECTION_MATRIX.csv`](reports/threadx-thorough-360/DETECTION_MATRIX.csv):
  all 52 candidate rows, with the same 50-valid denominator and side-by-side
  60/360-second outcomes.
- [`reports/threadx-thorough-360/FINDING_DELTA.csv`](reports/threadx-thorough-360/FINDING_DELTA.csv):
  all 11 added harness diagnostics and their review.
- `reports/threadx-thorough-360/tx-{clean,bugged}-findings.csv`: complete normalized findings.
- [`reports/threadx-thorough-360/CLEAN_FINDING_REVIEW.csv`](reports/threadx-thorough-360/CLEAN_FINDING_REVIEW.csv):
  all 20 clean-harness classifications.
- Raw local evidence:
  `C:\projects\parasoft-vs-SQ\reports\tx-thorough-360-20261011T133054Z`.

The exporter `scripts/export-threadx-360.py` checks actual timeout log text,
configuration hashes, checked-file counts, preservation of all previous harness
findings, review of every added harness finding, and the unchanged 20/50 matrix.

Prior results are retained in [`THREADX_THOROUGH_REPORT.md`](THREADX_THOROUGH_REPORT.md).
No presentation or public page was changed or republished for this rescan.
