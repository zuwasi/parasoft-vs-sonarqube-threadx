/*
 * Static-analysis model smoke test, not part of the planted-defect benchmark.
 * Never execute this fixture. Its unprotected counter is intentionally unsafe.
 */
#include "tx_api.h"

static TX_THREAD first_thread;
static TX_THREAD second_thread;
static TX_MUTEX counter_mutex;
static UCHAR first_stack[2048];
static UCHAR second_stack[2048];
static ULONG unprotected_counter;
static ULONG protected_counter;

static VOID first_entry(ULONG input)
{
    (void)input;
    ++unprotected_counter; /* Expected: shared access without a lock. */
    if (tx_mutex_get(&counter_mutex, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
        ++protected_counter; /* Must not be reported as unprotected. */
        tx_thread_sleep(1);  /* Expected: blocking inside a critical section. */
        tx_mutex_put(&counter_mutex);
    }
}

static VOID second_entry(ULONG input)
{
    (void)input;
    ++unprotected_counter; /* Expected: shared access without a lock. */
    if (tx_mutex_get(&counter_mutex, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
        ++protected_counter; /* Must not be reported as unprotected. */
        tx_mutex_put(&counter_mutex);
    }
}

VOID tx_application_define(VOID *unused_memory)
{
    (void)unused_memory;
    tx_mutex_create(&counter_mutex, "counter", TX_NO_INHERIT);
    tx_thread_create(&first_thread, "first", first_entry, 0,
                     first_stack, sizeof(first_stack), 5, 5,
                     TX_NO_TIME_SLICE, TX_AUTO_START);
    tx_thread_create(&second_thread, "second", second_entry, 0,
                     second_stack, sizeof(second_stack), 5, 5,
                     TX_NO_TIME_SLICE, TX_AUTO_START);
}
