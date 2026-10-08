/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* Size-arithmetic routines for the telemetry pipeline: allocation sizing,*/
/* length arithmetic, ring-buffer indexing and serialized header storage. */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#include "harness_common.h"

/* Log ring (owned by this module) */
static LOG_ENTRY log_ring[LOG_DEPTH];

/* Shared scratch pattern used as a copy source */
static UCHAR sa_src[128];

/* ---------------------------------------------------------------------- */
/* sa_alloc_copy: allocate count*4 bytes and copy a pattern into them.     */
/* ---------------------------------------------------------------------- */

UINT sa_alloc_copy(UINT count)
{
    UINT16 size;
    UCHAR *p;

    /* count*4 must fit in the UINT16 size word and inside sa_src. */
    if (count > 31u)
        return HR_FAIL;

    size = (UINT16)(count * 4u);
    p = (UCHAR *)malloc((size_t)size);
    if (p == TX_NULL)
        return HR_FAIL;

    memcpy(p, sa_src, (size_t)size);
    free(p);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_copy_checked: copy n bytes after validating the length.             */
/* ---------------------------------------------------------------------- */

UINT sa_copy_checked(INT n)
{
    UCHAR dst[64];

    if ((n < 0) || (n > 63))
        return HR_FAIL;

    memcpy(dst, sa_src, (size_t)n);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_log_write: record a log entry at a derived ring index.              */
/* ---------------------------------------------------------------------- */

void sa_log_write(UINT16 code)
{
    UINT idx;

    idx = (UINT)code >> 3;
    if (idx < (UINT)LOG_DEPTH)
    {
        log_ring[idx].code  = code;
        log_ring[idx].seq   = (UINT16)(code ^ 0x5Au);
        log_ring[idx].stamp = tx_time_get();
    }
}

/* ---------------------------------------------------------------------- */
/* sa_log_flush: count recorded entries under the stats lock.             */
/* ---------------------------------------------------------------------- */

void sa_log_flush(void)
{
    UINT  i;
    ULONG used;

    used = 0;
    for (i = 0; i < (UINT)LOG_DEPTH; i++)
    {
        if (log_ring[i].stamp != 0u)
            used = used + 1u;
    }

    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    g_stats.peak = used;
    tx_mutex_put(&stats_mutex);
}

/* ---------------------------------------------------------------------- */
/* sa_frame_new: heap-allocate a copy of a locally built frame.           */
/* ---------------------------------------------------------------------- */

UINT sa_frame_new(void)
{
    FRAME  stack_frame;
    FRAME *p;

    init_hdr(&stack_frame.hdr);
    stack_frame.hdr.seq = tx_time_get();

    p = (FRAME *)malloc(sizeof(FRAME));
    if (p == TX_NULL)
        return HR_FAIL;

    memcpy(p, &stack_frame, sizeof(FRAME));

    tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
    if ((p->hdr.seq & 0x1u) != 0u)
        g_stats.errors = g_stats.errors + 1u;
    tx_mutex_put(&stats_mutex);

    free(p);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_pack: serialize payload_len bytes after a 12-byte header.           */
/* ---------------------------------------------------------------------- */

UINT sa_pack(UINT16 payload_len)
{
    ULONG total;
    UCHAR *p;

    /* Guard: total = payload + header(12) + crc(4) must not wrap. */
    if (payload_len > (UINT16)(FRAME_PAYLOAD_CAP - 16u))
        return HR_FAIL;

    total = (ULONG)payload_len + 12u + 4u;
    p = (UCHAR *)malloc((size_t)total);
    if (p == TX_NULL)
        return HR_FAIL;

    memcpy(p + 12, sa_src, (size_t)payload_len);
    free(p);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_store_len: buffer msg_len bytes with a consistent stored length.    */
/* ---------------------------------------------------------------------- */

UINT sa_store_len(ULONG msg_len)
{
    UINT16 stored;
    UCHAR *p;

    /* The UINT16 stored length must represent msg_len exactly. */
    if (msg_len > 0xFFFFu)
        return HR_FAIL;

    stored = (UINT16)msg_len;
    p = (UCHAR *)malloc((size_t)stored + 1u);
    if (p == TX_NULL)
        return HR_FAIL;

    memcpy(p, sa_src, (size_t)msg_len);
    free(p);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_table_put: bounded 2D table write.                                   */
/* ---------------------------------------------------------------------- */

void sa_table_put(UINT row, INT col, ULONG value)
{
    static ULONG sa_table[SA_ROWS][SA_COLS];
    UINT r;
    UINT c;

    if ((row >= (UINT)SA_ROWS) || (col < 0) || (col >= (INT)SA_COLS))
        return;

    r = row;
    c = (UINT)col;
    sa_table[r][c] = value;
}

/* ---------------------------------------------------------------------- */
/* sa_cfg_snapshot: copy the active configuration and inspect the rate.    */
/* ---------------------------------------------------------------------- */

UINT sa_cfg_snapshot(void)
{
    HARNESS_CFG  local;
    HARNESS_CFG *cfg;

    cfg = &g_cfg;
    memcpy(&local, cfg, sizeof(local));

    if (local.rate > 1000u)
    {
        tx_mutex_get(&stats_mutex, TX_WAIT_FOREVER);
        g_stats.errors = g_stats.errors + 1u;
        tx_mutex_put(&stats_mutex);
    }
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_pool_resize: provision a scratch pool sized from live parameters.    */
/* ---------------------------------------------------------------------- */

UINT sa_pool_resize(UINT nblocks)
{
    static UCHAR        probe_area[8192];
    static TX_BYTE_POOL probe_pool;
    ULONG bytes;
    VOID  *b;
    UINT   status;

    bytes = (ULONG)nblocks * g_sensor_count * 72u;
    if (bytes > (ULONG)sizeof(probe_area))
        bytes = (ULONG)sizeof(probe_area);
    if (bytes == 0u)
        bytes = 64u;

    status = tx_byte_pool_create(&probe_pool, "sa probe pool", probe_area, bytes);
    if (status != TX_SUCCESS)
        return HR_FAIL;

    b = TX_NULL;
    status = tx_byte_allocate(&probe_pool, &b, 64u, TX_NO_WAIT);
    if (status != TX_SUCCESS)
    {
        tx_byte_pool_delete(&probe_pool);
        return HR_FAIL;
    }

    memset(b, 0, 16);
    tx_byte_release(b);
    tx_byte_pool_delete(&probe_pool);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_scan_raw: fill the raw sample buffer and measure its string length. */
/* ---------------------------------------------------------------------- */

UINT sa_scan_raw(UINT fill_len)
{
    UCHAR raw[64];
    UINT  n;
    UINT  i;

    /* Keep at least one byte free for the string terminator. */
    if (fill_len >= (UINT)sizeof(raw))
        fill_len = (UINT)sizeof(raw) - 1u;

    for (i = 0; i < fill_len; i++)
        raw[i] = (UCHAR)('a' + (i & 0xFu));
    raw[fill_len] = 0;

    n = (UINT)strlen((const char *)raw);
    if (n >= (UINT)sizeof(raw))
        return HR_FAIL;

    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_masked_copy: copy msg_len bytes after a full-width length check.     */
/* ---------------------------------------------------------------------- */

UINT sa_masked_copy(ULONG msg_len)
{
    UCHAR dst[64];

    if (msg_len > 63u)
        return HR_FAIL;

    memcpy(dst, sa_src, (size_t)msg_len);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* sa_hdr_store: store a serialized frame header in a configured buffer.  */
/* ---------------------------------------------------------------------- */

UINT sa_hdr_store(void)
{
    FRAME_HDR h;
    UCHAR    *p;
    UINT      size;

    init_hdr(&h);

    /* The pad configuration must cover the serialized header. */
    size = (UINT)g_hdr_pad;
    if ((size < (UINT)sizeof(FRAME_HDR)) || (size > 64u))
        return HR_FAIL;

    p = (UCHAR *)malloc((size_t)size);
    if (p == TX_NULL)
        return HR_FAIL;

    memcpy(p, &h, sizeof(FRAME_HDR));
    free(p);
    return HR_OK;
}
