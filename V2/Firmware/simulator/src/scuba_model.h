#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Desktop demonstration only. NDL and stops are scripted, not physiological
 * calculations. This module must not supply guidance for a real dive. */
enum scuba_scenario { SCUBA_MANUAL, SCUBA_NO_DECO, SCUBA_SAFETY, SCUBA_DECO, SCUBA_ALERTS, SCUBA_SCENARIO_COUNT };
enum scuba_alert { SCUBA_CEILING = 1, SCUBA_ASCENT = 2, SCUBA_BATTERY = 4,
                   SCUBA_NDL = 8, SCUBA_DEPTH = 16, SCUBA_TIME = 32, SCUBA_OXYGEN = 64, SCUBA_DECOMPRESSION = 128 };
struct scuba_summary {
    bool valid;
    uint32_t duration_s;
    uint16_t max_dm, average_dm;
    int16_t min_temperature;
    bool missed_stop;
};
struct scuba_model {
    bool armed, active, salt_water, safety_required, deco_started, deco_done;
    uint16_t depth_dm, peak_dm, start_depth_dm, depth_alarm_dm;
    uint32_t time_alarm_s, ndl_s, ndl_budget_s, surface_ms, stop_ms, fast_ascent_ms;
    uint8_t speed, battery, deco_stage;
    int16_t temperature, min_temperature;
    int32_t ascent_dm_min;
    uint32_t alerts, acknowledged, notifications;
    uint64_t last_ms, scenario_ms, dive_ms, depth_integral, underwater_ms;
    uint64_t rate_ms, fast_until_ms;
    uint32_t rate_depth_mm;
    enum scuba_scenario scenario;
    struct scuba_summary summary;
};
void scuba_model_init(struct scuba_model *s);
void scuba_model_arm(struct scuba_model *s, uint64_t now_ms);
void scuba_model_finish(struct scuba_model *s);
void scuba_model_scenario(struct scuba_model *s, enum scuba_scenario scenario, uint64_t now_ms);
void scuba_model_update(struct scuba_model *s, uint64_t now_ms, uint16_t manual_depth_dm, int16_t temperature, uint8_t battery);
void scuba_model_acknowledge(struct scuba_model *s);
uint16_t scuba_model_ceiling(const struct scuba_model *s);
uint32_t scuba_model_stop_target(const struct scuba_model *s);
uint32_t scuba_model_tts(const struct scuba_model *s);
bool scuba_model_in_stop(const struct scuba_model *s);
bool scuba_model_surface(const struct scuba_model *s);
const char *scuba_model_scenario_name(enum scuba_scenario scenario);

void scuba_model_update_mm(struct scuba_model *s, uint64_t now_ms, uint32_t manual_depth_mm, int16_t temperature, uint8_t battery);
