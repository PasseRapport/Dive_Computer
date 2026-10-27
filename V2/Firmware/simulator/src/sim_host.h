#pragma once

/* Host-only SDL bridge; never accesses LVGL from an SDL event callback. */
void sim_host_init(void);
int sim_host_take_wheel(void);
int sim_host_check_enabled(void);
void sim_host_push_wheel(int x, int y, int steps);
void sim_host_push_button(int x, int y, int pressed);
int sim_host_capture(const char *path);
const char *sim_host_capture_path(void);
const char *sim_host_gallery_path(void);
int sim_host_set_zoom(unsigned zoom);
unsigned sim_host_get_zoom(void);
void sim_host_get_clock(unsigned *hour, unsigned *minute);

int sim_host_water_check_enabled(void);

int sim_host_dive_check_enabled(void);

int sim_host_take_orientation(void);

int sim_host_scuba_check_enabled(void);

void sim_host_get_date(unsigned *day,unsigned *month,unsigned *weekday,unsigned *second);
int sim_host_scale_check_enabled(void);
int sim_host_appearance_check_enabled(void);
int sim_host_zoom_matches(unsigned zoom);

/* Consume window redraw requests on the UI thread, never in the SDL watcher. */
int sim_host_take_redraw(void);
/* Native regression test: detect a fully black simulator surface. */
int sim_host_frame_visible(void);
