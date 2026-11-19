/* Shared physical-depth policy; engine reference metres remain unchanged. */
#ifndef WATER_H
#define WATER_H
#include <stdbool.h>
static inline float water_density(bool salt) { return salt ? 1025.0f : 1000.0f; }
static inline float water_depth(float pressure, float surface, float density)
{ return (pressure-surface)*100000.0f/(density*9.80665f); }
static inline float water_pressure(float depth, float surface, float density)
{ return surface+depth*density*9.80665f/100000.0f; }
#endif
