/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* The peripheral has no host connection; use the state received from the central. */
struct sofle_lock_status {
    uint8_t indicators;
    bool valid;
};

struct sofle_lock_status sofle_lock_status_get(void);
