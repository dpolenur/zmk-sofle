/*
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Based on the nice!view peripheral screen, with lock indicators instead of art.
 */

#include <zephyr/kernel.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>
#include "sofle_lock_status.h"
#include "util.h"

static lv_obj_t *top;
static lv_obj_t *caps;
static lv_obj_t *num;
static lv_color_t top_buffer[CANVAS_SIZE * CANVAS_SIZE];
static lv_color_t caps_buffer[CANVAS_SIZE * CANVAS_SIZE];
static lv_color_t num_buffer[CANVAS_SIZE * CANVAS_SIZE];

struct screen_state {
    struct status_state peripheral;
    struct sofle_lock_status locks;
};

static void draw_lock(lv_obj_t *canvas, lv_color_t buffer[], const char *label,
                      bool active, bool valid) {
    lv_draw_rect_dsc_t background;
    init_rect_dsc(&background, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &background);

    lv_draw_rect_dsc_t box;
    init_rect_dsc(&box, active && valid ? LVGL_FOREGROUND : LVGL_BACKGROUND);
    box.border_width = 1;
    box.border_color = LVGL_FOREGROUND;
    lv_canvas_draw_rect(canvas, 2, 5, 64, 53, &box);

    lv_draw_label_dsc_t text;
    init_label_dsc(&text, active && valid ? LVGL_BACKGROUND : LVGL_FOREGROUND,
                   &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 2, 11, 64, &text, label);
    lv_canvas_draw_text(canvas, 2, 34, 64, &text, valid ? (active ? "ON" : "OFF") : "--");
    rotate_canvas(canvas, buffer);
}

static void update_screen(struct screen_state state) {
    lv_draw_rect_dsc_t background;
    init_rect_dsc(&background, LVGL_BACKGROUND);
    lv_canvas_draw_rect(top, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &background);
    draw_battery(top, &state.peripheral);

    lv_draw_label_dsc_t text;
    init_label_dsc(&text, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_canvas_draw_text(top, 0, 0, CANVAS_SIZE, &text,
                        state.peripheral.connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    rotate_canvas(top, top_buffer);

    /* HID LED usages are one-based bit positions in the host's output report. */
    bool valid = state.peripheral.connected && state.locks.valid;
    draw_lock(caps, caps_buffer, "CAPS",
              state.locks.indicators & BIT(HID_USAGE_LED_CAPS_LOCK - 1), valid);
    draw_lock(num, num_buffer, "NUM",
              state.locks.indicators & BIT(HID_USAGE_LED_NUM_LOCK - 1), valid);
}

static struct screen_state get_screen_state(const zmk_event_t *eh) {
    struct screen_state state = {
        .peripheral = {
            .battery = zmk_battery_state_of_charge(),
            .connected = zmk_split_bt_peripheral_is_connected(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
            .charging = zmk_usb_is_powered(),
#endif
        },
        .locks = sofle_lock_status_get(),
    };
    /* Use the event itself so listener registration order cannot cause stale UI. */
    const struct zmk_hid_indicators_changed *indicators =
        eh ? as_zmk_hid_indicators_changed(eh) : NULL;
    if (indicators) {
        state.locks = (struct sofle_lock_status){indicators->indicators, true};
    }
    if (eh && as_zmk_split_peripheral_status_changed(eh)) {
        state.locks.valid = false;
    }
    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(sofle_right_screen, struct screen_state, update_screen, get_screen_state)
ZMK_SUBSCRIPTION(sofle_right_screen, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(sofle_right_screen, zmk_split_peripheral_status_changed);
ZMK_SUBSCRIPTION(sofle_right_screen, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(sofle_right_screen, zmk_usb_conn_state_changed);
#endif

static lv_obj_t *create_canvas(lv_obj_t *parent, lv_color_t buffer[], lv_align_t align, int x) {
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_obj_align(canvas, align, x, 0);
    lv_canvas_set_buffer(canvas, buffer, CANVAS_SIZE, CANVAS_SIZE, LV_IMG_CF_TRUE_COLOR);
    return canvas;
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *widget = lv_obj_create(screen);
    lv_obj_set_size(widget, 160, 68);
    lv_obj_align(widget, LV_ALIGN_TOP_LEFT, 0, 0);
    /* The panel is 160x68, mounted upright. The top canvas uses only its last
     * 20 columns after rotation; the two 68x68 lock panels fill the space below. */
    top = create_canvas(widget, top_buffer, LV_ALIGN_TOP_RIGHT, 0);
    caps = create_canvas(widget, caps_buffer, LV_ALIGN_TOP_LEFT, 68);
    num = create_canvas(widget, num_buffer, LV_ALIGN_TOP_LEFT, 0);
    sofle_right_screen_init();
    return screen;
}
