/* Host-only adapter, no reference math. One request per line, repeated until
 * EOF. Numeric tokens use strtod syntax (nan/inf accepted for rejection tests).
 * fixed SURFACE GF IDENTITY P0 ... P15
 *   IDENTITY: 0 accepted, 1 invalid policy, 2 invalid coefficient hash.
 * gf CURRENT SURFACE none|ANCHOR
 * violation CURRENT CEILING_STATUS LOCAL_CEILING
 * JSON always includes status; unavailable physiological fields are null.
 * The violation form constructs a status-tagged supplied local result so the
 * exact binary64 threshold can be exercised independently of ceiling math.
 * Malformed commands exit 2. Physiological argument errors stay JSON results.
 */
#include "deco_ceiling.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void number(double value)
{
    if (isfinite(value)) printf("%.17g", value);
    else printf("null");
}

static int parse_double(const char *token, double *value)
{
    char *end;
    *value = strtod(token, &end);
    return end != token && *end == '\0';
}

static int parse_status(const char *token, int *value)
{
    if (strlen(token) != 1 || token[0] < '0' || token[0] > '3') return 0;
    *value = token[0] - '0';
    return 1;
}

int main(void)
{
    char line[8192];
    const deco_tissues_constants *c = deco_tissues_get_constants();
    printf("{\"policy_id\":\"%s\",\"coefficient_id\":\"%s\","
           "\"coefficient_sha256\":\"%s\"}\n", c->policy_id,
           c->coefficient_id, c->coefficient_sha256);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *tokens[21], *token;
        size_t count = 0, i;
        double first, second;
        if (strchr(line, '\n') == NULL && !feof(stdin)) return 2;
        token = strtok(line, " \t\r\n");
        while (token != NULL) {
            if (count == 21) return 2;
            tokens[count++] = token;
            token = strtok(NULL, " \t\r\n");
        }
        if (count == 0) return 2;
        if (strcmp(tokens[0], "fixed") == 0) {
            deco_tissues_state state, before;
            deco_ceiling_result result;
            int identity;
            if (count != 20 || !parse_double(tokens[1], &first) ||
                !parse_double(tokens[2], &second) ||
                !parse_status(tokens[3], &identity) || identity > 2) return 2;
            if (deco_tissues_init_fresh(&state, 1.0) != DECO_TISSUES_OK) return 2;
            if (identity == 1) state.policy_id[0] ^= 1;
            if (identity == 2) state.coefficient_sha256[0] ^= 1;
            for (i = 0; i < DECO_TISSUE_COUNT; ++i)
                if (!parse_double(tokens[4 + i], &state.nitrogen_bar[i])) return 2;
            memcpy(&before, &state, sizeof(state));
            result = deco_ceiling_fixed(&state, first, second);
            printf("{\"status\":%d,\"input_unchanged\":%s,\"raw_limits_bar_abs\":[",
                   (int)result.status, memcmp(&before, &state, sizeof(state)) == 0 ? "true" : "false");
            for (i = 0; i < DECO_TISSUE_COUNT; ++i) {
                if (i) printf(",");
                number(result.raw_limits_bar_abs[i]);
            }
            printf("],\"raw_max_bar_abs\":"); number(result.raw_max_bar_abs);
            printf(",\"local_ceiling_bar_abs\":"); number(result.local_ceiling_bar_abs);
            puts("}");
        } else if (strcmp(tokens[0], "gf") == 0) {
            double anchor, before;
            const double *supplied = NULL;
            deco_gf_result result;
            if (count != 4 || !parse_double(tokens[1], &first) ||
                !parse_double(tokens[2], &second)) return 2;
            if (strcmp(tokens[3], "none") != 0) {
                if (!parse_double(tokens[3], &anchor)) return 2;
                supplied = &anchor;
                memcpy(&before, &anchor, sizeof(anchor));
            }
            result = deco_ceiling_select_gf(first, second, supplied);
            printf("{\"status\":%d,\"input_unchanged\":%s,\"gf\":", (int)result.status,
                   supplied == NULL || memcmp(&before, &anchor, sizeof(anchor)) == 0 ? "true" : "false");
            number(result.gf); puts("}");
        } else if (strcmp(tokens[0], "violation") == 0) {
            int status;
            deco_ceiling_result ceiling = {0};
            deco_violation_result result;
            if (count != 4 || !parse_double(tokens[1], &first) ||
                !parse_status(tokens[2], &status) || !parse_double(tokens[3], &second)) return 2;
            ceiling.status = (deco_tissues_result)status;
            ceiling.local_ceiling_bar_abs = second;
            result = deco_ceiling_check_violation(&ceiling, first);
            printf("{\"status\":%d,\"violated\":", (int)result.status);
            if (result.status == DECO_TISSUES_OK) printf("%s", result.violated ? "true" : "false");
            else printf("null");
            puts("}");
        } else return 2;
    }
    return ferror(stdin) ? 2 : 0;
}
