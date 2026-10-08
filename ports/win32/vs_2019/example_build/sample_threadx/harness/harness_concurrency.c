/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* Concurrency routines for the telemetry pipeline: shared statistics,   */
/* slot hand-off, ordered lock acquisition, job list and queue draining.  */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#include "harness_common.h"

/* Shared job list (also read by the NULL-path family) */
JOB *g_job_head = TX_NULL;

/* Slot hand-off area and readiness state */
static SLOT            slot_bank[2];
static SLOT * volatile g_slot = TX_NULL;
static volatile ULONG  g_ready = 0;
static volatile ULONG  g_inited = 0;

/* ---------------------------------------------------------------------- */
/* stats_record: locked statistics update.                                 */
/* ---------------------------------------------------------------------- */

void stats_record(ULONG v)
{
    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    g_stats.total = g_stats.total + v;
    tx_mutex_put(&stats_mutex);
}

/* ---------------------------------------------------------------------- */
/* stats_record_fast: hot-path statistics update; also locked.            */
/* ---------------------------------------------------------------------- */

void stats_record_fast(ULONG v)
{
    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    g_stats.total = g_stats.total + v;
    tx_mutex_put(&stats_mutex);
}

/* ---------------------------------------------------------------------- */
/* slot_use: publish the current slot under the table lock.               */
/* ---------------------------------------------------------------------- */

UINT slot_use(void)
{
    UINT status;

    status = tx_mutex_get(&table_mutex, TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return HR_FAIL;

    g_slot = &slot_bank[tx_time_get() & 0x1u];
    if (g_slot != TX_NULL)
    {
        g_slot->seq = tx_time_get();
        g_slot->len = 4;
    }

    tx_mutex_put(&table_mutex);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* net_update_a / net_update_b: both take the locks in the same order.     */
/* ---------------------------------------------------------------------- */

void net_update_a(void)
{
    tx_mutex_get(&net_mutex_1, TX_WAIT_FOREVER);
    tx_mutex_get(&net_mutex_2, TX_WAIT_FOREVER);

    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    g_stats.peak = g_stats.peak + 1u;
    tx_mutex_put(&stats_mutex);

    tx_mutex_put(&net_mutex_2);
    tx_mutex_put(&net_mutex_1);
}

void net_update_b(void)
{
    /* Same lock order as net_update_a. */
    tx_mutex_get(&net_mutex_1, TX_WAIT_FOREVER);
    tx_mutex_get(&net_mutex_2, TX_WAIT_FOREVER);

    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    g_stats.errors = g_stats.errors + 1u;
    tx_mutex_put(&stats_mutex);

    tx_mutex_put(&net_mutex_2);
    tx_mutex_put(&net_mutex_1);
}

/* ---------------------------------------------------------------------- */
/* wait_ready: wait until the producer signals a cycle of inputs.         */
/* ---------------------------------------------------------------------- */

void wait_ready(void)
{
    ULONG flags;

    (void)tx_event_flags_get(&harness_events, 0x1u, TX_OR, &flags, TX_WAIT_FOREVER);
}

/* ---------------------------------------------------------------------- */
/* job_append: prepend a job to the shared list under the table lock.     */
/* ---------------------------------------------------------------------- */

void job_append(ULONG v)
{
    JOB *j;

    j = (JOB *)malloc(sizeof(JOB));
    if (j == TX_NULL)
        return;

    j->value = v;

    tx_mutex_get(&table_mutex, TX_WAIT_FOREVER);
    j->next = g_job_head;
    g_job_head = j;
    tx_mutex_put(&table_mutex);
}

/* ---------------------------------------------------------------------- */
/* mutex_teardown: only the owner of a mutex may put it back.             */
/* ---------------------------------------------------------------------- */

void mutex_teardown(void)
{
    UINT status;

    status = tx_mutex_get(&net_mutex_1, TX_NO_WAIT);
    if (status == TX_SUCCESS)
    {
        tx_mutex_put(&net_mutex_1);
    }
}

/* ---------------------------------------------------------------------- */
/* lazy_init: one-time state initialization, guarded by a lock.           */
/* ---------------------------------------------------------------------- */

void lazy_init(void)
{
    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);

    if (g_inited == 0u)
    {
        g_ready = 1u;
        g_slot  = &slot_bank[0];
        g_inited = 1u;
    }

    tx_mutex_put(&stats_mutex);
}

/* ---------------------------------------------------------------------- */
/* queue_drain_safe: drain pending frames.  normalize_frame releases the  */
/* buffer when it rejects a frame, so it is only released here when it    */
/* accepts one.                                                            */
/* ---------------------------------------------------------------------- */

UINT queue_drain_safe(void)
{
    FRAME *f;
    UINT   status;

    while (1)
    {
        f = TX_NULL;
        status = tx_queue_receive(&harness_frame_queue, &f, TX_NO_WAIT);
        if (status != TX_SUCCESS)
            break;

        if (normalize_frame(f) == HR_OK)
        {
            tx_byte_release(f);
        }
        /* else: normalize_frame already released the frame */
    }
    return HR_OK;
}
