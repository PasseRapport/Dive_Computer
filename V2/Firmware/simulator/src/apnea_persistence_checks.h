#pragma once
#include "watch_data.h"
bool apnea_persistence_checks_inputs(uint64_t *sample_ms);
void apnea_persistence_checks_observe(const struct watch_data *data);
