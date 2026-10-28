/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PC_BENCH_STORAGE_H
#define PC_BENCH_STORAGE_H
#include "ostc4_exposure.h"
#include <stdbool.h>
#include <stddef.h>
struct pc_bench_log {
    unsigned long long id;
    uint32_t started_seconds, completed_seconds, dive_seconds;
    ostc4_gf_pair active_gf;
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
int pc_storage_log_read(size_t index,struct pc_bench_log *record);
#endif
