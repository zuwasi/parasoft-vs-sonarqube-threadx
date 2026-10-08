/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* Deliberate-defect test harness for the ThreadX Win32 sample application.*/
/* Part of a static analysis tool comparison (see evaluation/PLANTED_BUGS.md). */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#ifndef HARNESS_COMMON_H
#define HARNESS_COMMON_H

#include "tx_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------- */
/* Limits and constants                                                   */
/* ---------------------------------------------------------------------- */

#define HARNESS_POOL_SIZE         32768
#define HARNESS_BLOCK_SIZE        64
#define HARNESS_BLOCK_COUNT      64
#define HARNESS_STACK_SIZE        1024
#define HARNESS_CMD_QUEUE_DEPTH   32
#define HARNESS_FRAME_QUEUE_DEPTH 16
#define HARNESS_PROBE_QUEUE_DEPTH 4

#define FRAME_PAYLOAD_CAP         64
#define FRAME_HDR_MAGIC           0xA55Au
#define CMD_NAME_CAP              32
#define LOG_DEPTH                 32
#define SA_ROWS                   4
#define SA_COLS                   4
#define MAX_SENSORS               8
#define CAL_TABLE_SIZE            8
#define SUMS_SIZE                 8
#define SLOT_VALUES_SIZE          4
#define HANDLER_COUNT             12
#define PIPELINE_BATCH            4

#define HR_OK                     0u
#define HR_FAIL                   1u

/* ---------------------------------------------------------------------- */
/* Types                                                                   */
/* ---------------------------------------------------------------------- */

typedef struct FRAME_HDR_STRUCT
{
    UINT16 magic;
    UCHAR  type;
    UCHAR  flags;
    ULONG  seq;
    UINT16 payload_len;
    UINT16 reserved;
} FRAME_HDR;

typedef struct FRAME_STRUCT
{
    FRAME_HDR hdr;
    UCHAR     payload[FRAME_PAYLOAD_CAP];
} FRAME;

typedef struct CMD_MSG_STRUCT
{
    UCHAR  opcode;
    UCHAR  sensor_id;
    UINT16 param;
    UCHAR  name[CMD_NAME_CAP];
} CMD_MSG;

typedef struct HARNESS_CFG_STRUCT
{
    UINT16 mode;
    UINT16 batch;
    ULONG  rate;
    ULONG  flags;
    CHAR   label[12];
} HARNESS_CFG;

typedef struct LOG_ENTRY_STRUCT
{
    UINT16 code;
    UINT16 seq;
    ULONG  stamp;
} LOG_ENTRY;

typedef struct JOB_STRUCT
{
    struct JOB_STRUCT *next;
    ULONG              value;
} JOB;

typedef struct CHANNEL_STRUCT
{
    UINT  id;
    UINT  state;
    ULONG errors;
} CHANNEL;

typedef struct SLOT_STRUCT
{
    ULONG seq;
    ULONG len;
    UCHAR data[16];
} SLOT;

typedef struct STATS_STRUCT
{
    ULONG total;
    ULONG errors;
    ULONG peak;
} STATS;

/* ---------------------------------------------------------------------- */
/* Kernel objects and shared state (defined in harness_init.c)            */
/* ---------------------------------------------------------------------- */

extern TX_BYTE_POOL         harness_byte_pool;
extern TX_BLOCK_POOL        harness_block_pool;
extern TX_QUEUE             harness_cmd_queue;
extern TX_QUEUE             harness_frame_queue;
extern TX_QUEUE             harness_probe_queue;
extern TX_MUTEX             stats_mutex;
extern TX_MUTEX             table_mutex;
extern TX_MUTEX             net_mutex_1;
extern TX_MUTEX             net_mutex_2;
extern TX_EVENT_FLAGS_GROUP harness_events;
extern TX_SEMAPHORE         slot_sem;

/* Simulated hardware inputs.  Volatile mirrors a memory-mapped register:
   static analyzers must treat every read as a fresh, externally
   changeable value.  At runtime the producer thread only ever writes
   small, in-range values into them (see harness_init.c). */
extern volatile ULONG g_sensor_raw;      /* 0 .. 255 at runtime   */
extern volatile LONG  g_sensor_signed;   /* 0 .. 127 at runtime   */
extern volatile ULONG g_sensor_id;       /* 0 .. 7 at runtime     */
extern volatile ULONG g_sensor_len;      /* 0 .. 63 at runtime    */
extern volatile ULONG g_sensor_count;    /* 0 .. 15 at runtime    */
extern volatile ULONG g_hdr_pad;         /* 12 or 16 at runtime   */
extern volatile ULONG g_stress_level;    /* stress hook: stays 0  */

extern STATS        g_stats;
extern HARNESS_CFG  g_cfg;

/* ---------------------------------------------------------------------- */
/* Harness lifecycle (harness_init.c)                                      */
/* ---------------------------------------------------------------------- */

UINT  harness_init(void);
void  harness_tick(UINT tick);
void  harness_dispatch(CMD_MSG *cmd);

/* ---------------------------------------------------------------------- */
/* Size arithmetic family (harness_sizearith.c)                            */
/* ---------------------------------------------------------------------- */

UINT  sa_alloc_copy(UINT count);
UINT  sa_copy_checked(INT n);
void  sa_log_write(UINT16 code);
void  sa_log_flush(void);
UINT  sa_frame_new(void);
UINT  sa_pack(UINT16 payload_len);
UINT  sa_store_len(ULONG msg_len);
void  sa_table_put(UINT row, INT col, ULONG value);
UINT  sa_cfg_snapshot(void);
UINT  sa_pool_resize(UINT nblocks);
UINT  sa_scan_raw(UINT fill_len);
UINT  sa_masked_copy(ULONG msg_len);
UINT  sa_hdr_store(void);

/* ---------------------------------------------------------------------- */
/* Memory family (harness_memory.c)                                       */
/* ---------------------------------------------------------------------- */

UINT  frame_send(FRAME *f);
UINT  pipeline_submit(FRAME *f);
UINT  frame_chain_check(void);
UINT  block_use(void);
UINT  normalize_frame(FRAME *f);
UINT  batch_send(FRAME *batch, UINT count);
UINT  pipeline_stop(void);
UINT  interior_test(void);
UINT  cache_store(FRAME *f);
UINT  cache_flush(void);
UINT  retry_send(UINT size);

/* ---------------------------------------------------------------------- */
/* Concurrency family (harness_concurrency.c)                              */
/* ---------------------------------------------------------------------- */

void  stats_record(ULONG v);
void  stats_record_fast(ULONG v);
UINT  slot_use(void);
void  net_update_a(void);
void  net_update_b(void);
void  wait_ready(void);
void  job_append(ULONG v);
void  mutex_teardown(void);
void  lazy_init(void);
UINT  queue_drain_safe(void);

/* ---------------------------------------------------------------------- */
/* NULL / validation-path family (harness_nullpaths.c)                     */
/* ---------------------------------------------------------------------- */

CHANNEL *lookup_channel(UINT id);
CHANNEL *accessor_channel(UINT id);
HARNESS_CFG *lookup_config(UINT opcode);
UINT  channel_update(UINT id);
UINT  sensor_dispatch(UINT id);
UINT  slot_allocate(SLOT **out);
UINT  frame_consume(UINT id);
UINT  get_frame(UINT id, FRAME **out);
UINT  cfg_rate(HARNESS_CFG *cfg, ULONG *out);
void  notify_fire(void (*cb)(ULONG));
ULONG job_tail_value(void);
UINT  sensor_deep_probe(UINT id);

/* ---------------------------------------------------------------------- */
/* Taint / bounds family (harness_taint.c)                                 */
/* ---------------------------------------------------------------------- */

void  cal_update(CMD_MSG *m);
void  msg_data_copy(CMD_MSG *m);
void  name_copy(CMD_MSG *m);
void  slot_store(CMD_MSG *m);
void  sums_update(CMD_MSG *m);
void  log_text(const CHAR *text);
UINT  dup_string(const CHAR *s);
UINT  nibble_dispatch(UCHAR raw);

/* ---------------------------------------------------------------------- */
/* Uninitialized-memory family (harness_uninit.c)                          */
/* ---------------------------------------------------------------------- */

void  init_hdr(FRAME_HDR *h);
UINT  frame_pool_use(void);
UINT  frame_send_local(void);
UINT  hdr_send(UINT16 len);
UINT  get_slot(SLOT **out);
UINT  crc_check(UINT expected);
UINT  stats_track(ULONG v);
UINT  slot_track(void);

#endif /* HARNESS_COMMON_H */
