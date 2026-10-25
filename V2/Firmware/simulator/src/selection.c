#include "selection.h"

static void set_x(void *object, int32_t value) { lv_obj_set_x(object, value); }
static void set_y(void *object, int32_t value) { lv_obj_set_y(object, value); }
static void set_width(void *object, int32_t value) { lv_obj_set_width(object, value); }
static void set_height(void *object, int32_t value) { lv_obj_set_height(object, value); }

static void move(lv_obj_t *cursor, int start, int end, lv_anim_exec_xcb_t callback)
{
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, cursor);
    lv_anim_set_values(&animation, start, end);
    lv_anim_set_duration(&animation, 250);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&animation, callback);
    lv_anim_start(&animation);
}

void selection_fit(lv_obj_t *cursor, lv_obj_t *label, lv_obj_t *icon, bool animate)
{
    lv_obj_t *parent = lv_obj_get_parent(cursor);
    lv_obj_update_layout(parent);
    lv_area_t bounds, origin;
    lv_obj_get_coords(label, &bounds);
    lv_obj_get_coords(parent, &origin);
    if (icon) {
        lv_area_t image;
        lv_obj_get_coords(icon, &image);
        bounds.x1 = LV_MIN(bounds.x1, image.x1);
        bounds.x2 = LV_MAX(bounds.x2, image.x2);
        bounds.y1 = LV_MIN(bounds.y1, image.y1);
        bounds.y2 = LV_MAX(bounds.y2, image.y2);
    }
    const int horizontal_padding = icon ? 24 : 14;
    const int x = bounds.x1 - origin.x1 - horizontal_padding;
    const int y = bounds.y1 - origin.y1 - 8;
    const int width = lv_area_get_width(&bounds) + 2 * horizontal_padding;
    const int height = lv_area_get_height(&bounds) + 16;
    lv_obj_set_style_border_color(cursor, lv_color_white(), 0);
    lv_obj_set_style_border_width(cursor, 2, 0);
    lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
    lv_anim_delete(cursor, NULL);
    /* Positions are absolute top-left offsets. A V1 TOP_MID anchor would
     * move x again as width changes and push the capsule beyond its page. */
    lv_area_t current;
    lv_obj_get_coords(cursor, &current);
    const int start_x = current.x1 - origin.x1;
    const int start_y = current.y1 - origin.y1;
    lv_obj_set_align(cursor, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(cursor, start_x, start_y);
    if (animate) {
        move(cursor, start_x, x, set_x);
        move(cursor, start_y, y, set_y);
        move(cursor, lv_obj_get_width(cursor), width, set_width);
        move(cursor, lv_obj_get_height(cursor), height, set_height);
    } else {
        lv_obj_set_pos(cursor, x, y);
        lv_obj_set_size(cursor, width, height);
    }
}

lv_obj_t *selection_place_row(lv_obj_t *label, const lv_image_dsc_t *source, int center_y)
{
    lv_obj_t *parent = lv_obj_get_parent(label);
    lv_obj_update_layout(parent);
    const int gap = source ? 10 : 0;
    const int icon_width = source ? source->header.w : 0;
    const int x = (lv_obj_get_width(parent) - lv_obj_get_width(label) - icon_width - gap) / 2;
    lv_obj_set_align(label, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(label, x + icon_width + gap, center_y - lv_obj_get_height(label) / 2);
    if (!source) return NULL;
    lv_obj_t *icon = lv_image_create(parent);
    lv_image_set_src(icon, source);
    lv_obj_set_pos(icon, x, center_y - source->header.h / 2);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    return icon;
}

void selection_row_color(lv_obj_t *label, lv_obj_t *icon, bool selected)
{
    lv_color_t color = selected ? lv_color_white() : lv_color_hex(0x555555);
    lv_obj_set_style_text_color(label, color, 0);
    if (icon) lv_obj_set_style_image_recolor(icon, color, 0);
}
