#include "compass_ui.h"

struct compass_face {
    lv_obj_t *ticks[72], *cardinals[4], *heading_label, *direction_label;
    lv_point_precise_t tick_points[72][2], marker_points[3];
    uint16_t last_heading;
};
static lv_obj_t *default_face;

static void compass_deleted(lv_event_t *event) { lv_free(lv_event_get_user_data(event)); }

static lv_point_precise_t point(int angle, int radius)
{
    return (lv_point_precise_t){
        160 + ((lv_trigo_sin(angle) * radius) >> LV_TRIGO_SHIFT),
        150 - ((lv_trigo_sin(angle + 90) * radius) >> LV_TRIGO_SHIFT),
    };
}

void compass_face_update(lv_obj_t *view, uint16_t heading_degrees)
{
    struct compass_face *face=lv_obj_get_user_data(view);
    uint16_t heading = heading_degrees % 360;
    if (heading == face->last_heading) return;
    face->last_heading = heading;
    for (int i = 0; i < 72; i++) {
        int angle = i*5 - heading;
        face->tick_points[i][0] = point(angle, i % 18 == 0 ? 129 : i % 6 == 0 ? 134 : 140);
        face->tick_points[i][1] = point(angle, 146);
        lv_line_set_points(face->ticks[i], face->tick_points[i], 2);
    }
    for (int i = 0; i < 4; i++) {
        lv_point_precise_t position = point(i*90 - heading, 110);
        lv_obj_update_layout(face->cardinals[i]);
        lv_obj_set_pos(face->cardinals[i], position.x - lv_obj_get_width(face->cardinals[i])/2,
                       position.y - lv_obj_get_height(face->cardinals[i])/2);
    }
    lv_label_set_text_fmt(face->heading_label, "%03u°", heading);
    lv_obj_align(face->heading_label, LV_ALIGN_CENTER, 0, 0);
    const char *directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    lv_label_set_text(face->direction_label, directions[((heading + 22)/45) % 8]);
    lv_obj_align(face->direction_label, LV_ALIGN_TOP_MID, 0, 183);
}

lv_obj_t *compass_face_create(lv_obj_t *parent)
{
    lv_obj_t *view = lv_obj_create(parent);
    struct compass_face *face=lv_malloc_zeroed(sizeof(*face));
    LV_ASSERT_MALLOC(face);
    face->last_heading=UINT16_MAX;
    lv_obj_set_user_data(view,face);
    lv_obj_add_event_cb(view, compass_deleted, LV_EVENT_DELETE, face);
    lv_obj_remove_style_all(view);
    lv_obj_set_size(view, 320, 300);
    lv_obj_set_style_bg_color(view, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(view, LV_OPA_COVER, 0);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 72; i++) {
        face->ticks[i] = lv_line_create(view);
        lv_line_set_points(face->ticks[i], face->tick_points[i], 2);
        lv_obj_set_style_line_width(face->ticks[i], i % 18 == 0 ? 3 : 1, 0);
        lv_obj_set_style_line_color(face->ticks[i], i == 0 ? lv_color_hex(0x00bcbc) :
                                   i % 6 == 0 ? lv_color_white() : lv_color_hex(0x777777), 0);
    }
    const char *names[] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; i++) {
        face->cardinals[i] = lv_label_create(view);
        lv_label_set_text(face->cardinals[i], names[i]);
        lv_obj_set_style_text_font(face->cardinals[i], &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(face->cardinals[i], i == 0 ? lv_color_hex(0x00bcbc) : lv_color_white(), 0);
    }
    /* Fixed index at the top: the compass rose rotates underneath it. */
    face->marker_points[0] = (lv_point_precise_t){154, 5};
    face->marker_points[1] = (lv_point_precise_t){160, 14};
    face->marker_points[2] = (lv_point_precise_t){166, 5};
    lv_obj_t *marker = lv_line_create(view);
    lv_line_set_points(marker, face->marker_points, 3);
    lv_obj_set_style_line_width(marker, 3, 0);
    lv_obj_set_style_line_color(marker, lv_color_hex(0x00bcbc), 0);
    face->heading_label = lv_label_create(view);
    lv_obj_set_style_text_font(face->heading_label, &lv_font_montserrat_42, 0);
    face->direction_label = lv_label_create(view);
    lv_obj_set_style_text_font(face->direction_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(face->direction_label, lv_color_hex(0x00bcbc), 0);
    compass_face_update(view,0);
    return view;
}

lv_obj_t *compass_ui_create(lv_obj_t *parent) { default_face=compass_face_create(parent); return default_face; }
void compass_ui_update(uint16_t heading) { compass_face_update(default_face,heading); }
uint16_t compass_ui_heading(void) { return ((struct compass_face *)lv_obj_get_user_data(default_face))->last_heading; }

void compass_face_set_direction_visible(lv_obj_t *view, bool visible)
{
    struct compass_face *face=lv_obj_get_user_data(view);
    if (visible) lv_obj_remove_flag(face->direction_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(face->direction_label, LV_OBJ_FLAG_HIDDEN);
}
