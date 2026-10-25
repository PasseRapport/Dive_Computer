#include "sim_controls.h"
#include "sim_host.h"
#include "sim_layout.h"
#include "watch_ui.h"
#include "scuba_ui.h"
#include <zephyr/sys/printk.h>

static lv_obj_t *last_event;
static lv_obj_t *counts_label;
static lv_obj_t *zoom_label;
static unsigned single_taps, double_taps;
static int total_steps;
static uint16_t depth_dm, heading_degrees;
static lv_obj_t *orientation_label;
static lv_obj_t *depth_label;
static lv_obj_t *scuba_panel, *scenario_label, *speed_label, *battery_label, *haptic_label;
static bool positive_wheel_seen, negative_wheel_seen;

static lv_obj_t *label_at(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void show_counts(void)
{
    lv_label_set_text_fmt(counts_label, "Steps: %+d\nSingle: %u   Double: %u",
                         total_steps, single_taps, double_taps);
}

static void single_tap(lv_event_t *event)
{
    (void)event;
    single_taps++;
    watch_ui_input(WATCH_SINGLE_TAP, 0);
    lv_label_set_text_fmt(last_event, "Tap : %s", watch_ui_page_name());
    show_counts();
}

static void double_tap(lv_event_t *event)
{
    (void)event;
    double_taps++;
    /* A double tap is one distinct input, never two single taps. */
    watch_ui_input(WATCH_DOUBLE_TAP, 0);
    lv_label_set_text_fmt(last_event, "Back: %s", watch_ui_page_name());
    show_counts();
}

void sim_controls_set_depth_dm(uint16_t value)
{
    depth_dm=LV_MIN(value,1000);
    lv_label_set_text_fmt(depth_label,"Wheel: rotate | Depth: %u.%u m",depth_dm/10,depth_dm%10);
}

static void change_depth(lv_event_t *event)
{
    intptr_t direction = (intptr_t)lv_event_get_user_data(event);
    if(watch_ui_view()==WATCH_SCUBA_VIEW) {depth_dm=scuba_ui_model()->depth_dm;scuba_ui_manual();}
    int next = direction == 0 ? 0 : (int)depth_dm + direction * 10;
    depth_dm = LV_CLAMP(0, next, 1000);
    lv_label_set_text_fmt(depth_label, "Wheel: rotate | Depth: %u.%u m", depth_dm / 10, depth_dm % 10);
}

uint16_t sim_controls_depth_dm(void) { return depth_dm; }

static void depth_button(lv_obj_t *parent, const char *text, int x, intptr_t direction)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, SIM_DEPTH_Y);
    lv_obj_set_size(button, SIM_DEPTH_BUTTON_WIDTH, SIM_DEPTH_BUTTON_HEIGHT);
    lv_obj_add_event_cb(button, change_depth, LV_EVENT_CLICKED, (void *)direction);
    if (direction) lv_obj_add_event_cb(button, change_depth, LV_EVENT_LONG_PRESSED_REPEAT, (void *)direction);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

static void change_zoom(lv_event_t *event)
{
    (void)event;
    unsigned zoom = sim_host_get_zoom() == 2 ? 1 : 2;
    if (sim_host_set_zoom(zoom) == 0) {
        lv_label_set_text_fmt(zoom_label, "Scale: x%u (click)", zoom);
        lv_obj_invalidate(lv_screen_active());
    }
}

static lv_obj_t *panel_button(lv_obj_t *parent, const char *text, int y,
                         lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, SIM_PANEL_X, y);
    lv_obj_set_size(button, SIM_CONTROL_WIDTH, SIM_BUTTON_HEIGHT);
    lv_obj_set_style_radius(button, 10, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return label;
}

static void scenario_click(lv_event_t *event)
{
    (void)event;
    scuba_ui_set_scenario((scuba_ui_model()->scenario+1)%SCUBA_SCENARIO_COUNT);
}
static void speed_click(lv_event_t *event)
{
    (void)event;
    unsigned speed=scuba_ui_model()->speed;
    scuba_ui_set_speed(speed==1?10:speed==10?60:1);
}
static void battery_click(lv_event_t *event)
{
    (void)event;
    unsigned battery=scuba_ui_model()->battery;
    scuba_ui_set_battery(battery>15?15:battery>5?5:87);
}
static lv_obj_t *test_button(lv_obj_t *parent,int y,lv_event_cb_t callback)
{
    lv_obj_t *button=lv_button_create(parent);
    lv_obj_set_pos(button,0,y);lv_obj_set_size(button,224,30);
    lv_obj_add_event_cb(button,callback,LV_EVENT_CLICKED,NULL);
    lv_obj_t *label=lv_label_create(button);lv_obj_center(label);return label;
}

lv_obj_t *sim_controls_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x20252d), 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    label_at(screen, "SCREEN 320 x 300 - CIRCLE 300 PX", 22, 7);
    lv_obj_t *bezel = lv_obj_create(screen);
    lv_obj_remove_style_all(bezel);
    lv_obj_set_pos(bezel, 18, 28);
    lv_obj_set_size(bezel, 328, 308);
    lv_obj_set_style_bg_color(bezel, lv_color_hex(0x9aa6b7), 0);
    lv_obj_set_style_bg_opa(bezel, LV_OPA_COVER, 0);
    lv_obj_remove_flag(bezel, LV_OBJ_FLAG_SCROLLABLE);

    /* User-selected 300-pixel circular aperture, centred in the 320 x 300
     * matrix. This simulation setting does not claim the panel's geometry. */
    lv_obj_t *viewport = lv_obj_create(screen);
    lv_obj_remove_style_all(viewport);
    lv_obj_set_pos(viewport, 22, 32);
    lv_obj_set_size(viewport, 320, 300);
    lv_obj_set_style_bg_color(viewport, lv_color_hex(0x697586), 0);
    lv_obj_set_style_bg_opa(viewport, LV_OPA_COVER, 0);
    lv_obj_remove_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *aperture = lv_obj_create(viewport);
    lv_obj_remove_style_all(aperture);
    lv_obj_set_pos(aperture, 10, 0);
    lv_obj_set_size(aperture, 300, 300);
    lv_obj_set_style_radius(aperture, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(aperture, true, 0);
    lv_obj_set_style_bg_color(aperture, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(aperture, LV_OPA_COVER, 0);
    lv_obj_remove_flag(aperture, LV_OBJ_FLAG_SCROLLABLE);

    /* UI coordinates and layout remain the original 320 x 300 rectangle. */
    lv_obj_t *watch = lv_obj_create(aperture);
    lv_obj_remove_style_all(watch);
    lv_obj_set_pos(watch, -10, 0);
    lv_obj_set_size(watch, 320, 300);
    lv_obj_set_style_bg_opa(watch, LV_OPA_COVER, 0);
    lv_obj_remove_flag(watch, LV_OBJ_FLAG_SCROLLABLE);

    label_at(screen, "CONTROLS", SIM_PANEL_X, 20);
    label_at(screen, "Watch - LVGL simulator", SIM_PANEL_X, 43);
    lv_obj_t *crown = lv_obj_create(screen);
    lv_obj_set_pos(crown, SIM_PANEL_X, SIM_CROWN_Y);
    lv_obj_set_size(crown, SIM_CONTROL_WIDTH, SIM_CROWN_HEIGHT);
    lv_obj_set_style_bg_color(crown, lv_color_hex(0x353f4e), 0);
    lv_obj_set_style_text_color(crown, lv_color_white(), 0);
    lv_obj_set_style_border_color(crown, lv_color_hex(0x7b8da6), 0);
    lv_obj_set_style_border_width(crown, 2, 0);
    lv_obj_set_style_radius(crown, 10, 0);
    lv_obj_remove_flag(crown, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *hint = lv_label_create(crown);
    lv_label_set_text(hint, "CROWN\nMouse wheel: scroll");
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(hint);
    panel_button(screen, "Single tap", SIM_TAP_Y, single_tap);
    panel_button(screen, "Double tap", SIM_DOUBLE_Y, double_tap);
    last_event = label_at(screen, "Waiting for input", SIM_PANEL_X, 275);
    counts_label = label_at(screen, "", SIM_PANEL_X, 302);
    show_counts();
    lv_obj_t *orientation = lv_obj_create(screen);
    lv_obj_remove_style_all(orientation);
    lv_obj_set_pos(orientation, SIM_ORIENTATION_X, SIM_ORIENTATION_Y);
    lv_obj_set_size(orientation, SIM_ORIENTATION_WIDTH, SIM_ORIENTATION_HEIGHT);
    lv_obj_set_style_bg_color(orientation, lv_color_hex(0x353f4e), 0);
    lv_obj_set_style_bg_opa(orientation, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(orientation, 6, 0);
    lv_obj_remove_flag(orientation, LV_OBJ_FLAG_SCROLLABLE);
    orientation_label = label_at(orientation, "ORIENTATION: 000°", 7, 1);
    depth_label = label_at(orientation, "Wheel: rotate | Depth: 0.0 m", 7, 19);
    lv_obj_set_style_text_font(depth_label, &lv_font_montserrat_12, 0);
    depth_button(screen, "Descend", SIM_DEPTH_DOWN_X, 1);
    depth_button(screen, "Ascend", SIM_DEPTH_UP_X, -1);
    depth_button(screen, "Surface", SIM_SURFACE_X, 0);
    zoom_label = panel_button(screen, "", SIM_ZOOM_Y, change_zoom);
    lv_label_set_text_fmt(zoom_label, "Scale: x%u (click)", sim_host_get_zoom());
    label_at(screen, "x1: 1 screen pixel per pixel", SIM_PANEL_X, 402);
    scuba_panel=lv_obj_create(screen);lv_obj_remove_style_all(scuba_panel);
    lv_obj_set_pos(scuba_panel,SIM_PANEL_X,274);lv_obj_set_size(scuba_panel,224,146);
    lv_obj_set_style_bg_color(scuba_panel,lv_color_hex(0x20252d),0);lv_obj_set_style_bg_opa(scuba_panel,LV_OPA_COVER,0);
    lv_obj_remove_flag(scuba_panel,LV_OBJ_FLAG_SCROLLABLE);
    scenario_label=test_button(scuba_panel,0,scenario_click);
    speed_label=test_button(scuba_panel,34,speed_click);
    battery_label=test_button(scuba_panel,68,battery_click);
    haptic_label=label_at(scuba_panel,"DEMO VALUES",0,110);
    lv_obj_set_style_text_font(haptic_label,&lv_font_montserrat_12,0);
    lv_obj_t *scale=lv_button_create(scuba_panel);lv_obj_set_pos(scale,124,104);lv_obj_set_size(scale,100,30);
    lv_obj_add_event_cb(scale,change_zoom,LV_EVENT_CLICKED,NULL);
    lv_obj_t *scale_text=lv_label_create(scale);lv_label_set_text(scale_text,"Scale x1/x2");lv_obj_set_style_text_font(scale_text,&lv_font_montserrat_12,0);lv_obj_center(scale_text);
    lv_obj_add_flag(scuba_panel,LV_OBJ_FLAG_HIDDEN);
    return watch;
}

uint16_t sim_controls_heading_degrees(void) { return heading_degrees; }

void sim_controls_poll(void)
{
    bool scuba=watch_ui_view()==WATCH_SCUBA_VIEW;
    if(scuba) {
        const struct scuba_model *s=scuba_ui_model();
        lv_obj_remove_flag(scuba_panel,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(scenario_label,"Scenario: %s",scuba_model_scenario_name(s->scenario));lv_obj_center(scenario_label);
        lv_label_set_text_fmt(speed_label,"Time: x%u",s->speed);lv_obj_center(speed_label);
        lv_label_set_text_fmt(battery_label,"Battery: %u%%",s->battery);lv_obj_center(battery_label);
        lv_label_set_text(haptic_label,scuba_ui_haptic_active()?"VIBRATION":"DEMO VALUES");
        lv_obj_set_style_text_color(haptic_label,lv_color_hex(scuba_ui_haptic_active()?0xffaa66:0xaaaaaa),0);
        lv_label_set_text_fmt(depth_label,"Wheel: rotate | Depth: %u.%u m",s->depth_dm/10,s->depth_dm%10);
    } else lv_obj_add_flag(scuba_panel,LV_OBJ_FLAG_HIDDEN);

    int rotation = -sim_host_take_orientation();
    if (rotation) {
        int next = (heading_degrees + (rotation % 72)*5) % 360;
        heading_degrees = next < 0 ? next + 360 : next;
        lv_label_set_text_fmt(orientation_label, "ORIENTATION: %03u°", heading_degrees);
    }
    /* Desktop wheel direction requested by the user: reverse the old mapping. */
    int steps = -sim_host_take_wheel();
    if (steps != 0) {
        positive_wheel_seen |= steps > 0;
        negative_wheel_seen |= steps < 0;
        total_steps += steps;
        watch_ui_input(WATCH_CROWN, steps);
        lv_label_set_text_fmt(last_event, "Screen: %s", watch_ui_page_name());
        show_counts();
    }
}

bool sim_controls_check_passed(void)
{
    printk("Control counts: wheel=%d single=%u double=%u\n", total_steps, single_taps, double_taps);
    return total_steps == -1 && single_taps == 1 && double_taps == 1 &&
           positive_wheel_seen && negative_wheel_seen;
}
