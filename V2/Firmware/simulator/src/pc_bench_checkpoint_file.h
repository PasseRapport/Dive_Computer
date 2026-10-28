/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PC_BENCH_CHECKPOINT_FILE_H
#define PC_BENCH_CHECKPOINT_FILE_H
#include "pc_bench.h"
/* PC-only durable envelope around the unchanged portable v2 bytes. No startup
 * resume. Caller asserts complete exposure and clock evidence explicitly.
 * configuration_unchanged asserts no GF/manual recording/threshold events since
 * the save; pressure-only replay cannot prove those events. */
int pc_bench_checkpoint_file_save(const char *path);
ostc4_restart_status pc_bench_checkpoint_file_load(const char *path,const ostc4_recovery *evidence,bool configuration_unchanged);
/* Explicit user assertion: restart is exactly the saved simulation instant,
 * with zero elapsed exposure. This is never inferred from wall-clock time. */
ostc4_restart_status pc_bench_checkpoint_file_load_saved_instant(const char *path);
/* Directory is shared with simulator preferences/logs; never creates it. */
const char *pc_bench_checkpoint_file_default_path(void);
/* Bench/PC adapter transfer boundary, not a second recovery representation. */
struct pc_bench_checkpoint_state {
    unsigned char core[OSTC4_CHECKPOINT_BYTES];
    struct recorded_dive recording;
    ostc4_gf_pair preferred;
    float density;
};
int pc_bench_checkpoint_export(struct pc_bench_checkpoint_state *state);
ostc4_restart_status pc_bench_checkpoint_import(const struct pc_bench_checkpoint_state *state,
    const ostc4_recovery *evidence,bool legacy_ambiguous);
ostc4_restart_status pc_bench_checkpoint_reject(ostc4_restart_status status);
void pc_bench_checkpoint_notice(const char *message);
#endif
