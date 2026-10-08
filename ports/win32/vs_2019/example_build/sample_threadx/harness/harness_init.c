/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* Harness core: kernel objects, worker threads and dispatch loops for the*/
/* sensor-telemetry pipeline.  The worker threads drive every function of */
/* the per-family modules so that the whole call graph is exercised.       */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#include "harness_common.h"

/* Kernel objects */
TX_BYTE_POOL         harness_byte_pool;
TX_BLOCK_POOL        harness_block_pool;
TX_QUEUE             harness_cmd_queue;
TX_QUEUE             harness_frame_queue;
TX_QUEUE             harness_probe_queue;
TX_MUTEX             stats_mutex;
TX_MUTEX             table_mutex;
TX_MUTEX             net_mutex_1;
TX_MUTEX             net_mutex_2;
TX_EVENT_FLAGS_GROUP harness_events;
TX_SEMAPHORE         slot_sem;

/* Simulated hardware inputs (see harness_common.h for rationale) */
volatile ULONG g_sensor_raw = 0;
volatile LONG  g_sensor_signed = 0;
volatile ULONG g_sensor_id = 0;
volatile ULONG g_sensor_len = 0;
volatile ULONG g_sensor_count = 0;
volatile ULONG g_hdr_pad = 0;
volatile ULONG g_stress_level = 0;

/* Shared state */
STATS        g_stats;
HARNESS_CFG  g_cfg;

/* Static storage areas */
static UCHAR harness_pool_area[HARNESS_POOL_SIZE];
static UCHAR harness_block_area[HARNESS_BLOCK_SIZE * HARNESS_BLOCK_COUNT];
static ULONG cmd_queue_area[HARNESS_CMD_QUEUE_DEPTH];
static ULONG frame_queue_area[HARNESS_FRAME_QUEUE_DEPTH];
static ULONG probe_queue_area[HARNESS_PROBE_QUEUE_DEPTH];

/* Worker threads */
static TX_THREAD producer_thread;
static TX_THREAD consumer_thread;
static TX_THREAD logger_thread;
static TX_THREAD maint_thread;
static UCHAR producer_stack[HARNESS_STACK_SIZE];
static UCHAR consumer_stack[HARNESS_STACK_SIZE];
static UCHAR logger_stack[HARNESS_STACK_SIZE];
static UCHAR maint_stack[HARNESS_STACK_SIZE];

static void producer_entry(ULONG arg);
static void consumer_entry(ULONG arg);
static void logger_entry(ULONG arg);
static void maint_entry(ULONG arg);
static void build_cmd(CMD_MSG *cmd, UINT tick);
static void cmd_post(CMD_MSG *cmd);
static FRAME *frame_build(UINT tick);

static void telemetry_notify(ULONG v)
{
    g_stats.total = g_stats.total + 1;
    (void)v;
}

UINT harness_init(void)
{
    UINT status;

    status = tx_byte_pool_create(&harness_byte_pool, "harness byte pool",
                                harness_pool_area, (ULONG)sizeof(harness_pool_area));
    if (status != TX_SUCCESS)
        return status;

    status = tx_block_pool_create(&harness_block_pool, "harness block pool",
                                 HARNESS_BLOCK_SIZE, harness_block_area, (ULONG)sizeof(harness_block_area));
    if (status != TX_SUCCESS)
        return status;

    status = tx_queue_create(&harness_cmd_queue, "harness cmd queue", TX_1_ULONG,
                             (VOID *)cmd_queue_area, (ULONG)sizeof(cmd_queue_area));
    if (status != TX_SUCCESS)
        return status;

    status = tx_queue_create(&harness_frame_queue, "harness frame queue", TX_1_ULONG,
                             (VOID *)frame_queue_area, (ULONG)sizeof(frame_queue_area));
    if (status != TX_SUCCESS)
        return status;

    status = tx_queue_create(&harness_probe_queue, "harness probe queue", TX_1_ULONG,
                             (VOID *)probe_queue_area, (ULONG)sizeof(probe_queue_area));
    if (status != TX_SUCCESS)
        return status;

    status = tx_mutex_create(&stats_mutex, "stats mutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS)
        return status;

    status = tx_mutex_create(&table_mutex, "table mutex", TX_NO_INHERIT);
    if (status != TX_SUCCESS)
        return status;

    status = tx_mutex_create(&net_mutex_1, "net mutex 1", TX_NO_INHERIT);
    if (status != TX_SUCCESS)
        return status;

    status = tx_mutex_create(&net_mutex_2, "net mutex 2", TX_NO_INHERIT);
    if (status != TX_SUCCESS)
        return status;

    status = tx_event_flags_create(&harness_events, "harness events");
    if (status != TX_SUCCESS)
        return status;

    status = tx_semaphore_create(&slot_sem, "harness slot sem", 1);
    if (status != TX_SUCCESS)
        return status;

    /* Default telemetry configuration */
    g_cfg.mode = 1;
    g_cfg.batch = PIPELINE_BATCH;
    g_cfg.rate = 1000;
    g_cfg.flags = 1;
    strcpy(g_cfg.label, "telemetry");

    g_stats.total = 0;
    g_stats.errors = 0;
    g_stats.peak = 0;

    status = tx_thread_create(&producer_thread, "harness producer", producer_entry, 0,
                              producer_stack, (ULONG)sizeof(producer_stack),
                              15, 15, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS)
        return status;

    status = tx_thread_create(&consumer_thread, "harness consumer", consumer_entry, 0,
                              consumer_stack, (ULONG)sizeof(consumer_stack),
                              16, 16, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS)
        return status;

    status = tx_thread_create(&maint_thread, "harness maintenance", maint_entry, 0,
                              maint_stack, (ULONG)sizeof(maint_stack),
                              17, 17, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS)
        return status;

    status = tx_thread_create(&logger_thread, "harness logger", logger_entry, 0,
                              logger_stack, (ULONG)sizeof(logger_stack),
                              18, 18, TX_NO_TIME_SLICE, TX_AUTO_START);
    return status;
}

/* Producer: simulates live inputs, posts commands and frames, and rotates
   through the producer-side family routines. */
static void producer_entry(ULONG arg)
{
    UINT     tick;
    CMD_MSG  cmd;
    FRAME   *f;

    (void)arg;
    tick = 0;

    while (1)
    {
        /* Simulated register reads: values stay inside documented ranges
           at runtime, but every read is volatile. */
        g_sensor_raw    = tx_time_get() & 0xFF;
        g_sensor_signed = (LONG)(tx_time_get() & 0x7F);
        g_sensor_id     = tx_time_get() & 0x7;
        g_sensor_len    = tx_time_get() & 0x3F;
        g_sensor_count  = tx_time_get() & 0xF;
        g_hdr_pad       = ((tx_time_get() & 0x1) != 0) ? 16u : 12u;

        /* The fast-path stats update runs concurrently with the
           consumer's locked update. */
        stats_record_fast(g_sensor_raw);

        build_cmd(&cmd, tick);
        cmd_post(&cmd);

        f = frame_build(tick);
        if (f != TX_NULL)
            pipeline_submit(f);

        harness_tick(tick);

        /* Signal the consumer that a cycle of inputs is available. */
        tx_event_flags_set(&harness_events, 0x1, TX_OR);

        tick++;
        tx_thread_sleep(8);
    }
}

/* Consumer: processes command messages routed in from the queue. */
static void consumer_entry(ULONG arg)
{
    CMD_MSG *cmd;
    UINT     status;

    (void)arg;

    while (1)
    {
        lazy_init();
        wait_ready();
        (void)slot_use();

        cmd = TX_NULL;
        status = tx_queue_receive(&harness_cmd_queue, &cmd, TX_WAIT_FOREVER);
        if (status != TX_SUCCESS)
            break;

        if (cmd != TX_NULL)
        {
            harness_dispatch(cmd);
            tx_byte_release(cmd);
        }
    }
}

/* Logger: periodically records log entries and reads the job tail. */
static void logger_entry(ULONG arg)
{
    (void)arg;

    /* Give the maintenance thread a head start so shared lists are
       populated before the first tail query. */
    tx_thread_sleep(32);

    while (1)
    {
        sa_log_write((UINT16)g_sensor_raw);
        sa_log_flush();
        (void)job_tail_value();
        tx_thread_sleep(16);
    }
}

/* Maintenance: exercises memory, concurrency and lifecycle routines. */
static void maint_entry(ULONG arg)
{
    FRAME *f;
    FRAME *b;
    SLOT  *s;
    UINT   i;

    (void)arg;

    while (1)
    {
        block_use();
        interior_test();
        (void)frame_chain_check();
        (void)slot_use();

        for (i = 0; i < 2; i++)
        {
            f = (FRAME *)malloc(sizeof(FRAME));
            if (f != TX_NULL)
            {
                init_hdr(&f->hdr);
                (void)cache_store(f);
            }
        }
        (void)cache_flush();

        b = (FRAME *)malloc((ULONG)PIPELINE_BATCH * (ULONG)sizeof(FRAME));
        if (b != TX_NULL)
        {
            for (i = 0; i < PIPELINE_BATCH; i++)
                init_hdr(&b[i].hdr);
            (void)batch_send(b, PIPELINE_BATCH);
        }

        (void)retry_send(32);
        stats_track(g_sensor_raw);
        lazy_init();
        net_update_a();
        net_update_b();
        mutex_teardown();
        (void)queue_drain_safe();
        (void)pipeline_stop();
        job_append(tx_time_get());

        s = TX_NULL;
        if (slot_allocate(&s) == HR_OK)
        {
            if (s != TX_NULL)
                s->seq = tx_time_get();
            free(s);
        }

        (void)slot_track();

        tx_thread_sleep(20);
    }
}

static void build_cmd(CMD_MSG *cmd, UINT tick)
{
    memset(cmd, 0, sizeof(CMD_MSG));
    cmd->opcode    = (UCHAR)((tick & 0x7) | 0x1);   /* feature bit 0 always on */
    cmd->sensor_id = (UCHAR)(tx_time_get() & 0x3);
    cmd->param     = (UINT16)(tick % 32);
    snprintf(cmd->name, sizeof(cmd->name), "cmd-%u", tick % 1000);
}

/* Copy the command into pooled memory and post it.  The pooled message
   owns the data; the consumer releases it. */
static void cmd_post(CMD_MSG *cmd)
{
    CMD_MSG *m;
    UINT     status;

    m = TX_NULL;
    status = tx_byte_allocate(&harness_byte_pool, (VOID **)&m,
                              (ULONG)sizeof(CMD_MSG), TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return;

    memcpy(m, cmd, sizeof(CMD_MSG));

    status = tx_queue_send(&harness_cmd_queue, &m, TX_NO_WAIT);
    if (status != TX_SUCCESS)
        tx_byte_release(m);
}

static FRAME *frame_build(UINT tick)
{
    FRAME  *f;
    UINT    status;
    UINT16  len;
    UINT    i;

    f = TX_NULL;
    status = tx_byte_allocate(&harness_byte_pool, (VOID **)&f,
                              (ULONG)sizeof(FRAME), TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return TX_NULL;

    init_hdr(&f->hdr);
    f->hdr.seq = tick;

    len = (UINT16)(g_sensor_len & 0xF);   /* 0 .. 15 at runtime */
    for (i = 0; i < len; i++)
        f->payload[i] = (UCHAR)(i + 1);

    f->hdr.payload_len = len;
    return f;
}

/* Producer-side rotation through size-arithmetic and uninitialized-memory
   routines.  Inputs are the volatile simulated registers. */
void harness_tick(UINT tick)
{
    switch (tick & 0x7)
    {
        case 0:
            (void)sa_alloc_copy(g_sensor_count);
            break;
        case 1:
            (void)sa_copy_checked(g_sensor_signed);
            break;
        case 2:
            (void)sa_frame_new();
            break;
        case 3:
            (void)sa_pack((UINT16)g_sensor_len);
            break;
        case 4:
            (void)sa_pool_resize(g_cfg.batch);
            break;
        case 5:
            (void)sa_scan_raw((UINT)g_sensor_len);
            break;
        case 6:
            (void)sa_cfg_snapshot();
            break;
        default:
            (void)sa_hdr_store();
            break;
    }

    switch (tick & 0x3)
    {
        case 0:
            (void)frame_pool_use();
            break;
        case 1:
            (void)frame_send_local();
            break;
        case 2:
            (void)hdr_send((UINT16)g_sensor_len);
            break;
        default:
            (void)crc_check(g_sensor_count);
            break;
    }
}

/* Consumer-side dispatch.  Every value that arrives here through the
   queue is data the analyzers cannot correlate with the producer. */
void harness_dispatch(CMD_MSG *cmd)
{
    HARNESS_CFG *cfgp;
    ULONG        rate;

    sa_store_len(g_sensor_raw);
    sa_masked_copy(g_sensor_raw);
    sa_table_put(cmd->sensor_id & 0x3, (INT)(cmd->param >> 3), tx_time_get());

    g_cfg.batch = (UINT16)(cmd->param & 0x3);

    switch (cmd->opcode & 0x7)
    {
        case 0:
            cal_update(cmd);
            break;
        case 1:
            msg_data_copy(cmd);
            break;
        case 2:
            name_copy(cmd);
            break;
        case 3:
            slot_store(cmd);
            break;
        case 4:
            sums_update(cmd);
            break;
        case 5:
            log_text((const CHAR *)cmd->name);
            break;
        case 6:
            (void)dup_string((const CHAR *)cmd->name);
            break;
        default:
            nibble_dispatch((UCHAR)cmd->param);
            break;
    }

    (void)channel_update(cmd->sensor_id);
    (void)sensor_dispatch(cmd->sensor_id);
    (void)sensor_deep_probe(cmd->sensor_id);

    cfgp = lookup_config(cmd->opcode);
    (void)cfg_rate(cfgp, &rate);

    stats_record(cmd->param);

    /* The notify callback is selected by a runtime feature bit. */
    notify_fire((cmd->opcode & 0x1) ? telemetry_notify : TX_NULL);

    (void)frame_consume(cmd->sensor_id);

    stats_record_fast(cmd->param);
}
