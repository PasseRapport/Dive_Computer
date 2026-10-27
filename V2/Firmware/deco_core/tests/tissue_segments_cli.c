/* Host-only adapter. argv[1] is fresh surface bar; stdin has one segment per
 * line: start_bar_abs end_bar_abs duration_ms. JSON-lines output begins with
 * immutable constants and initial tissues, then one result per input segment.
 * No reference implementation or calculations are embedded here. */
#include "deco_tissues.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static void print_tissues(const deco_tissues_state *state, deco_tissues_result result)
{
    size_t i;
    printf("{\"result\":%d,\"nitrogen_bar\":[", (int)result);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        printf("%s%.17g", i ? "," : "", state->nitrogen_bar[i]);
    }
    puts("]}");
}

/* Read the complete bounded token before accepting a segment. Decimal digits
 * only: strtoull/scanf also accept signs and can conceal uint64 overflow. */
static int read_duration(uint64_t *duration)
{
    int ch;
    unsigned digits = 0;
    uint64_t value = 0;
    do { ch = getchar(); } while (ch != EOF && isspace((unsigned char)ch));
    while (ch != EOF && !isspace((unsigned char)ch)) {
        unsigned digit;
        if (ch < '0' || ch > '9' || ++digits > 20) return 0;
        digit = (unsigned)(ch - '0');
        if (value > (UINT64_MAX - digit) / 10) return 0;
        value = value * 10 + digit;
        ch = getchar();
    }
    if (digits == 0 || value == 0) return 0;
    *duration = value;
    return 1;
}

int main(int argc, char **argv)
{
    deco_tissues_state state;
    const deco_tissues_constants *c = deco_tissues_get_constants();
    char *end;
    double surface, start, finish;
    uint64_t duration;
    size_t i;
    int fields;
    if (argc != 2) return 2;
    surface = strtod(argv[1], &end);
    if (end == argv[1] || *end != '\0' ||
        deco_tissues_init_fresh(&state, surface) != DECO_TISSUES_OK) return 2;
    printf("{\"policy_id\":\"%s\",\"coefficient_id\":\"%s\","
           "\"coefficient_sha256\":\"%s\",\"nitrogen_fraction\":%.17g,"
           "\"water_vapour_bar\":%.17g,\"coefficients\":[",
           c->policy_id, c->coefficient_id, c->coefficient_sha256,
           c->nitrogen_fraction, c->water_vapour_bar);
    for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
        printf("%s[%.17g,%.17g,%.17g]", i ? "," : "",
               c->compartments[i].half_time_min, c->compartments[i].a_bar,
               c->compartments[i].b);
    }
    puts("]}");
    print_tissues(&state, DECO_TISSUES_OK);
    while ((fields = scanf("%lf %lf", &start, &finish)) == 2) {
        if (!read_duration(&duration)) return 2;
        deco_tissues_result result = deco_tissues_integrate(&state, start, finish, duration);
        print_tissues(&state, result);
        if (result != DECO_TISSUES_OK) return 1;
    }
    return fields == EOF ? 0 : 2;
}
