# ThreadX conventions: controlled Thorough rerun

> A later rerun increased the TX hotspot limit to 360 seconds. The score remained
> **20/50**, with fewer timeouts. See [`THREADX_360_REPORT.md`](THREADX_360_REPORT.md).

**Run date:** 2026-10-11. **Result:** explicit Thorough analysis with the TX model
reported **20 strict true positives out of 50 valid planted defects (40%)**,
versus **16/50 (32%)** for the same-depth control without TX conventions and
**15/50 (30%)** for the original 2026-10-08 Aggressive scan.

The model adds **four concurrency detections** at the same analysis depth.
The Thorough control adds **one uninitialized-memory detection** relative to the
original run. No previously credited strict true positive was lost.

These are distinct scans, not combined finding sets. All 52 candidate rows
(including the two invalid candidates) are in
[`reports/threadx-thorough/DETECTION_MATRIX.csv`](reports/threadx-thorough/DETECTION_MATRIX.csv).

## Settings and scope

- Parasoft C/C++test Standard **2026.1.3**, compiler **`vc_14_5`**.
- Original source tags, unchanged: clean `844260e1497fecffeb05c41cdf94e57044afee6e`,
  bugged `9d6995958d8e7618e570e4724083e5a3edfe934b`.
- Same `azure_rtos.sln@Release|Win32` scope, **214 files checked** in each run.
- Control configuration: `configs/Flow Analysis Aggressive Thorough.properties`.
- TX configuration: `configs/TX_Flow Analysis Aggressive Thorough.properties`.
- Both explicitly set
  `com.parasoft.xtest.checker.flowanalysis.depthOfAnalysis=4`, which the installed
  configuration schema names **Thorough (slowest)**.
- Both retain the same Aggressive rule settings. Their semantic difference is the
  custom multithreading API, described in
  [`configs/THREADX_CONVENTIONS.md`](configs/THREADX_CONVENTIONS.md).
- Each run used a new workspace and report directory. No source fixes, resource
  models, or queue/taint models were added.

**Depth correction:** the original Aggressive preset and the first TX smoke-test
profile did not explicitly set the analysis depth field. The installed UI schema
defaults that field to Standard (`2`). The old scan must not be described as a
verified Thorough run merely because its preset was named Aggressive. The
follow-up configurations explicitly set Thorough.

## Measured counts

| Run | Total XML findings | Harness findings | Strict TP | Strict FN | Reported analysis timeouts |
|---|---:|---:|---:|---:|---:|
| Thorough control, clean | 121 | 7 | N/A | N/A | 239 |
| Thorough control, bugged | 143 | 29 | **16** | **34** | 238 |
| Thorough + TX, clean | 297 | 18 | N/A | N/A | 240 |
| Thorough + TX, bugged | 324 | 40 | **20** | **30** | 240 |

All four runs exited **0**. A finding count is not a detected-defect count:
multiple messages can refer to one candidate, and upstream/scaffold findings
are not benchmark detections.

The two related signals remain separate: BUG-021 is partial (a lost resource
instead of the cataloged double free), and BUG-035 is indirect (a late null
check, not a direct dereference report). Both remain strict false negatives.
The TX run therefore has **28 direct misses + these two signals = 30 strict FN**.

## Newly detected candidates

| Candidate | New evidence | Attribution |
|---|---|---|
| BUG-048 | `BD-PB-NOTINIT`, `harness_uninit.c:69`: uninitialized `h.flags` in `frame_send_local` | Present in both Thorough runs; depth-control improvement |
| BUG-025 | `BD-TRS-ORDER`, `harness_concurrency.c:75`: `net_mutex_1` / `net_mutex_2` acquired in opposite orders | TX conventions |
| BUG-026 | `BD-TRS-THRDR`, `harness_concurrency.c:111`: `g_ready` accessed without a lock | TX conventions |
| BUG-028 | `BD-TRS-REVLOCK`, `harness_concurrency.c:155`: release of `net_mutex_1` without acquisition | TX conventions |
| BUG-029 | `BD-TRS-THRDR`, `harness_concurrency.c:166`: unlocked initialization-guard access to `g_inited` | TX conventions |

The remaining concurrency candidates **BUG-023, BUG-024, BUG-027, BUG-030** have
no matching root-cause report. The unreleased `table_mutex` alert at
`harness_init.c:225` is **not** credited to BUG-024: its detailed path assumes a
failed mutex acquire followed by an early return and later thread-loop exit.
It does not diagnose publishing/using the slot outside its critical section.
This is a model/path inconsistency to review, not an extra planted detection.

## Strict recall by family

| Family | Valid | Original TP | Thorough control TP | Thorough + TX TP | TX strict FN |
|---|---:|---:|---:|---:|---:|
| Size arithmetic | 11 | 2 | 2 | 2 | 9 |
| Memory lifecycle | 10 | 3 | 3 | 3 | 7 |
| Concurrency | 8 | 0 | 0 | **4** | 4 |
| NULL / validation | 7 | 5 | 5 | 5 | 2 |
| Taint / bounds | 8 | 3 | 3 | 3 | 5 |
| Uninitialized memory | 6 | 2 | **3** | **3** | 3 |
| **Total** | **50** | **15** | **16** | **20** | **30** |

## Clean-state noise and baseline issues

Do not call all 18 TX clean-harness findings false positives:

| Category | Thorough control | Thorough + TX | Interpretation |
|---|---:|---:|---|
| Confirmed pre-existing source-buffer over-read | 1 | 1 | Real baseline issue, not a planted TP |
| Contextual code-shape findings | 6 | 9 | Scratch copies, overwritten value, constant-condition reports; non-defect benchmark noise |
| RTOS control-object lock alerts | 0 | 3 | False-positive context: byte pool, block pool and event group passed to their synchronized ThreadX APIs |
| Unlock-after-assumed-failed-acquire warnings | 0 | 4 | Assumption-dependent; not established as unconditional false positives |
| Unmarked shared-input concurrency concern | 0 | 1 | `g_sensor_raw` shared by producer/logger/maintenance; not credited to a planted bug and not dismissed as an FP |
| **Total** | **7** | **18** | |

Thus the TX clean run has **12 contextual/RTOS-model false positives**, **four
assumption-dependent warnings**, **one confirmed baseline defect**, and **one
additional baseline concurrency concern**.

The four clean `BD-TRS-REVLOCK` paths enter `_txe_mutex_get` and assume
`TX_MUTEX_ERROR` or `TX_CALLER_ERROR`; the application then ignores the failed
acquisition and performs an unconditional put. Under successful initialization
and valid thread context these paths are inappropriate, but the scan does not
prove those preconditions. Do not hide this distinction in the FP score.

## Analysis timeout limitation

The CLI reported **238–240 Flow Analysis timeouts per run**, with the default
**60-second per-hotspot timeout strategy**. Exit 0 means reports were generated,
not that every suspicious path was explored to completion.

These scores are the measured result of Thorough **with that timeout limit**.
The logs do not assign the aggregate timeout counts to individual missed bugs,
so no specific miss can be explained conclusively as a timeout. Raising or
disabling the timeout would be a separate experiment.

The TX model improves thread/lock reasoning, but it does not model byte/block
pool lifetime, queue ownership transfer, taint sources, recursion counts, or
all ThreadX scheduling semantics. Those remaining gaps must not be presented
as proven causes of every miss.

## Audit trail and reproduction

- [`reports/threadx-thorough/run-summary.csv`](reports/threadx-thorough/run-summary.csv):
  UTC timestamps, commits, configuration hashes, report hashes, checked-file
  counts, exits, and timeout counts.
- `reports/threadx-thorough/{control,tx}-{clean,bugged}-findings.csv`: all normalized findings.
- [`reports/threadx-thorough/CLEAN_FINDING_REVIEW.csv`](reports/threadx-thorough/CLEAN_FINDING_REVIEW.csv):
  each clean harness diagnostic and its classification.
- Raw reports/logs are preserved locally outside the repository:
  `C:\projects\parasoft-vs-SQ\reports\thorough-20261011T123829Z`.
- `scripts/export-threadx-thorough.py` exports the new reports, checks configuration
  hashes and checked-file counts, and asserts the independently reviewed
  per-candidate diagnostic locations and 16/50 versus 20/50 totals.

Run the existing scanner runner with either configuration file and a **new**
`-ReportsRoot`. Use the same immutable worktrees and compiler `vc_14_5`. Do not
overwrite original reports. The earlier partial `tx-aware-20261011T123457Z`
attempt, made before Thorough was explicit, is preserved locally but excluded
from this comparison.

The original report, presentation, and published pages remain historical
artifacts; they have not been regenerated or published as part of this rerun.
