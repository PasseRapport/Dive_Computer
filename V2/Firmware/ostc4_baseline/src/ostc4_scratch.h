/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OSTC4_SCRATCH_H
#define OSTC4_SCRATCH_H
/* All planner mutable file state and backup storage in the pinned revision. */
typedef struct {
    float surface, pressure, nitrogen[16], helium[16], gf, cns, gf_depth;
    int gas_id;
    float stop_depth;
    int stop_id;
    float backup_pressure, backup_gas_id, backup_nitrogen[16], backup_helium[16];
    float backup_gf, backup_cns;
} ostc4_scratch;
void ostc4_scratch_save(ostc4_scratch *state);
void ostc4_scratch_restore(const ostc4_scratch *state);
#endif
