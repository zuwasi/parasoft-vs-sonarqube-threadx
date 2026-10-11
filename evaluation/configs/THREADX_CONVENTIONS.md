# ThreadX Flow Analysis conventions

`TX_Flow Analysis Aggressive.properties` is a separate copy of the installed
2026.1.3 Flow Analysis Aggressive configuration with a custom multithreading API
named **TX**. It does not change the original scanner settings, source tags, or
the published 15/50 result. A new measured result requires a new scan and review.

## Import into C/C++test Professional

Import the `.properties` file in the Test Configuration manager. Select
**TX_Flow Analysis Aggressive > Static > Flow Analysis Advanced Settings >
Multithreading > TX > Edit**.

The existing Eclipse `TX_Flow Analysis Standard` configuration is left unchanged.
Importing a separate profile avoids modifying an open configuration behind the
IDE's back.

## Entries in the TX dialog

All entries are enabled. The namespace is empty because these are global C
functions. Subclass definitions, `this` is a mutex, and returns a mutex are
unchecked. Parameter indices are **1-based**.

| Section | Function names | Parameter | Error-return constraint |
|---|---|---:|---|
| Locking | `tx_mutex_get`, `_tx_mutex_get`, `_txe_mutex_get`, `_txr_mutex_get` | Mutex: **1** | **`!=0`** |
| Unlocking | `tx_mutex_put`, `_tx_mutex_put`, `_txe_mutex_put`, `_txr_mutex_put` | Mutex: **1** | Not available |
| Lock destruction | `tx_mutex_delete`, `_tx_mutex_delete`, `_txe_mutex_delete`, `_txr_mutex_delete` | Mutex: **1** | Not available |
| Sleep | `tx_thread_sleep`, `_tx_thread_sleep` | None | Not available |
| Thread creation | `tx_thread_create`, `_tx_thread_create`, `_txe_thread_create`, `_txr_thread_create` | Entry function: **3** | Not available |
| Condition-variable blocking | **Empty** | None | None |

`TX_SUCCESS` equals zero. The field describes **failure**, not success: enter
`!=0`, not `==0` and not descriptive text. A failed `TX_NO_WAIT` mutex acquisition
must not make the analyzer assume the mutex was acquired.

ThreadX public API names are macros. Depending on error-checking settings they
expand to `_tx_*`, `_txe_*`, or `_txr_*`; model the expanded names as well as the
public names. `_txe_thread_create` and `_txr_thread_create` add a final control
block size argument, but the entry function remains argument 3.

## Do not misrepresent other ThreadX services

- Counting semaphores are not owner-held mutexes. A semaphore put can signal a
  different thread and does not mean that thread released a lock it owned.
- Queue, event-flag, semaphore, and pool-allocation waits do not atomically release
  and reacquire an application `TX_MUTEX`. Do not put them in the
  condition-variable table with their queue/pool/event pointer as a "mutex".
- Those APIs can block when the wait option permits it, but this dialog has no
  argument-sensitive wait-option field. Listing them as unconditional sleep
  functions would also label `TX_NO_WAIT` calls as blocking.
- `tx_thread_relinquish` yields the processor; it is not a timed sleep. It is not
  included as an unconditional sleep function.
- The model does not represent recursive mutex counts, priority inheritance,
  preemption thresholds, conditional auto-start, queue ownership transfer, or
  byte/block pool lifetime. Resource conventions require the separate Resources
  tab. This change alone cannot be claimed to fix all 35 strict misses.

## CLI use

Pass the file path as the configuration argument to the existing scan runner:

```powershell
.\evaluation\scripts\run-parasoft.ps1 `
  -Worktree C:\projects\parasoft-vs-SQ\states\bugged `
  -State bugged `
  -Config "C:\projects\parasoft-vs-SQ\threadx\evaluation\configs\TX_Flow Analysis Aggressive.properties" `
  -Compiler vc_14_5
```

Use a new report/workspace directory for any rerun. Do not replace the original
benchmark reports or add findings from different runs together.

The isolated smoke-test fixture is
`evaluation/fixtures/threadx-conventions.c`. It is not part of the 50-defect
benchmark. It contains an unprotected shared counter, a mutex-protected counter,
and a sleep inside a critical section so that the model can be checked independently.

## Smoke-test validation, 2026-10-11

C/C++test Standard 2026.1.3, compiler `vc_14_5`, checked the same single fixture
with the repository's real ThreadX API and Win32 port headers in separate
workspaces. Both runs exited **0**, checked one file, and generated XML reports.

| Configuration | Findings |
|---|---|
| Unmodified Flow Analysis Aggressive | **0** |
| TX_Flow Analysis Aggressive | **3** |

The configured run reported:

- `BD-TRS-THRDR` at fixture line **18**: `unprotected_counter` used without a lock.
- `BD-TRS-THRDR` at fixture line **30**: the same unprotected shared variable in
  the other thread.
- `BD-TRS-TSHL` at fixture line **22**: `_tx_thread_sleep` inside a critical section.

Neither mutex-protected counter access was reported as an unlocked shared access.
This is evidence that the configured `_txe_thread_create`, `_txe_mutex_get`,
`_txe_mutex_put`, and `_tx_thread_sleep` conventions work together on this fixture,
not proof that every ThreadX variant or semantic edge case is modeled correctly.
The other aliases are included from their documented signatures.

Raw local evidence is retained outside the repository in
`C:\projects\parasoft-vs-SQ\reports\threadx-conventions-smoke-20261011\`.
At the smoke-test stage the benchmark had not been rerun. A subsequent controlled
rerun explicitly set Thorough depth (`4`) in two new profiles:
`Flow Analysis Aggressive Thorough.properties` and
`TX_Flow Analysis Aggressive Thorough.properties`. It measured **16/50 without
TX** and **20/50 with TX**, subject to reported analysis timeouts. See
[`../THREADX_THOROUGH_REPORT.md`](../THREADX_THOROUGH_REPORT.md).
The historical published 15/50 remains the original unconfigured-run result.

## References

- [Parasoft C/C++test Professional 2026.1: Flow Analysis, Multithreading Tab Options](https://docs.parasoft.com/display/CPPTESTPROEC20261/Flow+Analysis)
- ThreadX `common/inc/tx_api.h`: `TX_SUCCESS`, API macro mappings, mutex prototypes,
  and thread-creation prototypes.
