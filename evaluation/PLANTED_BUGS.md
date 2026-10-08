# Deliberate Defect Catalog — ThreadX Static-Analysis Tool Comparison

**Document purpose:** Engineering ground truth for the Parasoft C/C++test (Standard, CLI) vs
SonarQube CLI static-analysis comparison performed on the ThreadX Win32 sample application.

**Ground truth definition:** the complete `git diff harness-clean..bugged` between the two
signed git tags in this repository.  Every defect below carries an in-source banner
`[PLANTED DEFECT BUG-nnn | CWE-x | ...]` at the exact defect line, so tool findings can be
mapped to this catalog mechanically.

| Git state | Tag | Commit |
|---|---|---|
| Pristine upstream ThreadX | `baseline-clean` | `93387b0a` |
| + Win32 example_build adapted to the VS2026 toolchain | (part of history) | `bb0c450c` |
| + clean harness (false-positive baseline, **no defects**) | `harness-clean` | `844260e1` |
| + 52 deliberate defects (**detection target**) | `bugged` | `9d699595` |

---

## 1. Design principles of the defect set

1. **Flow-analysis grade.**  Every defect requires interprocedural, path-sensitive or
   dataflow reasoning to detect.  None are findable by grep-style lexical rules alone.
2. **Runtime-benign, statically reachable.**  Trigger data originates from ThreadX queue
   receives (`tx_queue_receive` into caller memory) and volatile simulated hardware
   registers (`g_sensor_*`).  At runtime the producer thread only ever writes documented,
   in-range values (see `harness_init.c`), so the demo application keeps running with all
   52 defects present (verified: 20s soak).  Statically, the analyzers cannot correlate
   queue payloads across threads and must treat every volatile read as a fresh unknown
   value, so every defect remains reachable.
3. **No constant-propagation give-aways.**  Bounds checks that are present use live
   values; defect branches depend on error paths (queue full, allocation failure, abort
   request) that genuinely cannot fire under the harness's runtime conditions.
4. **ThreadX realism.**  Defects live inside a sensor-telemetry pipeline that uses the
   same ThreadX primitives as real firmware: byte/block pools, three queues, four
   mutexes, event flags, and four worker threads.  A subset of defects additionally
   requires the analyzer to model ThreadX API semantics (marked **[TX]** below); these
   probe the depth of each tool's RTOS knowledge.
5. **Size arithmetic emphasized** (12 defects, user requirement): truncation, wrap,
   signed/unsigned, `sizeof(pointer)` misuse, masked-length checks, off-by-one.

## 2. Defect summary table

| ID | CWE | Family | Site (file:line) | Function | Short description |
|----|-----|--------|------------------|----------|-------------------|
| BUG-001 | CWE-190/787 | Size arithmetic | harness_sizearith.c:26 | sa_alloc_copy | `count*4` truncated to UINT16 for malloc, full length for memcpy |
| BUG-002 | CWE-197 | Size arithmetic | harness_sizearith.c:46 | sa_copy_checked | magnitude check admits negative lengths → huge size_t |
| BUG-003 | CWE-193 | Size arithmetic | harness_sizearith.c:63 | sa_log_write | `idx <= LOG_DEPTH` off-by-one writes one past ring |
| BUG-004 | CWE-131 | Size arithmetic | harness_sizearith.c:111 | sa_frame_new | memcpy of `sizeof(pointer)` bytes → partially initialized frame |
| BUG-005 | CWE-190 | Size arithmetic | harness_sizearith.c:133 | sa_pack | unguarded `payload+12+4` wraps to undersized allocation |
| BUG-006 | CWE-197 | Size arithmetic | harness_sizearith.c:154 | sa_store_len | stored length truncated for allocation, full length for copy |
| BUG-007 | CWE-787 | Size arithmetic | harness_sizearith.c:176 | sa_table_put | missing column bounds check; negative columns walk below table |
| BUG-008 | CWE-131 | Size arithmetic | harness_sizearith.c:196 | sa_cfg_snapshot | `sizeof(cfg)` (pointer) copied instead of `sizeof(struct)` |
| BUG-009 | CWE-190/690 | Size arithmetic | harness_sizearith.c:231 | sa_pool_resize | wrapped pool arithmetic; allocation status ignored → NULL deref |
| BUG-010 | CWE-170 | Size arithmetic | harness_sizearith.c:252 | sa_scan_raw | full-length fill leaves no NUL; strlen reads out of bounds |
| BUG-011 | CWE-704 | Size arithmetic | harness_sizearith.c:276 | sa_masked_copy | length checked after 16-bit mask, copy uses full 32-bit length |
| BUG-012 | CWE-190/787 | Size arithmetic | harness_sizearith.c:295 | sa_hdr_store | configured buffer size never validated against header size |
| BUG-013 | CWE-416 | Memory | harness_memory.c:45 | pipeline_submit | frame flags written after tx_byte_release |
| BUG-014 | CWE-415 | Memory | harness_concurrency.c:203 | queue_drain_safe | caller ignores callee's release-on-reject contract → double release |
| BUG-015 | CWE-401 | Memory | harness_memory.c:85 | frame_chain_check | abort path returns without releasing allocated chain |
| BUG-016 | CWE-762 [TX] | Memory | harness_memory.c:125 | block_use | block-pool block released to the byte pool |
| BUG-017 | CWE-415 | Memory | harness_memory.c:148 | normalize_frame | callee releases frame but reports success |
| BUG-018 | CWE-416 | Memory | harness_memory.c:175 | batch_send | loop keeps using freed batch after free() |
| BUG-019 | CWE-404 [TX] | Memory | harness_memory.c:206 | pipeline_stop | early return leaks created TX_TIMER |
| BUG-020 | CWE-761 | Memory | harness_memory.c:233 | interior_test | interior pointer freed instead of allocation base |
| BUG-021 | CWE-415 | Memory | harness_memory.c:257 | cache_store | both cache slots alias one frame → flush double-frees |
| BUG-022 | CWE-401 | Memory | harness_memory.c:311 | retry_send | retry loop orphans the failed attempt's buffer |
| BUG-023 | CWE-362 | Concurrency | harness_concurrency.c:37 | stats_record_fast | unlocked read-modify-write of shared stats |
| BUG-024 | CWE-366 | Concurrency | harness_concurrency.c:56 | slot_use | slot published/consumed outside critical section (TOCTOU) |
| BUG-025 | CWE-833 | Concurrency | harness_concurrency.c:87 | net_update_b | lock-order inversion vs net_update_a |
| BUG-026 | CWE-364 | Concurrency | harness_concurrency.c:108 | wait_ready | unsynchronized spin on shared flag |
| BUG-027 | CWE-362 | Concurrency | harness_concurrency.c:134 | job_append | unlocked list append while readers walk the list |
| BUG-028 | CWE-667 [TX] | Concurrency | harness_concurrency.c:153 | mutex_teardown | non-owner mutex put, failed status ignored |
| BUG-029 | CWE-366 | Concurrency | harness_concurrency.c:164 | lazy_init | init guard read outside the lock (racy double-checked init) |
| BUG-030 | CWE-366 | Concurrency | harness_concurrency.c:190 | queue_drain_safe | enqueue-count check instead of receive status; f read unchecked |
| BUG-031 | CWE-476 | NULL paths | harness_nullpaths.c:74 | channel_update | mirror pointer checked, primary pointer dereferenced |
| BUG-032 | CWE-476 | NULL paths | harness_nullpaths.c:94 | sensor_dispatch | lookup_channel result used without NULL check |
| BUG-033 | CWE-690 | NULL paths | harness_nullpaths.c:113 | slot_allocate | malloc result dereferenced without NULL check |
| BUG-034 | CWE-457 | NULL paths | harness_nullpaths.c:151 | frame_consume | get_frame status ignored; out-param never assigned on error |
| BUG-035 | CWE-476 | NULL paths | harness_nullpaths.c:166 | cfg_rate | configuration read before the NULL check |
| BUG-036 | CWE-690 | NULL paths | harness_nullpaths.c:184 | notify_fire | optional callback invoked without NULL check |
| BUG-037 | CWE-476 | NULL paths | harness_nullpaths.c:200 | job_tail_value | guard protects the loop but n is never assigned when list is empty |
| BUG-038 | CWE-476 | NULL paths | harness_nullpaths.c:224 | probe_level2 | unchecked accessor across four-hop call chain |
| BUG-039 | CWE-129 | Taint/bounds | harness_taint.c:56 | cal_update | received sensor id indexes table without bounds check |
| BUG-040 | CWE-787 | Taint/bounds | harness_taint.c:71 | msg_data_copy | copy length validated against queue capacity, not destination |
| BUG-041 | CWE-120 | Taint/bounds | harness_taint.c:86 | name_copy | unbounded strcpy into 16-byte label |
| BUG-042 | CWE-129 | Taint/bounds | harness_taint.c:101 | slot_store | bound checked against stale constant (8) for a 4-entry table |
| BUG-043 | CWE-787 | Taint/bounds | harness_taint.c:116 | sums_update | received count not clamped to accumulator array size |
| BUG-044 | CWE-134 | Taint/bounds | harness_taint.c:133 | log_text | received text used directly as printf format |
| BUG-045 | CWE-680/787 | Taint/bounds | harness_taint.c:151 | dup_string | legacy branch sizes allocation without terminator |
| BUG-046 | CWE-129 | Taint/bounds | harness_taint.c:179 | nibble_dispatch | type nibble indexes handler table unchecked |
| BUG-047 | CWE-457 [TX] | Uninit | harness_uninit.c:41 | frame_pool_use | pooled memory read before initialization |
| BUG-048 | CWE-457 | Uninit | harness_uninit.c:63 | frame_send_local | stack header partially initialized; flags read |
| BUG-049 | CWE-457 | Uninit | harness_uninit.c:17 | init_hdr | helper leaves flags/reserved unset; callers read them |
| BUG-050 | CWE-457 | Uninit | harness_uninit.c:127 | slot_track | caller ignores error contract; out-param used unassigned |
| BUG-051 | CWE-457 | Uninit | harness_uninit.c:143 | crc_check | fill loop stops one short; crcs[3] compared uninitialized |
| BUG-052 | CWE-457 | Uninit | harness_uninit.c:167 | stats_track | malloc'd snapshot read before zeroing |

Legend **[TX]** = additionally requires modeling ThreadX API semantics (byte/block pool
lifecycle, mutex ownership rules, kernel object create/delete).  These probe RTOS
knowledge rather than generic C analysis.

---

## 3. Size-arithmetic defects (BUG-001 .. BUG-012)

### BUG-001 — UINT16 truncation sizes the allocation, not the copy
- **CWE:** CWE-190 → CWE-787
- **Site:** `ports/win32/vs_2019/example_build/sample_threadx/harness/harness_sizearith.c:26`
- **Trigger path:** `producer_entry → harness_tick → sa_alloc_copy(count = g_sensor_count)` —
  `g_sensor_count` is a volatile simulated register.
- **Buggy code:**
  ```c
  size = (UINT16)(count * 4u);
  p = (UCHAR *)malloc((size_t)size);
  ...
  memcpy(p, sa_src, (size_t)count * 4u);
  ```
- **Why hard:** the allocation and the copy derive from the same source value through
  different expressions; detection requires integer-range reasoning that a volatile
  32-bit input can exceed 16383 so that the truncated size is smaller than the copy.
- **Runtime safety:** producer writes `g_sensor_count = tx_time_get() & 0xF` (0..15), so
  `size == count*4` exactly and nothing wraps.

### BUG-002 — magnitude check admits negative lengths
- **CWE:** CWE-197 (signed/unsigned) with CWE-787 effect
- **Site:** `harness_sizearith.c:46` in `sa_copy_checked`
- **Trigger path:** `producer_entry → harness_tick → sa_copy_checked(n = g_sensor_signed)`
- **Buggy code:**
  ```c
  if ((n < 64) && (n > -64))
  {
      memcpy(dst, sa_src, (size_t)n);
  }
  ```
- **Why hard:** the check looks like a range check but admits -63..-1, which convert to
  huge `size_t`; requires signed-range reasoning plus the implicit-conversion rule.
- **Runtime safety:** `g_sensor_signed = tx_time_get() & 0x7F` (0..127) — never negative.

### BUG-003 — off-by-one ring index (`<=` vs `<`)
- **CWE:** CWE-193 / CWE-787
- **Site:** `harness_sizearith.c:63` in `sa_log_write`
- **Trigger path:** `logger_entry → sa_log_write(code = g_sensor_raw)`
- **Buggy code:**
  ```c
  idx = (UINT)code >> 3;
  if (idx <= (UINT)LOG_DEPTH)     /* allows idx == LOG_DEPTH */
  ```
- **Why hard:** boundary reasoning on a derived index (`>> 3`) of a volatile value against
  a `#define` bound.
- **Runtime safety:** `g_sensor_raw` is 0..255 at runtime, so `idx` is 0..31 — index 32
  never occurs.

### BUG-004 — `sizeof(pointer)` in memcpy leaves struct partially initialized
- **CWE:** CWE-131 with CWE-457 effect
- **Site:** `harness_sizearith.c:111` in `sa_frame_new`
- **Trigger path:** `producer_entry → harness_tick → sa_frame_new`
- **Buggy code:**
  ```c
  memcpy(p, &stack_frame, sizeof(p));   /* copies 4 bytes, not sizeof(FRAME) */
  ...
  if ((p->hdr.seq & 0x1u) != 0u)        /* seq never initialized */
  ```
- **Why hard:** requires type reasoning that `sizeof(p)` is pointer size, then
  field-level reasoning that `hdr.seq` was never stored; the read is in a later
  statement (inter-statement flow).
- **Runtime safety:** the garbage read only steers a counter increment.

### BUG-005 — unguarded size addition wraps
- **CWE:** CWE-190 → CWE-787
- **Site:** `harness_sizearith.c:133` in `sa_pack`
- **Trigger path:** `producer_entry → harness_tick → sa_pack(payload_len = g_sensor_len)`
- **Buggy code:**
  ```c
  total = (ULONG)payload_len + 12u + 4u;      /* no overflow guard */
  p = (UCHAR *)malloc((size_t)total);
  memcpy(p + 12, sa_src, (size_t)payload_len);
  ```
- **Why hard:** wrap detection on a three-term addition of a volatile-derived UINT16,
  chained to an allocation-vs-copy mismatch.
- **Runtime safety:** `g_sensor_len` is 0..63 at runtime; total never exceeds 79.

### BUG-006 — truncated stored length vs full-width copy
- **CWE:** CWE-197 → CWE-787
- **Site:** `harness_sizearith.c:154` in `sa_store_len`
- **Trigger path:** `consumer_entry → harness_dispatch → sa_store_len(msg_len = g_sensor_raw)`
- **Buggy code:**
  ```c
  stored = (UINT16)msg_len;
  p = (UCHAR *)malloc((size_t)stored);   /* no overflow guard, no +1 */
  memcpy(p, sa_src, (size_t)msg_len);    /* full 32-bit length */
  ```
- **Why hard:** narrowing-conversion reasoning: `msg_len` is ULONG from a volatile
  register; for values ≥ 65536 the allocation is smaller than the copy.
- **Runtime safety:** runtime values are 0..255, so `stored == msg_len` and the copy fits.

### BUG-007 — missing column bounds check allows negative index
- **CWE:** CWE-787
- **Site:** `harness_sizearith.c:176` in `sa_table_put`
- **Trigger path:** `consumer_entry → harness_dispatch → sa_table_put(row, col = cmd->param >> 3, ...)`
- **Buggy code:**
  ```c
  if (row >= (UINT)SA_ROWS)
      return;
  ...
  sa_table[r][c] = value;   /* col unchecked: negative or >= SA_COLS */
  ```
- **Why hard:** only one of three bounds is checked; requires reasoning that `INT col`
  derived from a queue message (`param >> 3`) can be negative or exceed the row width,
  and how C flattens 2D indexing.
- **Runtime safety:** producer sends `param = tick % 32` → `col = 0..3` (in range);
  negative or oversized columns never occur at runtime.

### BUG-008 — `sizeof(pointer)` config snapshot
- **CWE:** CWE-131 with CWE-457 effect
- **Site:** `harness_sizearith.c:196` in `sa_cfg_snapshot`
- **Buggy code:**
  ```c
  memcpy(&local, cfg, sizeof(cfg));    /* 4 bytes, not sizeof(HARNESS_CFG) */
  if (local.rate > 1000u)              /* rate never initialized */
  ```
- **Why hard:** same class as BUG-004 but on a stack destination; the read of
  `local.rate` is several statements after the copy.
- **Runtime safety:** garbage only steers a counter; MSVC already warns C4700 here
  (evidence the defect is visible to a baseline compiler).

### BUG-009 — wrapped pool arithmetic + ignored allocation status
- **CWE:** CWE-190 → CWE-690/476
- **Site:** `harness_sizearith.c:231` in `sa_pool_resize`
- **Trigger path:** `producer_entry → harness_tick → sa_pool_resize(g_cfg.batch)`;
  `g_cfg.batch` is itself assigned in `harness_dispatch` from a received message.
- **Buggy code:**
  ```c
  bytes = (ULONG)nblocks * g_sensor_count * 72u;   /* can wrap */
  ...
  tx_byte_allocate(&probe_pool, &b, 64u, TX_NO_WAIT);   /* status ignored */
  memset(b, 0, 16);                                     /* b may be NULL */
  ```
- **Why hard:** chained defect — overflow in a three-factor multiply of a
  queue-correlated global and a volatile register makes the pool too small, then the
  ignored `tx_byte_allocate` status propagates a NULL into `memset`.  Requires both
  integer-range reasoning and API return-value modeling.
- **Runtime safety:** runtime factors are `batch ≤ 3` and `count ≤ 15` → bytes ≤ 3240;
  the allocation always succeeds.
- **[TX]-adjacent:** ThreadX byte-pool API return value.

### BUG-010 — unterminated buffer passed to strlen
- **CWE:** CWE-170
- **Site:** `harness_sizearith.c:252` in `sa_scan_raw`
- **Buggy code:**
  ```c
  if (fill_len > (UINT)sizeof(raw))
      fill_len = (UINT)sizeof(raw);      /* full fill leaves no room for NUL */
  ...
  raw[fill_len] = 0;                     /* writes raw[64] when fill_len == 64 */
  n = (UINT)strlen((const char *)raw);
  ```
- **Why hard:** requires reasoning that the boundary case leaves the buffer
  unterminated before `strlen`, and that the terminator write itself is out of bounds.
- **Runtime safety:** `fill_len` (g_sensor_len) is 0..63 at runtime; the boundary case
  never occurs.

### BUG-011 — masked length check, full-width copy
- **CWE:** CWE-704 (with CWE-787 effect)
- **Site:** `harness_sizearith.c:276` in `sa_masked_copy`
- **Buggy code:**
  ```c
  if ((msg_len & 0xFFFFu) < 64u)
      memcpy(dst, sa_src, (size_t)msg_len);   /* full 32-bit length */
  ```
- **Why hard:** requires reasoning that the 16-bit mask hides high bits, i.e. values
  like 0x1003F pass the check but overflow the 64-byte destination.
- **Runtime safety:** `g_sensor_raw` is 0..255: values below 64 copy safely; values
  64..255 fail the check and skip the copy.

### BUG-012 — unvalidated configured buffer size
- **CWE:** CWE-190 / CWE-787
- **Site:** `harness_sizearith.c:295` in `sa_hdr_store`
- **Buggy code:**
  ```c
  size = (UINT)g_hdr_pad;               /* volatile config, never validated */
  p = (UCHAR *)malloc((size_t)size);
  memcpy(p, &h, sizeof(FRAME_HDR));     /* writes 12 bytes */
  ```
- **Why hard:** config-value range reasoning: a volatile "pad" register below
  `sizeof(FRAME_HDR)` produces an undersized heap buffer.
- **Runtime safety:** producer writes `g_hdr_pad` as 12 or 16 — never below 12.

---

## 4. Memory-lifecycle defects (BUG-013 .. BUG-022)

### BUG-013 — write after release on the queue-full path
- **CWE:** CWE-416
- **Site:** `harness_memory.c:45` in `pipeline_submit`
- **Trigger path:** `producer_entry → pipeline_submit(f)`; `frame_send` fails only when
  the frame queue is full.
- **Buggy code:**
  ```c
  tx_byte_release(f);
  f->hdr.flags = (UCHAR)(f->hdr.flags & 0xFDu);   /* use after release */
  ```
- **Why hard:** the release is a ThreadX pool API; detection requires either
  ThreadX modeling (`tx_byte_release` = memory invalidation) or treating it as an
  opaque sink plus local flow.  **[TX]**
- **Runtime safety:** the frame queue never fills (consumer drains with
  TX_WAIT_FOREVER at higher priority), so the path never executes.

### BUG-014 — caller ignores callee's release-on-reject contract
- **CWE:** CWE-415
- **Site:** `harness_concurrency.c:203` in `queue_drain_safe`
- **Trigger path:** `maint_entry → queue_drain_safe → normalize_frame`
- **Buggy code:**
  ```c
  (void)normalize_frame(f);      /* callee releases rejected frames */
  tx_byte_release(f);            /* unconditional second release */
  ```
- **Why hard:** interprocedural ownership contract — the callee (`normalize_frame`)
  releases the buffer on one path and returns HR_FAIL; the caller must correlate the
  return value with the buffer's validity.  **[TX]** (byte-pool release semantics)
- **Runtime safety:** frames always carry `type ≤ 3`, so the callee never rejects and
  releases exactly once.  Related to BUG-017: the two sides of the same contract are
  independent defect sites; either alone produces a double release.

### BUG-015 — early return leaks the allocated chain
- **CWE:** CWE-401
- **Site:** `harness_memory.c:85` in `frame_chain_check`
- **Buggy code:**
  ```c
  if (g_stress_level != 0u)
  {
      return HR_FAIL;            /* frames[0..n] still allocated */
  }
  ```
- **Why hard:** resource-leak reasoning over an array of pooled pointers with an
  early exit in the middle of a two-phase function (allocate-all, then release-all).
  **[TX]** (byte pool as allocator)
- **Runtime safety:** `g_stress_level` is a stress hook that stays 0 at runtime.

### BUG-016 — block released to the wrong pool
- **CWE:** CWE-762
- **Site:** `harness_memory.c:125` in `block_use`
- **Buggy code:**
  ```c
  if ((g_stress_level & 4u) != 0u)
  {
      tx_byte_release(b);        /* allocated via tx_block_allocate! */
  }
  ```
- **Why hard:** requires ThreadX API-pair modeling: `tx_block_allocate` buffers must be
  returned with `tx_block_release`.  **[TX]** — a direct probe of RTOS knowledge.
- **Runtime safety:** stress hook is 0 at runtime.

### BUG-017 — callee releases and reports success
- **CWE:** CWE-415
- **Site:** `harness_memory.c:148` in `normalize_frame`
- **Buggy code:**
  ```c
  if (f->hdr.type > 3u)
  {
      tx_byte_release(f);
      return HR_OK;              /* caller will release again */
  }
  ```
- **Why hard:** the return value lies about ownership; paired with the (correct)
  caller check `== HR_OK` this still double-releases.  Requires correlating release
  sites with return-value semantics across functions.  **[TX]**
- **Runtime safety:** `type ≤ 3` always at runtime; branch never taken.

### BUG-018 — loop keeps using the freed batch
- **CWE:** CWE-416 (secondary CWE-415 on a second failure)
- **Site:** `harness_memory.c:175` in `batch_send`
- **Buggy code:**
  ```c
  if (frame_send(&batch[i]) != HR_OK)
  {
      free(batch);              /* whole contiguous block */
      /* loop continues: next iteration reads batch[i+1] */
  }
  ```
- **Why hard:** loop-carried use-after-free: `free()` inside the loop body while the
  loop condition and next iteration still dereference the block; also a potential
  double free if a second iteration fails.
- **Runtime safety:** the frame queue never rejects, so the branch never executes.

### BUG-019 — created timer leaked on abort path
- **CWE:** CWE-404
- **Site:** `harness_memory.c:206` in `pipeline_stop`
- **Buggy code:**
  ```c
  status = tx_timer_create(&probe_timer, ...);
  ...
  if ((g_stress_level & 8u) != 0u)
  {
      return HR_OK;              /* probe_timer never deleted */
  }
  ```
- **Why hard:** kernel-object lifecycle pairing (create/delete) across an early exit.
  **[TX]** — probes whether the tool models TX_TIMER at all.
- **Runtime safety:** stress hook is 0.

### BUG-020 — interior pointer released
- **CWE:** CWE-761
- **Site:** `harness_memory.c:233` in `interior_test`
- **Buggy code:**
  ```c
  if ((g_stress_level & 16u) != 0u)
  {
      free(body);                /* body = base + 16 */
  }
  ```
- **Why hard:** pointer-aliasing/arithmetic reasoning: the freed pointer is derived
  from the allocation by an offset.
- **Runtime safety:** stress hook is 0.

### BUG-021 — aliasing cache slots cause double free
- **CWE:** CWE-415
- **Site:** `harness_memory.c:257` in `cache_store`
- **Buggy code:**
  ```c
  frame_cache[0] = f;
  frame_cache[1] = f;            /* cache_flush frees both */
  ```
- **Why hard:** the double free materializes in a *different function* (`cache_flush`)
  that iterates the cache; detection requires pointer-equality reasoning across
  functions and an alias condition under a stress branch.
- **Runtime safety:** stress hook is 0; runtime stores rotate distinct frames.

### BUG-022 — retry loop orphans the failed attempt's buffer
- **CWE:** CWE-401
- **Site:** `harness_memory.c:311` in `retry_send`
- **Buggy code:**
  ```c
  while (tries > 0)
  {
      p = (UCHAR *)malloc((size_t)size);
      ...
      /* no free(p) when the send fails and we retry */
      tries = tries - 1u;
  }
  ```
- **Why hard:** loop-carried allocation tracking: the previous iteration's `p` is
  overwritten while still allocated.
- **Runtime safety:** the probe queue send never fails at runtime (paired immediate
  receive), so the retry path never executes.

---

## 5. Concurrency defects (BUG-023 .. BUG-030)

All concurrency defects require the analyzer to understand that the four harness
threads (created via `tx_thread_create` in `harness_init`) run concurrently.
Parasoft models ThreadX thread creation; a tool without RTOS thread modeling cannot
see the concurrency at all — this family is the sharpest differentiator in the set.

### BUG-023 — unlocked read-modify-write of shared stats
- **CWE:** CWE-362
- **Site:** `harness_concurrency.c:37` in `stats_record_fast`
- **Trigger:** called by both the producer thread (`producer_entry`) and the consumer
  thread (`harness_dispatch`), while `stats_record` updates the same field under
  `stats_mutex`.
- **Buggy code:**
  ```c
  g_stats.total = g_stats.total + v;    /* no stats_mutex */
  ```
- **Runtime safety:** 32-bit aligned reads/writes on x86; worst case lost updates.

### BUG-024 — slot hand-off outside the critical section
- **CWE:** CWE-366 / CWE-362
- **Site:** `harness_concurrency.c:56` in `slot_use` (called from consumer **and**
  maintenance threads)
- **Buggy code:** the lock is taken and released, then `g_slot` is published and
  dereferenced after the critical section.
- **Runtime safety:** slot storage is static; races cause at worst stale reads.

### BUG-025 — lock-order inversion
- **CWE:** CWE-833 (CWE-567)
- **Site:** `harness_concurrency.c:87` in `net_update_b`
- **Buggy code:** `net_update_b` takes `net_mutex_2` then `net_mutex_1`;
  `net_update_a` takes them in the opposite order.
- **Why hard:** requires collecting lock-acquisition orders across functions and
  checking for cycles (classic potential-deadlock detection).
- **Runtime safety:** both functions are only called sequentially by the maintenance
  thread, so no deadlock materializes at runtime.

### BUG-026 — unsynchronized spin on a shared flag
- **CWE:** CWE-364
- **Site:** `harness_concurrency.c:108` in `wait_ready`
- **Buggy code:**
  ```c
  while ((g_ready == 0u) && (spins < 1000u)) { spins++; tx_thread_relinquish(); }
  ```
- **Why hard:** the flag is volatile but there is no lock, barrier or event
  synchronization; the check-then-act after the loop races with `lazy_init`.
- **Runtime safety:** consumer calls `lazy_init()` first, so `g_ready` is 1 and the
  loop exits immediately.

### BUG-027 — unlocked list append while readers walk the list
- **CWE:** CWE-362
- **Site:** `harness_concurrency.c:134` in `job_append` (writer: maintenance thread;
  reader: logger thread in `job_tail_value`).
- **Buggy code:** under the stress branch the list is prepended without `table_mutex`.
- **Runtime safety:** stress hook is 0; runtime appends are locked.

### BUG-028 — non-owner mutex put
- **CWE:** CWE-667 / CWE-369 **[TX]**
- **Site:** `harness_concurrency.c:153` in `mutex_teardown`
- **Buggy code:**
  ```c
  (void)tx_mutex_put(&net_mutex_1);   /* this thread does not own it */
  ```
- **Why hard:** requires ThreadX ownership rules (put without prior get returns
  TX_NOT_OWNED) plus ignored-return reasoning.
- **Runtime safety:** ThreadX rejects the put; harmless.

### BUG-029 — racy double-checked initialization
- **CWE:** CWE-366
- **Site:** `harness_concurrency.c:164` in `lazy_init` (called from consumer and
  maintenance threads)
- **Buggy code:** the `g_inited` guard is read outside the lock; the lock is only
  taken inside the branch — the inner locking *looks* correct, which is the trap.
- **Runtime safety:** initialization is idempotent; worst case duplicate benign init.

### BUG-030 — TOCTOU between queue count and receive, status ignored
- **CWE:** CWE-366 / CWE-252 **[TX]**
- **Site:** `harness_concurrency.c:190` in `queue_drain_safe`
- **Buggy code:**
  ```c
  status = tx_queue_info_get(&harness_frame_queue, TX_NULL, &enqueued, ...);
  if ((status != TX_SUCCESS) || (enqueued == 0u)) break;
  f = TX_NULL;
  (void)tx_queue_receive(&harness_frame_queue, &f, TX_NO_WAIT);
  if (f->hdr.magic != FRAME_HDR_MAGIC) break;    /* f may be NULL */
  ```
- **Why hard:** requires queue-state reasoning: between the count check and the
  receive, the higher-priority consumer can drain the queue, so `f` stays NULL and is
  dereferenced.
- **Runtime safety:** the consumer (higher priority) has always drained the queue by
  the time maintenance runs, so `enqueued == 0` and the buggy path is never entered.

---

## 6. NULL / validation-path defects (BUG-031 .. BUG-038)

### BUG-031 — validated the wrong lookup result
- **CWE:** CWE-476
- **Site:** `harness_nullpaths.c:74` in `channel_update`
- **Buggy code:**
  ```c
  primary = lookup_channel(id);      /* may return NULL for id >= 8 */
  mirror  = accessor_channel(id);    /* may return NULL for disabled channels */
  if (mirror != TX_NULL)
  {
      primary->state = mirror->state;   /* primary unchecked */
  ```
- **Why hard:** two similar calls; the NULL check tests the wrong variable.  Requires
  distinguishing which call's result is dereferenced.
- **Runtime safety:** runtime ids are 0..3 → both lookups always succeed.

### BUG-032 — unchecked lookup dereference
- **CWE:** CWE-476
- **Site:** `harness_nullpaths.c:94` in `sensor_dispatch`
- **Trigger path:** `consumer_entry → harness_dispatch → sensor_dispatch(id = cmd->sensor_id)`
- **Buggy code:** `ch = lookup_channel(id); ch->errors = ...;`
- **Runtime safety:** received ids are 0..3 at runtime.

### BUG-033 — unchecked malloc result
- **CWE:** CWE-690 / CWE-476
- **Site:** `harness_nullpaths.c:113` in `slot_allocate`
- **Buggy code:** `s = malloc(sizeof(SLOT)); s->seq = 0;` without the NULL check.
- **Runtime safety:** the machine never exhausts memory in this workload.

### BUG-034 — out-param used despite error return
- **CWE:** CWE-457 / CWE-252
- **Site:** `harness_nullpaths.c:151` in `frame_consume`
- **Buggy code:**
  ```c
  (void)get_frame(id, &f);        /* on error, f is never assigned */
  tx_byte_release(f);
  ```
- **Why hard:** requires reading the callee's contract (all `*out` assignments are on
  the success path) and correlating it with the ignored return value.
- **Runtime safety:** `get_frame` blocks on the queue (TX_WAIT_FOREVER) and only fails
  if the queue is deleted, which never happens.

### BUG-035 — dereference before the NULL check
- **CWE:** CWE-476
- **Site:** `harness_nullpaths.c:166` in `cfg_rate`
- **Buggy code:**
  ```c
  rate = cfg->rate;               /* deref first */
  if (cfg == TX_NULL) return HR_FAIL;
  ```
- **Trigger path:** `harness_dispatch → cfg_rate(lookup_config(cmd->opcode), &rate)`;
  `lookup_config` returns NULL for opcodes ≥ 8.
- **Runtime safety:** producer opcodes are `(tick & 7) | 1` → always < 8.

### BUG-036 — optional callback invoked unconditionally
- **CWE:** CWE-690
- **Site:** `harness_nullpaths.c:184` in `notify_fire`
- **Trigger path:** `harness_dispatch → notify_fire((cmd->opcode & 1) ? telemetry_notify : TX_NULL)`
- **Buggy code:** `cb(1u);` without the NULL check.
- **Runtime safety:** the producer always sets feature bit 0, so the callback is
  never NULL at runtime — but the *selection expression* is queue-correlated, so the
  NULL arm is statically reachable.

### BUG-037 — guard protects the loop, not the return
- **CWE:** CWE-476 / CWE-457
- **Site:** `harness_nullpaths.c:200` in `job_tail_value`
- **Buggy code:**
  ```c
  if (g_job_head != TX_NULL)
  {
      n = g_job_head;
      while (n->next != TX_NULL) n = n->next;
      v = n->value;
  }
  return v;                       /* v unassigned when list is empty */
  ```
- **Why hard:** the check *exists* but does not cover the empty-list path of `v` (and
  `n`); requires path-sensitive reasoning that the guarded branch is not exhaustive.
- **Runtime safety:** the logger sleeps 32 ticks before its first query, and the
  maintenance thread appends its first job long before that, so the list is never
  empty when queried.

### BUG-038 — unchecked accessor across a four-hop chain
- **CWE:** CWE-476
- **Site:** `harness_nullpaths.c:224` in `probe_level2`
- **Trigger path:** `harness_dispatch → sensor_deep_probe → probe_level1 → probe_level2 → accessor_channel`
- **Buggy code:** `c = accessor_channel(id); c->errors = ...;`
- **Why hard:** the NULL source is a volatile channel-enable mask three frames up
  the stack; requires interprocedural NULL propagation through 4 calls.
- **Runtime safety:** mask is 0xFF (all enabled) and ids are 0..3.

---

## 7. Taint / bounds defects (BUG-039 .. BUG-046)

All inputs in this family arrive through `harness_cmd_queue`; the producer fills
`sensor_id` as 0..3, `param` as 0..31, and `name` as `"cmd-N"` (≤ 7 characters).  The
analyzers cannot correlate queue payloads across threads, so these values are
unknown-range at the defect sites.

### BUG-039 — unmasked table index
- **CWE:** CWE-129 / CWE-787
- **Site:** `harness_taint.c:56` in `cal_update`
- **Buggy code:** `idx = (UINT)m->sensor_id; cal_table[idx] += ...;` (mask removed).
- **Runtime safety:** ids are 0..3 at runtime; table holds 8 entries.

### BUG-040 — length validated against the wrong constant
- **CWE:** CWE-787
- **Site:** `harness_taint.c:71` in `msg_data_copy`
- **Buggy code:**
  ```c
  len = (UINT16)((ULONG)m->param + 4u);
  if (len <= 128u)                     /* queue capacity, not sizeof(dst)=64 */
      memcpy(dst, m, (size_t)len);
  ```
- **Runtime safety:** runtime lengths are ≤ 35 bytes, which fits the 64-byte scratch.

### BUG-041 — unbounded strcpy into a fixed label
- **CWE:** CWE-120
- **Site:** `harness_taint.c:86` in `name_copy`
- **Buggy code:** `strcpy(label, (const char *)m->name);` (length check removed).
- **Runtime safety:** names are ≤ 7 characters at runtime; MSVC flags the raw strcpy
  use as C4996 (deprecation), a rule-level (not flow-level) baseline finding.

### BUG-042 — bound checked against a stale constant
- **CWE:** CWE-129 / CWE-787
- **Site:** `harness_taint.c:101` in `slot_store`
- **Buggy code:**
  ```c
  if (idx < 8u)                       /* array has only SLOT_VALUES_SIZE = 4 */
      slot_values[idx] = (ULONG)m->param;
  ```
- **Why hard:** the check *looks* present; detection requires comparing the bound
  against the actual array size, not just noting a check exists.
- **Runtime safety:** runtime ids are 0..3.

### BUG-043 — unclamped accumulator loop
- **CWE:** CWE-787
- **Site:** `harness_taint.c:116` in `sums_update`
- **Buggy code:** `n = m->param >> 2; for (i = 0; i < n; i++) sums[i] += 1;` (clamp to
  SUMS_SIZE removed).
- **Runtime safety:** runtime params are 0..31 → `n` is 0..7, within the 8-entry array.

### BUG-044 — received text as printf format
- **CWE:** CWE-134
- **Site:** `harness_taint.c:133` in `log_text`
- **Buggy code:** `printf((const char *)text);`
- **Runtime safety:** runtime text is `"cmd-N"`, harmless when interpreted as a format.

### BUG-045 — allocation omits the terminator on the legacy branch
- **CWE:** CWE-680 / CWE-787
- **Site:** `harness_taint.c:151` in `dup_string`
- **Buggy code:**
  ```c
  if (((UCHAR)s[0] & 0x80u) != 0u)   /* legacy binary-name branch */
      need = strlen(s);               /* no +1 */
  ...
  strcpy(p, s);                       /* writes terminator past the end */
  ```
- **Runtime safety:** names start with 'c' (0x63), so the high-bit branch never runs.

### BUG-046 — unchecked nibble index into a function-pointer table
- **CWE:** CWE-129 / CWE-787
- **Site:** `harness_taint.c:179` in `nibble_dispatch`
- **Buggy code:** `type = (raw >> 4) & 0xF; handlers[type]();` (bounds check removed;
  table holds 12 entries, 16 nibble values possible).
- **Runtime safety:** runtime `raw` is 0..31 → `type` is 0..1.

---

## 8. Uninitialized-memory defects (BUG-047 .. BUG-052)

### BUG-047 — pooled memory read before initialization
- **CWE:** CWE-457 **[TX]**
- **Site:** `harness_uninit.c:41` in `frame_pool_use`
- **Buggy code:** the `memset(f, 0, sizeof(FRAME))` after `tx_byte_allocate` is
  removed; `f->hdr.flags` is then read.
- **Why hard:** requires knowing that ThreadX byte-pool memory is not zeroed (an API
  semantic) before flagging the read.  **[TX]**
- **Runtime safety:** the pool area is a static array, zero-initialized in this build;
  the read yields 0 and steers a counter.

### BUG-048 — partially initialized stack header
- **CWE:** CWE-457
- **Site:** `harness_uninit.c:63` in `frame_send_local`
- **Buggy code:** only `magic` and `seq` are set; `h.flags` is read afterwards.
- **Note:** MSVC already emits warning C4700 here (compiler-visible defect — a useful
  baseline for the tool comparison).
- **Runtime safety:** garbage flags only steer a counter.

### BUG-049 — init helper leaves fields unset (interprocedural)
- **CWE:** CWE-457
- **Site:** `harness_uninit.c:17` in `init_hdr`
- **Buggy code:** `flags` and `reserved` are no longer assigned; multiple callers
  (`frame_build`, `hdr_send`, `frame_chain_check`, `batch_send`, `sa_hdr_store`)
  receive partially initialized headers, and `hdr_send` reads `h.flags`.
- **Why hard:** the defect is in a widely-called helper; detection requires
  whole-program initialization tracking to the read sites.
- **Runtime safety:** pool/stack memory in this build is mostly zeroed; reads steer
  counters only.

### BUG-050 — caller ignores the out-param error contract
- **CWE:** CWE-457 / CWE-252
- **Site:** `harness_uninit.c:127` in `slot_track`
- **Buggy code:** `SLOT *u; (void)get_slot(&u); free(u);` — `get_slot` never assigns
  `*out` when malloc fails.
- **Runtime safety:** malloc never fails in this workload.

### BUG-051 — fill loop stops one short
- **CWE:** CWE-457
- **Site:** `harness_uninit.c:143` in `crc_check`
- **Buggy code:** `for (i = 0; i < 3; i++) crcs[i] = ...;` then `crcs[3]` is compared.
- **Runtime safety:** stack garbage only affects the comparison result.

### BUG-052 — snapshot read before zeroing
- **CWE:** CWE-457
- **Site:** `harness_uninit.c:167` in `stats_track`
- **Buggy code:** `st->total = v; if (st->peak > 100u)` with no `memset` after malloc.
- **Runtime safety:** heap garbage only steers a counter.

---

## 9. Adjudication rules (for the comparison report)

When mapping tool findings to this catalog:

1. **True Positive (TP):** a finding whose primary location is at, or on a path
   directly implicated by, a defect site above (± a few lines), and whose message
   describes the planted defect's root cause.
2. **False Negative (FN):** a catalog defect with no matching finding by that tool.
3. **False Positive (FP):** a finding on code that is **not** a planted defect site
   and does not describe a genuine defect of the clean harness or upstream ThreadX
   code (judged against the `harness-clean` baseline and manual review).
4. **Pre-existing/upstream:** findings on pristine ThreadX kernel code (also present
   in the `baseline-clean` state) are counted separately; they are real-code findings
   of the upstream project, not FPs of this evaluation.
5. The compiler's own warnings (e.g. MSVC C4700 for BUG-008/BUG-048, C4996 for the
   strcpy sites) are recorded as a free "baseline detector" column in the report.
