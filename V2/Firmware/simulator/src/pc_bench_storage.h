/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PC_BENCH_STORAGE_H
#define PC_BENCH_STORAGE_H
#include "ostc4_exposure.h"
#include <stdbool.h>
#include <stddef.h>
struct pc_bench_log {
    unsigned long long id;
    uint64_t recording_id; /* Stable session/start identity; zero in preserved schema-1 logs. */
    uint32_t started_seconds, completed_seconds, dive_seconds;
    ostc4_gf_pair active_gf;
    uint16_t max_depth_dm; /* Original physical peak, never reprojected from later preferences. */
    uint16_t density_kg_m3; /* 1000/1025 for new records; 0 means unknown legacy water. */
    char policy[192];
    bool automatic_completion;
};
struct pc_bench_storage_state {
    bool enabled, preference_loaded, preference_saved;
    ostc4_gf_pair saved_preference;
    size_t log_count, unsaved_logs;
    const char *preference_error, *log_error;
};
/* PC adapter only. Configure at process startup, before explicit fresh exposure.
 * NULL disables storage. Configuration never touches the calculation module. */
int pc_storage_configure(const char *directory);
const struct pc_bench_storage_state *pc_storage_read(void);
void pc_storage_save_preference(ostc4_gf_pair pair);
void pc_storage_append_log(const struct pc_bench_log *record);
void pc_storage_reconcile_log(const struct pc_bench_log *record);
uint64_t pc_storage_new_session_id(void);
int pc_storage_log_read(size_t index,struct pc_bench_log *record);
#endif
