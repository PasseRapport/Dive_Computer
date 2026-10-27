/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Headless CSV -> JSON Lines driver. Synthetic baseline observations only. */
#include "ostc4_baseline.h"
#include <ctype.h>
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_sample(char *line, ostc4_sample *sample)
{
    char *end;
    if (!isdigit((unsigned char)line[0])) return 0;
    errno = 0;
    uintmax_t timestamp = strtoumax(line, &end, 10);
    if (errno || timestamp > UINT32_MAX || *end != ',') return 0;
    char *start = end + 1;
    errno = 0;
    float pressure = strtof(start, &end);
    if (errno || end == start || !isfinite(pressure)) return 0;
    while (isspace((unsigned char)*end)) ++end;
    if (*end) return 0;
    sample->timestamp_seconds = (uint32_t)timestamp;
    sample->absolute_pressure_bar = pressure;
    return 1;
}

static void print_float_array(const float *values, int count)
{
    putchar('[');
    for (int i = 0; i < count; ++i) printf("%s%.9g", i ? "," : "", (double)values[i]);
    putchar(']');
}

static void print_observation(const ostc4_observation *out)
{
    printf("{\"type\":\"observation\",\"timestamp_seconds\":%" PRIu32
           ",\"absolute_pressure_bar\":%.9g,\"dive_seconds\":%" PRIu32
           ",\"nitrogen_bar\":", out->timestamp_seconds,
           (double)out->absolute_pressure_bar, out->dive_seconds);
    print_float_array(out->nitrogen_bar, OSTC4_TISSUES);
    printf(",\"helium_bar\":");
    print_float_array(out->helium_bar, OSTC4_TISSUES);
    printf(",\"gf_anchor_ambient_bar\":%.9g,\"gf_low_depth_bar\":%.9g,"
           "\"ndl_seconds\":%d,\"tts_seconds\":%d,\"ceiling_meters\":%.9g,\"stop_seconds\":[",
           (double)out->gf_anchor_ambient_bar, (double)out->gf_low_depth_bar,
           out->ndl_seconds, out->tts_seconds, (double)out->ceiling_meters);
    for (int i = 0; i < OSTC4_STOPS; ++i)
        printf("%s%u", i ? "," : "", (unsigned int)out->stop_seconds[i]);
    printf("],\"availability\":{\"tissues\":\"%s\",\"ndl\":\"%s\",\"tts\":\"%s\","
           "\"ceiling\":\"%s\",\"stops\":\"%s\"}}\n",
           ostc4_availability_name(out->tissues), ostc4_availability_name(out->ndl),
           ostc4_availability_name(out->tts), ostc4_availability_name(out->ceiling),
           ostc4_availability_name(out->stops));
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: ostc4_replay profile.csv\n"); return 2; }
    if (fegetround() != FE_TONEAREST) { fprintf(stderr, "requires round-to-nearest\n"); return 2; }
    FILE *file = fopen(argv[1], "r");
    if (!file) { perror(argv[1]); return 2; }
    char line[256];
    if (!fgets(line, sizeof(line), file) ||
        (strcmp(line, "timestamp_seconds,absolute_pressure_bar\n") &&
         strcmp(line, "timestamp_seconds,absolute_pressure_bar\r\n"))) {
        fprintf(stderr, "invalid profile header\n"); fclose(file); return 2;
    }
    unsigned int number = 1;
    ostc4_sample sample;
    while (fgets(line, sizeof(line), file)) {
        ++number;
        if (!parse_sample(line, &sample)) {
            fprintf(stderr, "invalid input at line %u\n", number); fclose(file); return 2;
        }
        ostc4_status status;
        if (number == 2) {
            const ostc4_initial initial = {sample.timestamp_seconds, sample.absolute_pressure_bar};
            status = ostc4_initialize(&initial);
            if (status == OSTC4_OK)
                printf("{\"type\":\"metadata\",\"revision\":\"%s\",\"policy\":\"%s\","
                       "\"correction_set\":\"none\",\"provenance\":\"experimental-fork-beta\","
                       "\"synthetic\":true,\"initialization\":\"upstream-surface-equilibrium\","
                       "\"surface_pressure_bar\":%.9g,\"initial_timestamp_seconds\":%" PRIu32 ","
                       "\"compiler\":\"%s\",\"float_eval_method\":%d,\"float_mantissa_bits\":%d,"
                       "\"double_mantissa_bits\":%d,\"rounding\":\"nearest\","
                       "\"required_math_flags\":\"-O2 -fno-fast-math -ffp-contract=off\","
                       "\"nitrogen_fraction_exposure\":0.79,\"nitrogen_fraction_reset\":0.7902,"
                       "\"water_vapour_bar\":0.0493,\"gf\":[30,70],\"ascent_m_per_min\":9,"
                       "\"bar_per_meter\":0.1,\"stop_gauge_bar\":[0.3,0.6],\"stop_increment_bar\":0.3,"
                       "\"no_deco_tts_semantics\":\"upstream-zero-not-transit-time\"}\n",
                       OSTC4_REVISION, OSTC4_POLICY, (double)initial.surface_pressure_bar,
                       initial.timestamp_seconds, __VERSION__, FLT_EVAL_METHOD, FLT_MANT_DIG, DBL_MANT_DIG);
        } else status = ostc4_advance(&sample);
        if (status != OSTC4_OK) {
            fprintf(stderr, "calculation rejected line %u (status %d); output is incomplete\n", number, status);
            fclose(file); return 2;
        }
        ostc4_observation observed;
        if (ostc4_read(&observed) != OSTC4_OK) { fclose(file); return 2; }
        print_observation(&observed);
    }
    const int failed = ferror(file) || number < 2;
    fclose(file);
    if (failed) { fprintf(stderr, "empty or unreadable profile\n"); return 2; }
    return fflush(stdout) == 0 ? 0 : 2;
}
