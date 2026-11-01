/*
 * Lecture 3 - Homework Starter Code
 *
 * GOAL: Convert a polling loop to an event-driven workqueue architecture.
 *
 * The starter code works but is INEFFICIENT.
 * polling_thread wakes every 10ms to check a flag.
 * sensor_sim fires every 100ms - that's 10 wasted wake-ups per event.
 *
 *
 * ================================================================
 * TASKS
 * ================================================================
 *
 * TASK 1 (starter - already works, just run it):
 *   Run the starter. Count wake-ups vs real events in the log.
 *   Expected: ~10 wake-ups per sensor event. Confirm this.
 *
 * TASK 2 (implement):
 *   Replace polling_thread with a k_work handler.
 *   sensor_sim should call k_work_submit() instead of setting a flag.
 *   The handler should do what polling_thread currently does.
 *
 *   Steps:
 *   - Define a work item with K_WORK_DEFINE
 *   - Write the handler function
 *   - In sensor_sim: call k_work_submit() (remove k_sem_give + flag)
 *   - Remove the polling_thread entirely
 *
 * TASK 3 (verify):
 *   Add k_uptime_get_32() to your handler's LOG_INF.
 *   Confirm handler runs only when sensor_sim fires (every ~100ms).
 *   No unnecessary wake-ups.
 *
 * BONUS (debounce):
 *   Change sensor_sim to fire 5 events within 20ms (not 1 per 100ms).
 *   Use k_work_reschedule with 30ms delay so only ONE handler
 *   call occurs after the burst - not 5.
 *   Log the reschedule timestamps to confirm the burst collapses.
 *
 * ================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE 1024
#define SENSOR_MS 100  /* sensor fires every 100ms */
#define EVENT_COUNT 10 /* total sensor events to produce */
#define BONUS_TASK 1   /* set to 1 to enable the bonus debounce task */
#define BURST_EVENT_COUNT 5
#define BURST_INTERVAL_MS 4

/* Statistics */
static int total_events;
static int total_processed;

#if BONUS_TASK
static void debounce_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;
    LOG_INF("[DEBOUNCE] processed event %d  tick=%u",
            total_processed, k_uptime_get_32());
}
K_WORK_DELAYABLE_DEFINE(debounce_work, debounce_handler);
#else
static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;
    LOG_INF("[CONSUMER] processed event %d  tick=%u",
            total_processed, k_uptime_get_32());
}
K_WORK_DEFINE(sensor_work, sensor_handler);
#endif
/* ------------------------------------------------------------------ */
/*  sensor_sim - fires EVENT_COUNT events, 100ms apart               */
/* ------------------------------------------------------------------ */

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < (BONUS_TASK ? BURST_EVENT_COUNT : EVENT_COUNT); i++)
    {
#if BONUS_TASK
        if (i > 0)
        {
            k_msleep(BURST_INTERVAL_MS);
        }
#else
        k_msleep(SENSOR_MS);
#endif

        total_events++;
        LOG_INF("[SENSOR] event %d  tick=%u", i, k_uptime_get_32());
#if BONUS_TASK
        int ret = k_work_reschedule(&debounce_work, K_MSEC(30));
        LOG_INF("[SENSOR] rescheduled debounce after event %d  tick=%u",
                i, k_uptime_get_32());
#else
        int ret = k_work_submit(&sensor_work);
#endif
        if (ret < 0)
        {
            LOG_ERR("[SENSOR] work scheduling failed: %d", ret);
        }

        /*
         * BONUS: Replace the single k_msleep(SENSOR_MS) above with
         * a burst of 5 rapid events, then use k_work_reschedule in
         * the handler to collapse them to one execution.
         */
    }

    LOG_INF("[SENSOR] all events produced");
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_sim_fn, NULL, NULL, NULL, 5, 0, 0);

/* ================================================================
 * BONUS PLACEHOLDER - for debounce:
 *
 * K_WORK_DELAYABLE_DEFINE(debounce_work, sensor_handler);
 * In sensor_sim: k_work_reschedule(&debounce_work, K_MSEC(30));
 * ================================================================ */

int main(void)
{
    LOG_INF("=== L3 Homework: Workqueue event processing ===");
#if BONUS_TASK
    LOG_INF("Sensor sends %d events %dms apart; debounce delay is 30ms",
            BURST_EVENT_COUNT, BURST_INTERVAL_MS);
#else
    LOG_INF("Sensor fires every %dms; events are submitted to the workqueue",
            SENSOR_MS);
#endif

    /* Wait long enough for all events to complete */
#if BONUS_TASK
    k_msleep(BURST_EVENT_COUNT * BURST_INTERVAL_MS + 100);
#else
    k_msleep((EVENT_COUNT + 2) * SENSOR_MS + 500);
#endif

    LOG_INF("[SUMMARY] events=%d  processed=%d",
            total_events, total_processed);

    return 0;
}
