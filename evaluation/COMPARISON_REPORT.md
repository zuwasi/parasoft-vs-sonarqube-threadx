# Parasoft C/C++test vs SonarQube CLI

- **Benchmark:** ThreadX Win32 flow analysis
- **Run date:** 2026-10-08
- **Source states:** `harness-clean` (`844260e1`) and `bugged` (`9d699595`)
- **PDF:** [`reports/COMPARISON_REPORT.pdf`](reports/COMPARISON_REPORT.pdf)
- **Detailed candidate outcomes:** [`reports/adjudication/DETECTION_MATRIX.csv`](reports/adjudication/DETECTION_MATRIX.csv)

## Executive summary

Parasoft C/C++test Standard 2026.1.3 completed four scans. Its `builtin://Flow
Analysis Aggressive` profile produced **15 strict true positives out of 50 valid
planted candidates**, plus one partial signal and one indirect signal. The
supplementary `builtin://Recommended Rules` profile produced **14 strict true
positives**, plus one indirect signal. The two profiles had **33 and 35 direct
misses**; counting partial and indirect signals as misses gives **35 and 36
strict false negatives**, respectively. Two of the original 52 candidates are
invalid on the Win32 target and are excluded from recall.

SonarQube CLI 1.9.0 authenticated and scanned the local secrets scope, reporting
zero secrets in each state. CLI system status explicitly reports Vortex as
**Not entitled** for this connection. Sonar's documentation says Vortex is part
of Sonar Agent Essentials, a separate subscription (Cloud annual Team or
Enterprise). The current organization has no Agent Essentials entitlement. Its
project-backed DEEP code-analysis requests were rejected with **403 Forbidden**
and all requested C files were skipped. Sonar therefore has **no code TP/FP/FN
result** in this experiment. A zero-secrets result is not evidence that the
planted code defects were absent.

The intended clean baseline is not fully clean. Parasoft found a real, unmarked
possible read past the 128-byte `sa_src` buffer in the clean `sa_store_len`
function. This contaminates the original false-positive baseline and is recorded
separately, not counted as a planted-bug detection.

## Results at a glance

| Tool/profile | Clean report XML | Bugged report XML | Valid candidates | Strict TP | Partial / indirect | Strict FN | Sonar code status |
|---|---:|---:|---:|---:|---:|---:|---|
| Parasoft Flow Analysis Aggressive | 90 total; 6 harness | 111 total; 27 harness | 50 | 15 | 1 partial, 1 indirect | 35 | N/A |
| Parasoft Recommended Rules | 20 total; 5 harness (+12 metrics) | 36 total; 21 harness (+12 metrics) | 50 | 14 | 1 indirect | 36 | N/A |
| SonarQube CLI DEEP | N/A | N/A | 50 | N/A | N/A | N/A | Vortex 403; 8/8 files skipped |

Parasoft totals include findings in upstream ThreadX and, for Recommended Rules,
12 ThreadX metrics violations per state. The profiles overlap and must not be
added together. Full XML finding exports are in
[`reports/parasoft/`](reports/parasoft/); raw report hashes are in
[`reports/parasoft/SHA256SUMS.txt`](reports/parasoft/SHA256SUMS.txt).

## Parasoft recall by defect family

| Family | Valid candidates | Aggressive TP | Aggressive partial / indirect | Aggressive direct FN | Recommended TP | Recommended indirect | Recommended direct FN |
|---|---:|---:|---:|---:|---:|---:|---:|
| Size arithmetic | 11 | 2 | 0 | 9 | 2 | 0 | 9 |
| Memory lifecycle | 10 | 3 | 1 partial | 6 | 2 | 0 | 8 |
| Concurrency | 8 | 0 | 0 | 8 | 0 | 0 | 8 |
| NULL / validation paths | 7 | 5 | 1 indirect | 1 | 5 | 1 indirect | 1 |
| Taint / bounds | 8 | 3 | 0 | 5 | 3 | 0 | 5 |
| Uninitialized memory | 6 | 2 | 0 | 4 | 2 | 0 | 4 |
| **Total** | **50** | **15** | **1 partial, 1 indirect** | **33** | **14** | **1 indirect** | **35** |

TP means the diagnostic describes the defect's root cause. A finding merely near
a marker is not credited. BUG-021's Aggressive `BD-RES-LEAKS` finding at the
second alias assignment is related to lost ownership, but it reports a lost
resource rather than the cataloged double-free effect; it is partial, not TP.
BUG-035's `BD-PB-CC` finding says the later null check is always false. It hints
that the guard follows a dereference but does not directly report a null
dereference, so it is indirect, not TP. The exact per-candidate notes are in the
matrix. The FN columns count both direct misses and partial/indirect findings,
so each profile's TP + FN equals the 50 valid candidates.

## Notable Parasoft detections

The Aggressive profile directly reported:

- **Bounds and size:** BUG-003 ring index (`BD-PB-ARRAY`), BUG-010 `raw[64]`,
  BUG-040 destination overflow, BUG-042 stale table bound, and BUG-046 handler
  table index.
- **Resource lifetime:** BUG-018 use-after-free, BUG-020 invalid interior free,
  and BUG-022 leaked retry buffers.
- **NULL and initialization:** BUG-032 unchecked channel lookup, BUG-033
  unchecked `malloc`, BUG-034 ignored output-parameter error contract, BUG-037
  uninitialized return value, BUG-038 deep null dereference, BUG-049 incomplete
  header initialization, and BUG-050 uninitialized slot pointer.

Neither profile reported a planted concurrency defect. The two Parasoft profiles
also differ: Recommended Rules did not report BUG-018, BUG-021's related leak, or
some other Aggressive-only findings.

## Clean-state findings and false-positive review

The Parasoft XML reports include findings in the harness and in the ThreadX
kernel. The 84 Flow Analysis Aggressive findings outside the harness were present
in both states; Recommended Rules had 15 such findings in each state. These
upstream findings are not counted as benchmark false positives.

| Profile | Clean harness findings | Adjudication |
|---|---:|---|
| Flow Analysis Aggressive | 6 | One genuine baseline issue; five non-defect scaffold findings |
| Recommended Rules | 5 | One genuine baseline issue; four non-defect or context-inappropriate findings |

The genuine baseline issue is `BD-PB-OVERFRD` at
`harness_sizearith.c:161`: `sa_store_len` can request a copy up to 65,535 bytes
from `sa_src[128]`. In the clean code the allocation is sized to `msg_len + 1`,
but the source buffer is still shorter than allowed `msg_len`. This issue
predates `harness-clean..bugged`, so it is not credited to a planted candidate.

The remaining Aggressive baseline diagnostics are three
`BD-PB-MEMOPT` findings on intentionally unused test copies, a `BD-PB-VOVR`
finding on an initial status value overwritten before use, and a
`BD-PB-CC` finding on a redundant condition after the input was clamped.
Recommended Rules additionally reports three `CODSTA-82` findings on perpetual
ThreadX worker loops. These are not cataloged security defects; the loop shape
is intentional for RTOS worker threads.
Row-level clean-state adjudication is in
[`reports/adjudication/CLEAN_FINDING_REVIEW.csv`](reports/adjudication/CLEAN_FINDING_REVIEW.csv).

## Benchmark ground-truth corrections

The source diff has 52 planted markers, but only **50 valid defects** on the
evaluated Win32 target:

1. **BUG-005 is invalid.** `payload_len` is `UINT16`; `payload_len + 12 + 4`
   cannot overflow 32-bit `ULONG`, and the resulting allocation covers the
   payload copy.
2. **BUG-031 is invalid.** `lookup_channel` and `accessor_channel` use the same
   range guard. A non-NULL accessor result implies that the primary result is
   also non-NULL.
3. **The clean baseline has an unmarked over-read.** See the finding above.

The 20-second soak establishes only that the process remained alive during that
run. It does not establish that every defect is runtime-benign or that undefined
behavior did not occur. No source fixes were made after scanning; the scanned tags
and raw reports remain unchanged.

## SonarQube CLI result and limitation

The public repository was imported into the authenticated `zuwasi` SonarQube
Cloud organization, project key `zuwasi_parasoft-vs-sonarqube-threadx`, after
user approval. CLI 1.9.0 then attempted both an eight-file DEEP analysis and a
bugged-state change-set analysis against `harness-clean`.

Both returned:

> Vortex analysis is not available on this connection (403 Forbidden).

The CLI JSON marks all 8 all-file inputs and all 6 changed harness C files as
skipped. This is a service/connection capability failure, not a clean Sonar
analysis. `sonar system status` also reports `VORTEX Status: Not entitled`.
The [Sonar Vortex documentation](https://docs.sonarsource.com/sonarqube-cloud/administering-sonarcloud/ai-features/sonar-vortex)
states that Vortex is part of Sonar Agent Essentials, a separate subscription
for SonarQube Cloud annual Team or Enterprise plans. The current organization
does not have that entitlement; changing CLI flags cannot grant it. The locally
executable secrets scanner did complete and reported **0 secrets** in each
state. It is a different analysis and does not score the 50 C defect candidates.

Normalized Sonar results and the exact 403 response are in
[`reports/sonarqube/`](reports/sonarqube/). The initial clean-state secrets scan
of the entire worktree timed out at 30 seconds; the scoped eight-file rerun
completed successfully.

## Reproduction and audit trail

- Machine, tool versions, profiles, commands, UTC run times, exit codes and
  Sonar outcomes: [`SCAN_PROTOCOL.md`](SCAN_PROTOCOL.md).
- Ground truth, paths, and per-defect validity notes:
  [`PLANTED_BUGS.md`](PLANTED_BUGS.md).
- Machine-readable detection matrix:
  [`reports/adjudication/DETECTION_MATRIX.csv`](reports/adjudication/DETECTION_MATRIX.csv).
- Parasoft normalized findings and aggregate counts:
  [`reports/parasoft/`](reports/parasoft/).
- Sonar CLI outputs and local secrets logs:
  [`reports/sonarqube/`](reports/sonarqube/).

The score describes this ThreadX Win32 harness, these versions, profiles, and
connection permissions. It is not a general ranking of either product.
