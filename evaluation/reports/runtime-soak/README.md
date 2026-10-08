# Runtime soak evidence

- harness-clean-12s.excerpt.log: the clean-harness build (`harness-clean`) run for 12s
  under Win32; process alive at kill, demo counters incrementing.
- bugged-20s.excerpt.log: the bugged build (`bugged`) with all 52 defects present, run
  for 20s; process alive at kill.  Confirms the runtime-benign design of every defect
  (see evaluation/PLANTED_BUGS.md per-defect runtime-safety arguments).
- SHA256SUMS.txt: integrity hashes of the full untruncated logs (kept in the evaluation
  workspace outside the repository; identical copies reproducible from any checkout by
  rebuilding the tagged states and running sample_threadx.exe).

Command: sample_threadx\Release\sample_threadx.exe (Release|Win32, MSVC v145).
