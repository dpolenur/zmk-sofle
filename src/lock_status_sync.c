/* SPDX-License-Identifier: MIT */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include "sofle_lock_status.h"

#define LOCK_REQUEST "slk"
BUILD_ASSERT(sizeof(LOCK_REQUEST) <= CONFIG_ZMK_SPLIT_RELAY_EVENT_TYPE_NAME_LEN,
             "Lock request identifier must fit the split relay field");

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <zmk/hid_indicators.h>
#include <zmk/split/central.h>

/* Core ZMK forwards changes. Also answer requests after the right half reconnects,
 * when the host may not send another LED report until a lock key is pressed. */
static int lock_request_listener(const zmk_event_t *eh) {
    const struct zmk_relay_event_received *ev = as_zmk_relay_event_received(eh);
    if (ev && strcmp(ev->event_name, LOCK_REQUEST) == 0 && ev->event_data_size == 0) {
        zmk_split_central_update_hid_indicator(zmk_hid_indicators_get_current_profile());
        return ZMK_EV_EVENT_HANDLED;
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(sofle_lock_request, lock_request_listener);
ZMK_SUBSCRIPTION(sofle_lock_request, zmk_relay_event_received);

#else

#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/split/peripheral.h>

#define LOCK_STATE_VALID BIT(8)
static atomic_t lock_state;

struct sofle_lock_status sofle_lock_status_get(void) {
    atomic_val_t state = atomic_get(&lock_state);
    return (struct sofle_lock_status){
        .indicators = state & 0xff,
        .valid = (state & LOCK_STATE_VALID) != 0,
    };
}

static void request_lock_status(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(lock_request_work, request_lock_status);

static void request_lock_status(struct k_work *work) {
    if (!zmk_split_bt_peripheral_is_connected() || sofle_lock_status_get().valid) {
        return;
    }
    struct zmk_split_transport_peripheral_event request = {
        .type = ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_RELAY_EVENT,
        .data.relay_event = {
            .header = {.event_data_size = 0},
            .event_type = LOCK_REQUEST,
        },
    };
    zmk_split_peripheral_report_event(&request);
    /* Retry until discovery is complete and the central's reply arrives. Stop
     * after synchronization; ordinary lock changes use ZMK's native transport. */
    k_work_reschedule(&lock_request_work, K_SECONDS(1));
}

static int lock_status_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *indicators = as_zmk_hid_indicators_changed(eh);
    if (indicators) {
        if (zmk_split_bt_peripheral_is_connected()) {
            atomic_set(&lock_state, LOCK_STATE_VALID | indicators->indicators);
            k_work_cancel_delayable(&lock_request_work);
        }
    } else {
        const struct zmk_split_peripheral_status_changed *connection =
            as_zmk_split_peripheral_status_changed(eh);
        if (connection) {
            atomic_clear(&lock_state);
            if (connection->connected) {
                k_work_reschedule(&lock_request_work, K_MSEC(500));
            } else {
                k_work_cancel_delayable(&lock_request_work);
            }
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(sofle_lock_status, lock_status_listener);
ZMK_SUBSCRIPTION(sofle_lock_status, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(sofle_lock_status, zmk_split_peripheral_status_changed);

#endif
