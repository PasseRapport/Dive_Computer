Requested UI cleanup, 2026-10-28. NO and DATA are stacked and text-centred
under DECO in the existing red94x77/radius10 badge. Pixel glyph centres agree
within1.5pixels. Calculated bench hides the legacy ascent/descent/surface row,
obsolete scenario/time controls and both Scale controls. The remaining battery
control and haptic indicator are compacted under the watch inputs. Standalone
legacy demos retain their own controls. Native restart and calculated-watch
recipes, gap-time/restart/calculated render checks pass. PC only; no core changes.
src/scuba_dashboard.c SHA256 506bfe8e69a296638bbbfdb5bff4c3f9133681fb93007f3a3745f51690e6d76b
src/sim_controls.c SHA256 e35a56b172072d634598fc7acbe6bcc3221196e4f53ad4c31afa6545d63f2e95
