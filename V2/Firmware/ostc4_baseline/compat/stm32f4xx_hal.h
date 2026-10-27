/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OSTC4_PC_HAL_TYPES_H
#define OSTC4_PC_HAL_TYPES_H
#include <stdint.h>
/* Declaration-only host compatibility: no HAL functions or peripheral access. */
typedef struct { uint8_t Hours, Minutes, Seconds, TimeFormat; uint32_t SubSeconds, SecondFraction, DayLightSaving, StoreOperation; } RTC_TimeTypeDef;
typedef struct { uint8_t WeekDay, Month, Date, Year; } RTC_DateTypeDef;
#endif
