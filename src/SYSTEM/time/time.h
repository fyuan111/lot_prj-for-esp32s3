#pragma once

#include <stdatomic.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint32_t hour : 5;     /* 0..23 */
    uint32_t minute : 6;   /* 0..59 */
    uint32_t second : 6;   /* 0..59 */
    uint32_t msec : 10;    /* 0..999 */
    uint32_t reserved : 5;
} lot_time_t;

_Static_assert(sizeof(lot_time_t) == 4, "lot_time_t must be 4 bytes");

typedef _Atomic(uint32_t) lot_time_atomic_t;

#define LOT_DEFAULT_TIME_STR "12:00:00.000"
#define LOT_DEFAULT_TIME_HMS ((lot_time_t){ .hour = 12, .minute = 0, .second = 0, .msec = 0, .reserved = 0 })

#define LOT_TIME_HOUR_MAX 23U
#define LOT_TIME_MINUTE_MAX 59U
#define LOT_TIME_SECOND_MAX 59U
#define LOT_TIME_MSEC_MAX 999U

#define LOT_TIME_HOUR_SHIFT 0U
#define LOT_TIME_MINUTE_SHIFT 5U
#define LOT_TIME_SECOND_SHIFT 11U
#define LOT_TIME_MSEC_SHIFT 17U

#define LOT_TIME_HOUR_MASK 0x1FU
#define LOT_TIME_MINUTE_MASK 0x3FU
#define LOT_TIME_SECOND_MASK 0x3FU
#define LOT_TIME_MSEC_MASK 0x3FFU

#define LOT_DEFAULT_TIME_RAW \
    ((12U << LOT_TIME_HOUR_SHIFT) | \
     (0U << LOT_TIME_MINUTE_SHIFT) | \
     (0U << LOT_TIME_SECOND_SHIFT) | \
     (0U << LOT_TIME_MSEC_SHIFT))

#define LOT_TIME_ATOMIC_INIT ATOMIC_VAR_INIT(LOT_DEFAULT_TIME_RAW)

/* Global atomic time variable */
extern lot_time_atomic_t g_lot_time;

esp_err_t lot_time_init(void);
lot_time_t lot_time(void);

static inline uint32_t lot_time_pack(lot_time_t t)
{
    uint32_t hour = (t.hour > LOT_TIME_HOUR_MAX) ? LOT_TIME_HOUR_MAX : t.hour;
    uint32_t minute = (t.minute > LOT_TIME_MINUTE_MAX) ? LOT_TIME_MINUTE_MAX : t.minute;
    uint32_t second = (t.second > LOT_TIME_SECOND_MAX) ? LOT_TIME_SECOND_MAX : t.second;
    uint32_t msec = (t.msec > LOT_TIME_MSEC_MAX) ? LOT_TIME_MSEC_MAX : t.msec;

    return ((hour & LOT_TIME_HOUR_MASK) << LOT_TIME_HOUR_SHIFT) |
           ((minute & LOT_TIME_MINUTE_MASK) << LOT_TIME_MINUTE_SHIFT) |
           ((second & LOT_TIME_SECOND_MASK) << LOT_TIME_SECOND_SHIFT) |
           ((msec & LOT_TIME_MSEC_MASK) << LOT_TIME_MSEC_SHIFT);
}

static inline lot_time_t lot_time_unpack(uint32_t raw)
{
    lot_time_t t = {
        .hour = (raw >> LOT_TIME_HOUR_SHIFT) & LOT_TIME_HOUR_MASK,
        .minute = (raw >> LOT_TIME_MINUTE_SHIFT) & LOT_TIME_MINUTE_MASK,
        .second = (raw >> LOT_TIME_SECOND_SHIFT) & LOT_TIME_SECOND_MASK,
        .msec = (raw >> LOT_TIME_MSEC_SHIFT) & LOT_TIME_MSEC_MASK,
        .reserved = 0,
    };
    return t;
}

static inline lot_time_t lot_time_atomic_load(const lot_time_atomic_t *atom)
{
    return lot_time_unpack(atomic_load_explicit(atom, memory_order_relaxed));
}

static inline void lot_time_atomic_store(lot_time_atomic_t *atom, lot_time_t t)
{
    atomic_store_explicit(atom, lot_time_pack(t), memory_order_relaxed);
}

static inline uint32_t lot_time_get_msec(lot_time_t t)
{
    return t.msec;
}

static inline void lot_time_set_msec(lot_time_t *t, uint32_t msec)
{
    if (msec > LOT_TIME_MSEC_MAX) {
        msec = LOT_TIME_MSEC_MAX;
    }
    t->msec = msec;
}
