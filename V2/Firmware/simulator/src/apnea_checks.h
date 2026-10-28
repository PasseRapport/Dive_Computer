#pragma once
#include <stdint.h>
#include "watch_data.h"
/* Deterministic native interaction fixtures, using public crown/depth inputs. */
bool apnea_checks_inputs(uint64_t *sample_ms);
void apnea_checks_observe(const struct watch_data *data);
