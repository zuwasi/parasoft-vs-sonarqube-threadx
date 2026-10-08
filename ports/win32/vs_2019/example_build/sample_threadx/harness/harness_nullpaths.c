/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* NULL / validation-path routines for the telemetry pipeline: channel    */
/* lookup, slot allocation, frame retrieval and configuration access.     */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#include "harness_common.h"

/* Shared job list, owned by the concurrency module */
extern JOB *g_job_head;

/* Channel registry */
static CHANNEL channel_table[CAL_TABLE_SIZE];

/* Live channel-enable mask (simulated hardware: all channels enabled at
   runtime, but volatile, so every read is a fresh value). */
static volatile ULONG g_chan_mask = 0xFFu;

/* ---------------------------------------------------------------------- */
/* lookup_channel: return the registry slot for a channel id, or NULL.    */
/* ---------------------------------------------------------------------- */

CHANNEL *lookup_channel(UINT id)
{
    if (id >= (UINT)CAL_TABLE_SIZE)
        return TX_NULL;

    return &channel_table[id];
}

/* ---------------------------------------------------------------------- */
/* accessor_channel: like lookup_channel, but also honors the live        */
/* channel-enable mask and returns NULL for disabled channels.            */
/* ---------------------------------------------------------------------- */

CHANNEL *accessor_channel(UINT id)
{
    if (id >= (UINT)CAL_TABLE_SIZE)
        return TX_NULL;

    if ((g_chan_mask & (1uL << id)) == 0u)
        return TX_NULL;

    return &channel_table[id];
}

/* ---------------------------------------------------------------------- */
/* lookup_config: map a command opcode to the active configuration, or     */
/* NULL for opcodes outside the implemented range.                        */
/* ---------------------------------------------------------------------- */

HARNESS_CFG *lookup_config(UINT opcode)
{
    if (opcode >= 8u)
        return TX_NULL;

    return &g_cfg;
}

/* ---------------------------------------------------------------------- */
/* channel_update: mirror accessor state into the primary registry slot.  */
/* ---------------------------------------------------------------------- */

UINT channel_update(UINT id)
{
    CHANNEL *primary;
    CHANNEL *mirror;

    primary = lookup_channel(id);
    mirror  = accessor_channel(id);

    /* [PLANTED DEFECT BUG-031 | CWE-476 | the mirror pointer is checked but
       the primary pointer is dereferenced] */
    if (mirror != TX_NULL)
    {
        primary->state = mirror->state;
        return HR_OK;
    }
    return HR_FAIL;
}

/* ---------------------------------------------------------------------- */
/* sensor_dispatch: record a sensor reading against its channel.           */
/* ---------------------------------------------------------------------- */

UINT sensor_dispatch(UINT id)
{
    CHANNEL *ch;

    ch = lookup_channel(id);

    /* [PLANTED DEFECT BUG-032 | CWE-476 | lookup result is used without a
       NULL check] */
    ch->errors = ch->errors + 1u;
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* slot_allocate: heap-allocate a fresh slot descriptor.                   */
/* ---------------------------------------------------------------------- */

UINT slot_allocate(SLOT **out)
{
    SLOT *s;

    if (out == TX_NULL)
        return HR_FAIL;

    s = (SLOT *)malloc(sizeof(SLOT));

    /* [PLANTED DEFECT BUG-033 | CWE-690 | allocation result dereferenced
       without a NULL check] */
    s->seq = 0;
    *out = s;
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* get_frame: receive the next frame (blocking).  On error, *out is left */
/* untouched; the caller must honor the return code.                      */
/* ---------------------------------------------------------------------- */

UINT get_frame(UINT id, FRAME **out)
{
    FRAME *f;
    UINT   status;

    (void)id;
    if (out == TX_NULL)
        return HR_FAIL;

    f = TX_NULL;
    status = tx_queue_receive(&harness_frame_queue, &f, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS)
        return HR_FAIL;

    *out = f;
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* frame_consume: receive and release a frame, honoring the status.       */
/* ---------------------------------------------------------------------- */

UINT frame_consume(UINT id)
{
    FRAME *f;

    /* [PLANTED DEFECT BUG-034 | CWE-457 | the status of get_frame is ignored;
       on error f is never assigned] */
    (void)get_frame(id, &f);
    tx_byte_release(f);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* cfg_rate: read the configured rate through a possibly-NULL config.     */
/* ---------------------------------------------------------------------- */

UINT cfg_rate(HARNESS_CFG *cfg, ULONG *out)
{
    ULONG rate;

    /* [PLANTED DEFECT BUG-035 | CWE-476 | configuration read before the NULL
       check] */
    rate = cfg->rate;

    if (cfg == TX_NULL)
        return HR_FAIL;

    if (out != TX_NULL)
        *out = rate;
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* notify_fire: invoke the optional telemetry notify callback.             */
/* ---------------------------------------------------------------------- */

void notify_fire(void (*cb)(ULONG))
{
    /* [PLANTED DEFECT BUG-036 | CWE-690 | optional callback invoked without a
       NULL check] */
    cb(1u);
}

/* ---------------------------------------------------------------------- */
/* job_tail_value: read the value of the oldest job under the table lock. */
/* ---------------------------------------------------------------------- */

ULONG job_tail_value(void)
{
    JOB   *n;
    ULONG  v;

    tx_mutex_get(&table_mutex, TX_WAIT_FOREVER);

    /* [PLANTED DEFECT BUG-037 | CWE-476 | the head check guards the loop but
       n is never assigned when the list is empty] */
    if (g_job_head != TX_NULL)
    {
        n = g_job_head;
        while (n->next != TX_NULL)
            n = n->next;
        v = n->value;
    }

    tx_mutex_put(&table_mutex);
    return v;
}

/* ---------------------------------------------------------------------- */
/* sensor_deep_probe: four-hop inspection path ending in accessor_channel.*/
/* ---------------------------------------------------------------------- */

static UINT probe_level2(UINT id)
{
    CHANNEL *c;

    c = accessor_channel(id);

    /* [PLANTED DEFECT BUG-038 | CWE-476 | deepest accessor result used without
       a NULL check across a four-hop call chain] */
    c->errors = c->errors + 1u;
    return HR_OK;
}

static UINT probe_level1(UINT id)
{
    return probe_level2(id);
}

UINT sensor_deep_probe(UINT id)
{
    return probe_level1(id);
}
