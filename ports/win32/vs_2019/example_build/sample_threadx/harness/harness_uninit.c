/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* Uninitialized-memory routines: pooled memory, partial initialization, */
/* out-parameters on error paths and partially-filled arrays.             */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#include "harness_common.h"

/* ---------------------------------------------------------------------- */
/* init_hdr: fully initialize a frame header.                              */
/* ---------------------------------------------------------------------- */

void init_hdr(FRAME_HDR *h)
{
    /* [PLANTED DEFECT BUG-049 | CWE-457 | flags and reserved are never
       initialized by this helper; callers read them] */
    h->magic       = FRAME_HDR_MAGIC;
    h->type        = 0;
    h->seq         = 0;
    h->payload_len = 0;
}

/* ---------------------------------------------------------------------- */
/* frame_pool_use: allocate a frame from the byte pool and read flags.     */
/* Pool memory is explicitly cleared before use.                          */
/* ---------------------------------------------------------------------- */

UINT frame_pool_use(void)
{
    FRAME *f;
    UINT   status;

    f = TX_NULL;
    status = tx_byte_allocate(&harness_byte_pool, (VOID **)&f,
                              (ULONG)sizeof(FRAME), TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return HR_FAIL;

    /* [PLANTED DEFECT BUG-047 | CWE-457 | pooled memory read before any
       initialization: hdr.flags is never set] */
    f->hdr.magic = FRAME_HDR_MAGIC;
    f->hdr.seq = 1;

    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    if ((f->hdr.flags & 0x1u) != 0u)
        g_stats.errors = g_stats.errors + 1u;
    tx_mutex_put(&stats_mutex);

    tx_byte_release(f);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* frame_send_local: build a header on the stack and inspect its flags.    */
/* ---------------------------------------------------------------------- */

UINT frame_send_local(void)
{
    FRAME_HDR h;

    /* [PLANTED DEFECT BUG-048 | CWE-457 | header partially initialized: seq
       and magic only; flags is read uninitialized] */
    h.magic = FRAME_HDR_MAGIC;
    h.seq = tx_time_get();

    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    if ((h.flags & 0x2u) != 0u)
        g_stats.peak = g_stats.peak + 1u;
    tx_mutex_put(&stats_mutex);

    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* hdr_send: prepare a header for transmission and inspect its flags.      */
/* ---------------------------------------------------------------------- */

UINT hdr_send(UINT16 len)
{
    FRAME_HDR h;

    init_hdr(&h);
    h.payload_len = len;

    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    if ((h.flags & 0x4u) != 0u)
        g_stats.total = g_stats.total + 1u;
    tx_mutex_put(&stats_mutex);

    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* get_slot: heap-allocate and fully initialize a slot descriptor.         */
/* On allocation failure *out is untouched; the caller must honor the     */
/* return code.                                                            */
/* ---------------------------------------------------------------------- */

UINT get_slot(SLOT **out)
{
    SLOT *s;

    if (out == TX_NULL)
        return HR_FAIL;

    s = (SLOT *)malloc(sizeof(SLOT));
    if (s == TX_NULL)
        return HR_FAIL;

    s->seq = tx_time_get();
    s->len = 4;
    memset(s->data, 0, sizeof(s->data));
    *out = s;
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* slot_track: allocate, touch and release a slot, honoring the status.    */
/* ---------------------------------------------------------------------- */

UINT slot_track(void)
{
    SLOT *u;

    /* [PLANTED DEFECT BUG-050 | CWE-457 | the error contract is ignored: on
       failure get_slot leaves u untouched] */
    (void)get_slot(&u);
    free(u);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* crc_check: fill the CRC scratch array and compare the last entry.       */
/* ---------------------------------------------------------------------- */

UINT crc_check(UINT expected)
{
    ULONG crcs[4];
    UINT  i;

    /* [PLANTED DEFECT BUG-051 | CWE-457 | the fill loop stops one short, so
       crcs[3] is compared uninitialized] */
    for (i = 0; i < 3; i++)
        crcs[i] = (ULONG)i * 7u + 1u;

    if (crcs[3] == (ULONG)expected)
        return HR_OK;

    return HR_FAIL;
}

/* ---------------------------------------------------------------------- */
/* stats_track: track a sample through a temporary stats snapshot.         */
/* The snapshot is zeroed before any field is read.                        */
/* ---------------------------------------------------------------------- */

UINT stats_track(ULONG v)
{
    STATS *st;

    st = (STATS *)malloc(sizeof(STATS));
    if (st == TX_NULL)
        return HR_FAIL;

    /* [PLANTED DEFECT BUG-052 | CWE-457 | snapshot fields read before the
       struct is zeroed: peak is uninitialized] */
    st->total = v;

    if (st->peak > 100u)
        st->errors = st->errors + 1u;

    free(st);
    return HR_OK;
}
