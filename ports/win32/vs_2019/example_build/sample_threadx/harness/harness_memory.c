/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* Memory-lifecycle routines for the telemetry pipeline: queue-based frame*/
/* ownership, pooled allocation, batch pipelines and cleanup helpers.     */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#include "harness_common.h"

/* Two-slot frame cache used by the maintenance thread */
static FRAME *frame_cache[2];
static UINT   cache_next = 0;

/* ---------------------------------------------------------------------- */
/* frame_send: enqueue a frame; ownership passes to the queue on success. */
/* ---------------------------------------------------------------------- */

UINT frame_send(FRAME *f)
{
    UINT status;

    if (f == TX_NULL)
        return HR_FAIL;

    status = tx_queue_send(&harness_frame_queue, &f, TX_NO_WAIT);
    return (status == TX_SUCCESS) ? HR_OK : HR_FAIL;
}

/* ---------------------------------------------------------------------- */
/* pipeline_submit: hand a pooled frame to the pipeline.  If the queue    */
/* cannot take it, the buffer goes back to the pool here.                 */
/* ---------------------------------------------------------------------- */

UINT pipeline_submit(FRAME *f)
{
    if (f == TX_NULL)
        return HR_FAIL;

    if (frame_send(f) != HR_OK)
    {
        /* Could not enqueue: hand the buffer back to the pool. */
        tx_byte_release(f);

        /* [PLANTED DEFECT BUG-013 | CWE-416 | frame flags updated after the
           buffer was handed back to the pool] */
        f->hdr.flags = (UCHAR)(f->hdr.flags & 0xFDu);
        return HR_FAIL;
    }
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* frame_chain_check: allocate a small chain of frames, inspect it, and  */
/* release everything on every path.                                      */
/* ---------------------------------------------------------------------- */

UINT frame_chain_check(void)
{
    FRAME *frames[PIPELINE_BATCH];
    UINT   i;
    UINT   status;
    UINT   released;

    status = HR_OK;
    for (i = 0; i < (UINT)PIPELINE_BATCH; i++)
    {
        frames[i] = TX_NULL;
        status = tx_byte_allocate(&harness_byte_pool, (VOID **)&frames[i],
                                  (ULONG)sizeof(FRAME), TX_NO_WAIT);
        if (status != TX_SUCCESS)
        {
            frames[i] = TX_NULL;
            break;
        }
        init_hdr(&frames[i]->hdr);
    }

    /* Abort the batch when maintenance requests it. */
    for (i = 0; i < (UINT)PIPELINE_BATCH; i++)
    {
        if (frames[i] == TX_NULL)
            break;

        /* [PLANTED DEFECT BUG-015 | CWE-401 | abort request returns without
           releasing the frames allocated so far] */
        if (g_stress_level != 0u)
        {
            return HR_FAIL;
        }
    }

    /* Release everything allocated, on every path. */
    released = 0;
    for (i = 0; i < (UINT)PIPELINE_BATCH; i++)
    {
        if (frames[i] != TX_NULL)
        {
            tx_byte_release(frames[i]);
            frames[i] = TX_NULL;
            released = released + 1u;
        }
    }
    (void)released;
    return status;
}

/* ---------------------------------------------------------------------- */
/* block_use: allocate one fixed block, touch it, return it to its pool.   */
/* ---------------------------------------------------------------------- */

UINT block_use(void)
{
    VOID *b;
    UINT  status;

    b = TX_NULL;
    status = tx_block_allocate(&harness_block_pool, &b, TX_NO_WAIT);
    if (status != TX_SUCCESS)
        return HR_FAIL;

    memset(b, 0, 16);
    if ((g_stress_level & 4u) != 0u)
    {
        /* [PLANTED DEFECT BUG-016 | CWE-762 | block released to the byte pool
           although it was allocated from the block pool] */
        tx_byte_release(b);
    }
    else
    {
        tx_block_release(b);
    }
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* normalize_frame: normalize a received frame.  On an invalid type the   */
/* frame is released here and the caller is told the buffer is gone.      */
/* ---------------------------------------------------------------------- */

UINT normalize_frame(FRAME *f)
{
    if (f == TX_NULL)
        return HR_FAIL;

    if (f->hdr.type > 3u)
    {
        /* [PLANTED DEFECT BUG-017 | CWE-415 | frame is released but success is
           reported, so the caller releases it again] */
        tx_byte_release(f);
        return HR_OK;
    }

    f->hdr.flags = (UCHAR)(f->hdr.flags & 0xFCu);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* batch_send: send a malloc'd contiguous batch of frames.  The batch is  */
/* released (single free) on both success and failure paths.              */
/* ---------------------------------------------------------------------- */

UINT batch_send(FRAME *batch, UINT count)
{
    UINT i;

    if (batch == TX_NULL)
        return HR_FAIL;

    for (i = 0; i < count; i++)
    {
        if (frame_send(&batch[i]) != HR_OK)
        {
            free(batch);          /* release the whole contiguous block */
            /* [PLANTED DEFECT BUG-018 | CWE-416 | the loop keeps sending from
               the freed batch instead of stopping] */
        }
    }

    free(batch);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* pipeline_stop: run a short-lived probe timer and tear it down.         */
/* ---------------------------------------------------------------------- */

static TX_TIMER probe_timer;

static void probe_timer_expiry(ULONG expiry_input)
{
    (void)expiry_input;
}

UINT pipeline_stop(void)
{
    UINT status;

    status = tx_timer_create(&probe_timer, "probe timer", probe_timer_expiry, 0,
                             100, 10, TX_AUTO_ACTIVATE);
    if (status != TX_SUCCESS)
        return HR_FAIL;

    if ((g_stress_level & 8u) != 0u)
    {
        /* [PLANTED DEFECT BUG-019 | CWE-404 | early return leaks the created
           timer object] */
        return HR_OK;
    }

    tx_timer_deactivate(&probe_timer);
    tx_timer_delete(&probe_timer);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* interior_test: work on the body of a larger allocation and release the */
/* allocation itself.                                                     */
/* ---------------------------------------------------------------------- */

UINT interior_test(void)
{
    UCHAR *base;
    UCHAR *body;

    base = (UCHAR *)malloc(80);
    if (base == TX_NULL)
        return HR_FAIL;

    body = base + 16;
    memset(body, 0, 32);

    /* [PLANTED DEFECT BUG-020 | CWE-761 | the interior pointer is released
       instead of the allocation base] */
    if ((g_stress_level & 16u) != 0u)
    {
        free(body);
    }
    else
    {
        free(base);
    }
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* cache_store / cache_flush: two-slot frame cache with distinct entries. */
/* ---------------------------------------------------------------------- */

UINT cache_store(FRAME *f)
{
    if (f == TX_NULL)
        return HR_FAIL;

    if ((g_stress_level & 32u) != 0u)
    {
        /* [PLANTED DEFECT BUG-021 | CWE-415 | both cache slots alias the same
           frame, so the flush releases it twice] */
        frame_cache[0] = f;
        frame_cache[1] = f;
    }
    else
    {
        frame_cache[cache_next] = f;
        cache_next = (cache_next + 1u) & 0x1u;
    }
    return HR_OK;
}

UINT cache_flush(void)
{
    UINT i;

    for (i = 0; i < 2; i++)
    {
        if (frame_cache[i] != TX_NULL)
        {
            free(frame_cache[i]);
            frame_cache[i] = TX_NULL;
        }
    }
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* retry_send: deliver a scratch buffer through the probe queue, retrying*/
/* when the queue is busy.  Every failed attempt is released.             */
/* ---------------------------------------------------------------------- */

UINT retry_send(UINT size)
{
    UCHAR *p;
    VOID   *r;
    UINT    tries;

    tries = 3;
    while (tries > 0)
    {
        p = (UCHAR *)malloc((size_t)size);
        if (p == TX_NULL)
            return HR_FAIL;

        if (tx_queue_send(&harness_probe_queue, &p, TX_NO_WAIT) == TX_SUCCESS)
        {
            r = TX_NULL;
            (void)tx_queue_receive(&harness_probe_queue, &r, TX_NO_WAIT);
            free(p);
            return HR_OK;
        }

        tries = tries - 1u;        /* [PLANTED DEFECT BUG-022 | CWE-401 | a
                                     retry leaks the buffer of the failed
                                     attempt] */
    }
    return HR_FAIL;
}
