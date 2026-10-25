#pragma once
#include <lvgl.h>

/* Fit the capsule to the actual rendered label and optional icon bounds. */
void selection_fit(lv_obj_t *cursor, lv_obj_t *label, lv_obj_t *icon, bool animate);

/* Centre a label and optional native-size icon as a single menu row. */
lv_obj_t *selection_place_row(lv_obj_t *label, const lv_image_dsc_t *source, int center_y);
void selection_row_color(lv_obj_t *label, lv_obj_t *icon, bool selected);
