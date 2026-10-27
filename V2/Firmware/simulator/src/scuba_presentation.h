#pragma once
#include <stddef.h>
#include <stdint.h>

enum scuba_ascent_color { ASCENT_GREY, ASCENT_GREEN, ASCENT_YELLOW, ASCENT_RED };
struct scuba_ascent_display { unsigned levels; enum scuba_ascent_color color; int highlighted; };
struct scuba_ascent_display scuba_ascent_display(int32_t dm_min, uint32_t fast_ms);
void scuba_surface_text(char *out, size_t size, uint32_t seconds);
void scuba_ndl_text(char *out, size_t size, uint32_t seconds);
/* Approximate air ppO2 for the desktop preview only, in hundredths of a bar. */
uint16_t scuba_demo_ppo2(uint16_t depth_dm);

/* Ring values use thousandths of the fixed scale. */
unsigned scuba_ring_ndl(uint32_t seconds);
unsigned scuba_ring_tts(uint32_t seconds);
unsigned scuba_ring_stop(uint32_t elapsed_ms,uint32_t target_ms);
