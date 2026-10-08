/***************************************************************************/
/* Copyright (c) 2026 zuwasi - Static analysis evaluation harness          */
/*                                                                         */
/* Taint / bounds routines: every value here originates from a received    */
/* command message, so its range is not statically known.                 */
/* SPDX-License-Identifier: MIT                                            */
/***************************************************************************/

#include "harness_common.h"

/* Calibration and slot tables fed from received messages */
static ULONG cal_table[CAL_TABLE_SIZE];
static ULONG slot_values[SLOT_VALUES_SIZE];
static ULONG sums[SUMS_SIZE];

/* Name tables used by the string duplication path.  Names with the high */
/* bit set in their first byte are legacy binary names that are not       */
/* NUL-counted (see dup_string).                                          */
static const CHAR *long_names[4] =
{
    "telemetry-frame-alpha",
    "sensor-sample-beta",
    "calibration-delta",
    "diagnostic-echo-long"
};

/* Trivial dispatch handlers (indexed by protocol type nibble) */
static void h_nop00(void) { }
static void h_nop01(void) { }
static void h_nop02(void) { }
static void h_nop03(void) { }
static void h_nop04(void) { }
static void h_nop05(void) { }
static void h_nop06(void) { }
static void h_nop07(void) { }
static void h_nop08(void) { }
static void h_nop09(void) { }
static void h_nop10(void) { }
static void h_nop11(void) { }

static void (* const handlers[HANDLER_COUNT])(void) =
{
    h_nop00, h_nop01, h_nop02, h_nop03,
    h_nop04, h_nop05, h_nop06, h_nop07,
    h_nop08, h_nop09, h_nop10, h_nop11
};

/* ---------------------------------------------------------------------- */
/* cal_update: add a received parameter into the calibration table.        */
/* ---------------------------------------------------------------------- */

void cal_update(CMD_MSG *m)
{
    UINT idx;

    idx = (UINT)m->sensor_id & (UINT)(CAL_TABLE_SIZE - 1u);
    cal_table[idx] = cal_table[idx] + (ULONG)m->param;
}

/* ---------------------------------------------------------------------- */
/* msg_data_copy: copy the incoming command image into a scratch buffer.   */
/* ---------------------------------------------------------------------- */

void msg_data_copy(CMD_MSG *m)
{
    UCHAR  dst[64];
    UINT16 len;

    len = (UINT16)((ULONG)m->param + 4u);
    if (len <= (UINT16)sizeof(dst))
        memcpy(dst, m, (size_t)len);
}

/* ---------------------------------------------------------------------- */
/* name_copy: copy the received command name into a fixed label buffer.    */
/* ---------------------------------------------------------------------- */

void name_copy(CMD_MSG *m)
{
    CHAR label[16];

    if (strlen((const char *)m->name) < sizeof(label))
        strcpy(label, (const char *)m->name);
}

/* ---------------------------------------------------------------------- */
/* slot_store: store a received parameter into the slot table.             */
/* ---------------------------------------------------------------------- */

void slot_store(CMD_MSG *m)
{
    UINT idx;

    idx = (UINT)m->sensor_id;
    if (idx < (UINT)SLOT_VALUES_SIZE)
        slot_values[idx] = (ULONG)m->param;
}

/* ---------------------------------------------------------------------- */
/* sums_update: advance the per-sensor sum accumulators.                   */
/* ---------------------------------------------------------------------- */

void sums_update(CMD_MSG *m)
{
    UINT n;
    UINT i;

    n = (UINT)m->param >> 2;
    if (n > (UINT)SUMS_SIZE)
        n = (UINT)SUMS_SIZE;

    for (i = 0; i < n; i++)
        sums[i] = sums[i] + 1u;
}

/* ---------------------------------------------------------------------- */
/* log_text: emit a received text message.                                 */
/* ---------------------------------------------------------------------- */

void log_text(const CHAR *text)
{
    if (text == TX_NULL)
        return;

    printf("%s", (const char *)text);
}

/* ---------------------------------------------------------------------- */
/* dup_string: duplicate a received string.  Legacy binary names (high    */
/* bit set in the first byte) are a fixed protocol quirk.                  */
/* ---------------------------------------------------------------------- */

UINT dup_string(const CHAR *s)
{
    CHAR   *p;
    size_t  need;

    if (s == TX_NULL)
        return HR_FAIL;

    need = strlen((const char *)s) + 1u;
    p = (CHAR *)malloc(need);
    if (p == TX_NULL)
        return HR_FAIL;

    strcpy(p, (const char *)s);
    free(p);
    return HR_OK;
}

/* ---------------------------------------------------------------------- */
/* nibble_dispatch: dispatch on the protocol type nibble of a raw byte.    */
/* ---------------------------------------------------------------------- */

UINT nibble_dispatch(UCHAR raw)
{
    UINT type;

    type = ((UINT)raw >> 4) & 0xFu;
    if (type < (UINT)HANDLER_COUNT)
        handlers[type]();

    return HR_OK;
}
