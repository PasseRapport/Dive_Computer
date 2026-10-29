#include "watch_ui.h"
#include "sim_host.h"
#include "apnea_main.h"
#include "apnea_settings.h"
#include "apnea_stats.h"
#include "v1_ui_layout.h"
#include "selection.h"
#include "native_fonts.h"
#include "compass_ui.h"
#include "scuba_ui.h"
#include "battery_icon.h"
#include "light_editor.h"
#include <zephyr/kernel.h>

LV_IMAGE_DECLARE(icon_scuba);
LV_IMAGE_DECLARE(icon_compass);
LV_IMAGE_DECLARE(icon_options);
LV_IMAGE_DECLARE(icon_watch);
LV_IMAGE_DECLARE(project_logo);

static lv_obj_t *pages[WATCH_PAGE_COUNT];
static enum watch_page current_page;
static enum watch_view current_view;
static enum watch_view leave_origin;
static enum watch_mode selected_mode = WATCH_WATCH;
static enum watch_dive_mode dive_mode = DIVE_SCUBA;
static bool sleeping, bluetooth, bluetooth_dfu;
static uint8_t brightness = 3;
static enum watch_backlight_mode backlight_mode = BACKLIGHT_AUTO;
static uint16_t sensitivity = 10;
enum option_item { OPTION_BLUETOOTH, OPTION_WATCH, OPTION_BACKLIGHT, OPTION_HAPTICS, OPTION_SLEEP, OPTION_COUNT };
static bool haptics = true;
static enum watch_pulse pulse = PULSE_MEDIUM;
static int selected_haptics;
static lv_obj_t *haptics_cursor, *haptics_labels[2];
static int selected_option, selected_backlight, selected_bluetooth, selected_face, selected_dial;
static enum watch_dial clock_dial=DIAL_MINIMALIST;
static int digital_dial=DIGITAL_EMBLEM;
static struct watch_data clock_data;
static lv_obj_t *clock_face,*preview,*preview_face,*outgoing,*dial_caption,*save_status;
static bool clock_face_dirty=true;
static int sleep_progress, leave_progress;
static bool recovery_active;
static lv_obj_t *confirm_arc, *recovery_arc;
static lv_obj_t *clock_view, *mode_view, *compass_view, *option_view;
static lv_obj_t *backlight_view, *logo_view, *scuba_view;
static lv_timer_t *logo_timer, *sleep_timer;
static lv_obj_t *mode_cursor, *mode_labels[WATCH_MODE_COUNT], *mode_icons[WATCH_MODE_COUNT];
static lv_obj_t *option_cursor, *option_labels[OPTION_COUNT];
static lv_obj_t *backlight_cursor, *backlight_labels[3];
static lv_obj_t *face_view, *face_cursor, *face_labels[2], *dial_view;
static lv_obj_t *free_battery, *logo_image;
static enum watch_clock_style clock_style = CLOCK_DIGITAL;
static lv_obj_t *bluetooth_cursor, *bluetooth_labels[2];
static const enum watch_page free_pages[] = {WATCH_MAIN, WATCH_STATS, WATCH_WATER, WATCH_LEAVE};
static const enum watch_page scuba_pages[] = {WATCH_SCUBA, WATCH_LEAVE};

static lv_obj_t *make_view(lv_obj_t *parent)
{
    lv_obj_t *view = lv_obj_create(parent);
    lv_obj_remove_style_all(view);
    lv_obj_set_size(view, 320, 300);
    lv_obj_set_style_bg_color(view, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(view, LV_OPA_COVER, 0);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    return view;
}

static lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static lv_obj_t *cursor(lv_obj_t *parent)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, 100, 42);
    lv_obj_set_style_bg_color(object, lv_color_hex(0x008b8b), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

static void refresh_clock_style(void)
{
    if(clock_face_dirty) {
        if(clock_face)lv_obj_delete(clock_face);
        clock_face=watch_face_create(clock_view,clock_style,clock_style==CLOCK_ANALOG?clock_dial:digital_dial);
        watch_face_update(clock_face,&clock_data);clock_face_dirty=false;
    }
    lv_obj_set_style_opa(clock_face,255,0);
    lv_obj_t *logo=watch_face_logo(clock_face);if(logo)lv_obj_remove_flag(logo,LV_OBJ_FLAG_HIDDEN);
}

static void logo_opacity(void *object, int32_t opacity)
{
    lv_obj_set_style_opa(object, opacity, 0);
}

static void fit_rows(lv_obj_t *pill, lv_obj_t **labels, int count, int selected, bool animate)
{
    selection_fit(pill, labels[selected], NULL, animate);
    for (int i = 0; i < count; i++) selection_row_color(labels[i], NULL, i == selected);
}

static void refresh_watch_menu(bool animate)
{
    lv_label_set_text(face_labels[0],clock_style==CLOCK_ANALOG?"TYPE : ANALOGIQUE":"TYPE : NUMERIQUE");
    lv_obj_align(face_labels[0],LV_ALIGN_TOP_MID,0,105);
    fit_rows(face_cursor,face_labels,2,selected_face,animate);
}
static void save_watch(void)
{
    int result=sim_host_watch_preferences_save(clock_style,clock_dial,digital_dial);
    lv_label_set_text(save_status,result?"SAVE FAILED":"");
}
static void slide_x(void *object,int32_t x){lv_obj_set_x(object,x);}
static void slide_finished(lv_anim_t *a)
{
    lv_obj_delete(a->var);outgoing=NULL;
}
static void finish_slides(void)
{
    if(outgoing){lv_anim_delete(outgoing,NULL);lv_obj_delete(outgoing);outgoing=NULL;}
    if(preview){lv_anim_delete(preview,NULL);lv_obj_set_x(preview,40);}
}
static void clear_preview(void)
{
    finish_slides();if(preview){lv_obj_delete(preview);preview=NULL;preview_face=NULL;}
}
static void show_preview(int direction)
{
    finish_slides();outgoing=preview;
    preview=lv_obj_create(dial_view);lv_obj_remove_style_all(preview);lv_obj_set_size(preview,300,300);
    lv_obj_set_style_radius(preview,LV_RADIUS_CIRCLE,0);lv_obj_set_style_clip_corner(preview,true,0);
    lv_obj_remove_flag(preview,LV_OBJ_FLAG_SCROLLABLE);
    preview_face=watch_face_create(preview,clock_style,selected_dial);lv_obj_set_x(preview_face,-10);
    watch_face_update(preview_face,&clock_data);
    lv_obj_set_style_transform_pivot_x(preview,0,0);lv_obj_set_style_transform_pivot_y(preview,0,0);
    lv_obj_set_style_transform_scale(preview,205,0);lv_obj_set_pos(preview,direction?40+direction*320:40,8);
    lv_obj_move_background(preview);
    lv_label_set_text_fmt(dial_caption,"%d/%d  %s",selected_dial+1,watch_face_count(clock_style),watch_face_name(clock_style,selected_dial));
    lv_obj_align(dial_caption,LV_ALIGN_TOP_MID,0,262);
    if(!direction){if(outgoing){lv_obj_delete(outgoing);outgoing=NULL;}return;}
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_exec_cb(&a,slide_x);lv_anim_set_duration(&a,220);lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out);
    lv_anim_set_var(&a,preview);lv_anim_set_values(&a,40+direction*320,40);lv_anim_start(&a);
    lv_anim_set_var(&a,outgoing);lv_anim_set_values(&a,40,40-direction*320);lv_anim_set_completed_cb(&a,slide_finished);lv_anim_start(&a);
}

static void refresh_mode_menu(bool animate)
{
    selection_fit(mode_cursor, mode_labels[selected_mode], mode_icons[selected_mode], animate);
    for (int i = 0; i < WATCH_MODE_COUNT; i++)
        selection_row_color(mode_labels[i], mode_icons[i], i == selected_mode);
}

static void refresh_options(bool animate)
{
    fit_rows(option_cursor, option_labels, OPTION_COUNT, selected_option, animate);
}

static void refresh_backlight(bool animate)
{
    const char *names[] = {"ON", "OFF", "AUTO"};
    lv_label_set_text(backlight_labels[0], names[backlight_mode]);
    lv_obj_align(backlight_labels[0], LV_ALIGN_TOP_MID, 0, 105);
    fit_rows(backlight_cursor, backlight_labels, 3, selected_backlight, animate);
}

static void refresh_haptics(bool animate)
{
    static const char *const pulses[] = {"SHORT", "MEDIUM", "LONG"};
    lv_label_set_text(haptics_labels[0], haptics ? "ON" : "OFF");
    lv_label_set_text(haptics_labels[1], pulses[pulse]);
    for (int i = 0; i < 2; i++) selection_place_row(haptics_labels[i], NULL, 68 + 56*i);
    fit_rows(haptics_cursor, haptics_labels, 2, selected_haptics, animate);
}

static void refresh_bluetooth(bool animate)
{
    lv_label_set_text(bluetooth_labels[0], bluetooth ? LV_SYMBOL_POWER "  ON" : LV_SYMBOL_POWER "  OFF");
    lv_label_set_text(bluetooth_labels[1], bluetooth_dfu ? "DFU" : "DATA");
    for (int i = 0; i < 2; i++) selection_place_row(bluetooth_labels[i], NULL, 68 + 56*i);
    fit_rows(bluetooth_cursor, bluetooth_labels, 2, selected_bluetooth, animate);
}

static void refresh_sensitivity(void)
{
    if(pages[WATCH_SENSITIVITY])light_editor_set_value(pages[WATCH_SENSITIVITY],sensitivity);
}

static void refresh_recovery_visibility(void)
{
    if(free_battery) {
        bool show=current_view==WATCH_DIVE_VIEW&&(current_page==WATCH_MAIN||current_page==WATCH_STATS)&&dive_mode==DIVE_FREE&&!sleeping;
        if(show)lv_obj_remove_flag(free_battery,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(free_battery,LV_OBJ_FLAG_HIDDEN);
    }
    bool visible = recovery_active && current_view == WATCH_DIVE_VIEW &&
                   current_page == WATCH_MAIN && dive_mode == DIVE_FREE && !sleeping;
    if (visible) {
        lv_obj_remove_flag(recovery_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(recovery_arc);
    } else lv_obj_add_flag(recovery_arc, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_confirmation(void)
{
    bool sleep = current_view == WATCH_OPTION_DETAIL && current_page == WATCH_SLEEP;
    bool leave = (current_view == WATCH_DIVE_VIEW || current_view == WATCH_TOOL_LEAVE) &&
                 current_page == WATCH_LEAVE;
    if ((sleep || leave) && !sleeping) {
        lv_arc_set_value(confirm_arc, sleep ? sleep_progress : leave_progress);
        lv_obj_remove_flag(confirm_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(confirm_arc);
    } else lv_obj_add_flag(confirm_arc, LV_OBJ_FLAG_HIDDEN);
}

static void set_view(enum watch_view next)
{
    lv_obj_t *roots[] = {clock_view, mode_view, compass_view, option_view, backlight_view, logo_view, face_view, dial_view, app_screen, scuba_view};
    for (unsigned i = 0; i < sizeof(roots)/sizeof(roots[0]); i++)
        lv_obj_add_flag(roots[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *visible = next == WATCH_CLOCK ? clock_view :
                        next == WATCH_MODE_MENU ? mode_view :
                        next == WATCH_COMPASS_VIEW ? compass_view :
                        next == WATCH_OPTION_VIEW ? option_view :
                        next == WATCH_BACKLIGHT_VIEW ? backlight_view :
                        (next == WATCH_BOOT || next == WATCH_SLEEP_TRANSITION) ? logo_view :
                        next == WATCH_FACE_SELECT ? face_view :
                        next == WATCH_DIAL_SELECT ? dial_view :
                        next == WATCH_SCUBA_VIEW ? scuba_view : app_screen;
    current_view = next;
    sleeping = false;
    lv_obj_remove_flag(visible, LV_OBJ_FLAG_HIDDEN);
    refresh_confirmation();
    refresh_recovery_visibility();
}

static void finish_logo(void)
{
    if (logo_timer) { lv_timer_delete(logo_timer); logo_timer = NULL; }
    lv_anim_delete(logo_image, NULL);
    if (current_view == WATCH_BOOT) {
        refresh_clock_style();
        set_view(WATCH_CLOCK);
    }
}

static void logo_finished(lv_timer_t *timer)
{
    (void)timer;
    finish_logo();
}

static void logo_shrink(void *object, int32_t progress)
{
    /* Show the live face behind the moving logo; keep only one octopus visible. */
    lv_obj_set_style_bg_opa(logo_view, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(clock_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *logo=watch_face_logo(clock_face);if(logo)lv_obj_add_flag(logo,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(clock_face,LV_CLAMP(0,(progress-850)*255/150,255),0);
    int scale=logo?watch_face_logo_scale(clock_face):0;
    int center_y=logo?watch_face_logo_y(clock_face):150;
    lv_obj_set_x(object,60+(watch_face_logo_x(clock_face)-160)*progress/1000);
    lv_image_set_scale(object, 256 + (scale - 256)*progress/1000);
    lv_obj_set_y(object, 150 + (center_y - 150)*progress/1000 - project_logo.header.h/2);
}

static lv_obj_t *detail_page(const char *title, const char *message)
{
    lv_obj_t *page = make_view(app_screen);
    lv_obj_set_size(page, 224, 168);
    lv_obj_set_pos(page, V1_PAGE_HOME_X + V1_PAGE_TRAVEL, V1_PAGE_HOME_Y);
    text(page, title, &lv_font_montserrat_24, 8);
    if (message && *message) {
        lv_obj_t *label = text(page, message, &lv_font_montserrat_16, 65);
        lv_obj_set_width(label, 224);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xaaaaaa), 0);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 65);
    }
    return page;
}

static lv_obj_t *ring(int width, int maximum)
{
    lv_obj_t *arc = lv_arc_create(app_screen);
    lv_obj_set_size(arc, 296, 296);
    lv_obj_set_pos(arc, 12, 2);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_rotation(arc, 90);
    lv_arc_set_range(arc, 0, maximum);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_border_width(arc,0,LV_PART_KNOB);lv_obj_set_style_shadow_width(arc,0,LV_PART_KNOB);
    lv_obj_set_style_arc_rounded(arc,false,LV_PART_MAIN);lv_obj_set_style_arc_rounded(arc,true,LV_PART_INDICATOR);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    return arc;
}

static void show_page(enum watch_page next, int direction, bool animate)
{
    sleeping = false;
    /* Fast input cancels old slides before showing the next page. */
    for (int i = 0; i < WATCH_PAGE_COUNT; i++) {
        lv_anim_delete(pages[i], NULL);
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(pages[i], V1_PAGE_HOME_X + V1_PAGE_TRAVEL);
    }
    lv_obj_t *out = pages[current_page], *in = pages[next];
    lv_obj_remove_flag(in, LV_OBJ_FLAG_HIDDEN);
    if (next != current_page && animate) {
        lv_obj_remove_flag(out, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(out, (320-lv_obj_get_width(out))/2);
        ui_slide_pages(out, in, direction);
    } else lv_obj_set_x(in, (320-lv_obj_get_width(in))/2);
    if(next==WATCH_BRIGHTNESS||next==WATCH_SENSITIVITY)lv_obj_set_pos(in,0,0);
    if(next!=WATCH_WATER)apnea_settings_leave();
    current_page = next;
    refresh_confirmation();
    refresh_recovery_visibility();
}

static void build_menu(lv_obj_t *view, const char *title, const char *const names[],
                       lv_obj_t **labels, lv_obj_t **pill, int count)
{
    lv_obj_t *heading = text(view, title, &lv_font_montserrat_24, count == 5 ? 32 : 48);
    lv_obj_set_style_text_color(heading, lv_color_white(), 0);
    *pill = cursor(view);
    for (int i = 0; i < count; i++)
        labels[i] = text(view, names[i], count == 5 ? &lv_font_montserrat_22 : &lv_font_montserrat_24,
                         count == 5 ? 78 + 40*i : count == 4 ? 92 + 44*i : 105 + 48*i);
}

void watch_ui_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_text_color(parent, lv_color_white(), 0);
    int type=CLOCK_DIGITAL,analog=DIAL_MINIMALIST,digital=DIGITAL_EMBLEM;
    (void)sim_host_watch_preferences_load(&type,&analog,&digital);
    clock_style=type;clock_dial=analog;digital_dial=digital;
    int water=0,field=0;
    (void)sim_host_apnea_preferences_load(&water,&field);
    apnea_settings_restore(water,(enum apnea_bottom_field)field);
    v1_ui_build(parent);
    ui_skip_boot();
    pages[WATCH_MAIN] = main_screen;
    pages[WATCH_STATS] = stats_screen;
    lv_obj_add_flag(water_screen,LV_OBJ_FLAG_HIDDEN);
    pages[WATCH_WATER] = apnea_settings_create(app_screen);
    pages[WATCH_DIVE_MODE] = ui_create_dive_mode_page();
    lv_obj_add_flag(brightness_screen,LV_OBJ_FLAG_HIDDEN);
    pages[WATCH_BRIGHTNESS]=light_editor_create(app_screen,"BRIGHTNESS",10);
    lv_obj_t *hint;
    /* The legacy hidden page owns the periodically refreshed battery label. */
    lv_obj_add_flag(sleep_screen, LV_OBJ_FLAG_HIDDEN);
    sleep_screen = detail_page("SLEEP", "Turn the crown\nto confirm sleep");
    pages[WATCH_SLEEP] = sleep_screen;
    pages[WATCH_LEAVE] = detail_page("LEAVE", "Turn the crown to leave");
    pages[WATCH_SCUBA] = detail_page("SCUBADIVING", "Interface coming soon\n\nDepth: -- m\nTime: --:--");
    pages[WATCH_BLUETOOTH] = detail_page("BLUETOOTH", "");
    bluetooth_cursor = cursor(pages[WATCH_BLUETOOTH]);
    for (int i = 0; i < 2; i++) bluetooth_labels[i] = text(pages[WATCH_BLUETOOTH], "", &lv_font_montserrat_24, 0);
    hint = text(pages[WATCH_BLUETOOTH], "Tap to change", &lv_font_montserrat_14, 153);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xaaaaaa), 0);
    refresh_bluetooth(false);

    pages[WATCH_HAPTICS] = detail_page("HAPTICS", "");
    haptics_cursor = cursor(pages[WATCH_HAPTICS]);
    for (int i = 0; i < 2; i++) haptics_labels[i] = text(pages[WATCH_HAPTICS], "", &lv_font_montserrat_24, 0);
    hint = text(pages[WATCH_HAPTICS], "Tap to change", &lv_font_montserrat_14, 153);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xaaaaaa), 0);
    refresh_haptics(false);

    pages[WATCH_SENSITIVITY]=light_editor_create(app_screen,"SENSITIVITY",100);
    refresh_sensitivity();
    confirm_arc = ring(10, 24);
    recovery_arc = ring(8, 1000);
    current_page = WATCH_MAIN;
    ui_refresh_brightness_bar(brightness);
    if(pages[WATCH_BRIGHTNESS])light_editor_set_value(pages[WATCH_BRIGHTNESS],brightness);

    clock_view = make_view(parent);
    free_battery=battery_icon_create(app_screen,142,16);
    refresh_clock_style();
    mode_view = make_view(parent);
    mode_cursor = cursor(mode_view);
    const char *names[] = {"WATCH", "DIVING", "COMPASS", "OPTIONS"};
    const lv_image_dsc_t *icons[] = {&icon_watch, &icon_scuba, &icon_compass, &icon_options};
    for (int i = 0; i < WATCH_MODE_COUNT; i++) {
        mode_labels[i] = text(mode_view, names[i], &lv_font_montserrat_28, 0);
        mode_icons[i] = selection_place_row(mode_labels[i], icons[i], 58 + 58*i);
    }
    refresh_mode_menu(false);
    scuba_view = scuba_ui_create(parent);
    compass_view = compass_ui_create(parent);
    option_view = make_view(parent);
    const char *options[] = {"BLUETOOTH", "WATCH", "BACKLIGHT", "HAPTICS", "SLEEP"};
    build_menu(option_view, "OPTIONS", options, option_labels, &option_cursor, OPTION_COUNT);
    refresh_options(false);
    backlight_view = make_view(parent);
    const char *light_options[] = {"AUTO", "BRIGHTNESS", "SENSITIVITY"};
    build_menu(backlight_view, "BACKLIGHT", light_options, backlight_labels, &backlight_cursor, 3);
    refresh_backlight(false);
    face_view = make_view(parent);
    const char *face_options[]={"TYPE : NUMERIQUE","CADRAN"};
    build_menu(face_view,"WATCH",face_options,face_labels,&face_cursor,2);
    save_status=text(face_view,"",&lv_font_montserrat_12,220);
    refresh_watch_menu(false);
    dial_view=make_view(parent);
    dial_caption=text(dial_view,"",&lv_font_montserrat_14,262);
    logo_view = make_view(parent);
    logo_image = lv_image_create(logo_view);
    lv_image_set_src(logo_image, &project_logo);
    lv_obj_set_pos(logo_image, (320 - project_logo.header.w)/2, 150 - project_logo.header.h/2);
    lv_obj_set_style_opa(logo_image, LV_OPA_TRANSP, 0);
    set_view(WATCH_BOOT);
    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, logo_image);
    lv_anim_set_exec_cb(&fade, logo_opacity);
    lv_anim_set_values(&fade, 0, 255);
    lv_anim_set_duration(&fade, 500);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_in_out);
    lv_anim_start(&fade);
    lv_anim_t shrink;
    lv_anim_init(&shrink);
    lv_anim_set_var(&shrink, logo_image);
    lv_anim_set_exec_cb(&shrink, logo_shrink);
    lv_anim_set_values(&shrink, 0, 1000);
    lv_anim_set_delay(&shrink, 1000);
    lv_anim_set_duration(&shrink, 1000);
    lv_anim_set_early_apply(&shrink, false);
    lv_anim_set_path_cb(&shrink, lv_anim_path_ease_in_out);
    lv_anim_start(&shrink);
    logo_timer = lv_timer_create(logo_finished, 2000, NULL);
}

static void sleep_logo_frame(void *object, int32_t progress)
{
    int background = progress < 250 ? progress*255/250 : 255;
    int logo = progress < 250 ? progress*255/250 :
               progress <= 500 ? 255 : (1000-progress)*255/500;
    lv_obj_set_style_bg_opa(logo_view, background, 0);
    lv_obj_set_style_opa(object, logo, 0);
}

static void sleep_finished(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_delete(sleep_timer);
    sleep_timer = NULL;
    lv_anim_delete(logo_image, NULL);
    sleep_logo_frame(logo_image, 1000);
    lv_obj_add_flag(app_screen, LV_OBJ_FLAG_HIDDEN);
    sleeping = true;
}

static void start_sleep_transition(void)
{
    lv_anim_delete(logo_image, NULL);
    lv_image_set_scale(logo_image, 256);
    lv_obj_set_pos(logo_image, (320-project_logo.header.w)/2, 150-project_logo.header.h/2);
    sleep_logo_frame(logo_image, 0);
    set_view(WATCH_SLEEP_TRANSITION);
    /* Crossfade the current screen into the logo, then the logo into black. */
    lv_obj_remove_flag(app_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(logo_view);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, logo_image);
    lv_anim_set_exec_cb(&animation, sleep_logo_frame);
    lv_anim_set_values(&animation, 0, 1000);
    lv_anim_set_duration(&animation, 2000);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_start(&animation);
    sleep_timer = lv_timer_create(sleep_finished, 2000, NULL);
}

static int bounded_selection(int value, int steps, int count)
{
    return LV_CLAMP(0, (int64_t)value + steps, count - 1);
}

static void open_dive_selection(void)
{
    leave_progress = 0;
    set_view(WATCH_DIVE_SELECT);
    show_page(WATCH_DIVE_MODE, 1, false);
}

static void open_tool_exit(void)
{
    leave_origin = current_view;
    leave_progress = 0;
    set_view(WATCH_TOOL_LEAVE);
    show_page(WATCH_LEAVE, 1, false);
}

static void scroll_tool_exit(int steps)
{
    if (steps < 0 && leave_progress == 0) {
        set_view(leave_origin);
        return;
    }
    leave_progress = LV_CLAMP(0, (int64_t)leave_progress + steps, 24);
    if (leave_progress == 24) {
        leave_progress = 0;
        refresh_mode_menu(false);
        set_view(WATCH_MODE_MENU);
    } else refresh_confirmation();
}

static void scroll_dive(int steps)
{
    const enum watch_page *sequence = dive_mode == DIVE_FREE ? free_pages : scuba_pages;
    const int count = dive_mode == DIVE_FREE ? 4 : 2;
    if (current_page == WATCH_LEAVE) {
        if (steps < 0 && leave_progress == 0) {
            show_page(sequence[count-2], -1, true);
            return;
        }
        leave_progress = LV_CLAMP(0, (int64_t)leave_progress + steps, 24);
        if (leave_progress == 24) {
            leave_progress = 0;
            refresh_mode_menu(false);
            set_view(WATCH_MODE_MENU);
        }
        else refresh_confirmation();
        return;
    }
    int index = 0;
    for (int i = 0; i < count; i++) if (sequence[i] == current_page) index = i;
    int next = LV_CLAMP(0, (int64_t)index + steps, count-1);
    if (next != index) {
        leave_progress = 0;
        show_page(sequence[next], steps > 0 ? 1 : -1, true);
    }
}

void watch_ui_input(enum watch_input input, int steps)
{
    if (current_view == WATCH_BOOT) finish_logo();
    if (current_view == WATCH_SLEEP_TRANSITION && !sleeping) return;
    if (sleeping) {
        if (input != WATCH_CROWN) {
            sleep_progress = 0;
            refresh_options(false);
            set_view(WATCH_OPTION_VIEW);
        }
        return;
    }
    if (current_view==WATCH_DIVE_VIEW && current_page==WATCH_WATER && apnea_settings_input(input,steps)) return;
    if (current_view == WATCH_SCUBA_VIEW) {
        if (scuba_ui_input(input,steps)) {refresh_mode_menu(false);set_view(WATCH_MODE_MENU);}
        return;
    }
    if (input == WATCH_CROWN && steps != 0) {
        if (current_view == WATCH_CLOCK || current_view == WATCH_COMPASS_VIEW) {
            open_tool_exit();
        } else if (current_view == WATCH_TOOL_LEAVE) {
            scroll_tool_exit(steps);
        } else if (current_view == WATCH_MODE_MENU) {
            selected_mode = bounded_selection(selected_mode, steps, WATCH_MODE_COUNT);
            refresh_mode_menu(true);
        } else if (current_view == WATCH_FACE_SELECT) {
            selected_face = bounded_selection(selected_face, steps, 2);
            refresh_watch_menu(true);
        } else if (current_view == WATCH_DIAL_SELECT) {
            int count=watch_face_count(clock_style);
            selected_dial=((selected_dial+(int64_t)steps)%count+count)%count;
            show_preview(steps>0?1:-1);
        } else if (current_view == WATCH_DIVE_SELECT) {
            dive_mode = bounded_selection(dive_mode, steps, 2);
            ui_refresh_dive_mode(dive_mode == DIVE_FREE);
        } else if (current_view == WATCH_DIVE_VIEW) {
            scroll_dive(steps);
        } else if (current_view == WATCH_OPTION_VIEW) {
            selected_option = bounded_selection(selected_option, steps, OPTION_COUNT);
            refresh_options(true);
        } else if (current_view == WATCH_BACKLIGHT_VIEW) {
            selected_backlight = bounded_selection(selected_backlight, steps, 3);
            refresh_backlight(true);
        } else if (current_view == WATCH_OPTION_DETAIL) {
            if (current_page == WATCH_BRIGHTNESS) {
                brightness = LV_CLAMP(0, (int64_t)brightness + steps, 10);
                ui_refresh_brightness_bar(brightness);
    if(pages[WATCH_BRIGHTNESS])light_editor_set_value(pages[WATCH_BRIGHTNESS],brightness);
            } else if (current_page == WATCH_SENSITIVITY) {
                sensitivity = LV_CLAMP(0, (int64_t)sensitivity + (int64_t)steps*10, 100);
                refresh_sensitivity();

            } else if (current_page == WATCH_BLUETOOTH) {
                selected_bluetooth = bounded_selection(selected_bluetooth, steps, 2);
                refresh_bluetooth(true);
            } else if (current_page == WATCH_HAPTICS) {
                selected_haptics = bounded_selection(selected_haptics, steps, 2);
                refresh_haptics(true);
            } else if (current_page == WATCH_SLEEP) {
                sleep_progress = LV_CLAMP(0, (int64_t)sleep_progress + steps, 24);
                if (sleep_progress == 24) {
                    start_sleep_transition();
                } else refresh_confirmation();
            }
        }
    } else if (input == WATCH_DOUBLE_TAP) {
        if (current_view == WATCH_DIVE_VIEW || current_view == WATCH_COMPASS_VIEW ||
            current_view == WATCH_CLOCK || current_view == WATCH_TOOL_LEAVE) return;
        if (current_view == WATCH_MODE_MENU) set_view(WATCH_CLOCK);
        else if (current_view == WATCH_OPTION_DETAIL) {
            if (current_page == WATCH_BRIGHTNESS || current_page == WATCH_SENSITIVITY) {
                refresh_backlight(false);
                set_view(WATCH_BACKLIGHT_VIEW);
            } else {
                sleep_progress = 0;
                refresh_options(false);
                set_view(WATCH_OPTION_VIEW);
            }
        } else if (current_view == WATCH_DIAL_SELECT) {
            clear_preview();refresh_watch_menu(false);
            set_view(WATCH_FACE_SELECT);
        } else if (current_view == WATCH_FACE_SELECT) {
            refresh_options(false);
            set_view(WATCH_OPTION_VIEW);
        } else if (current_view == WATCH_BACKLIGHT_VIEW) {
            refresh_options(false);
            set_view(WATCH_OPTION_VIEW);
        } else if (current_view != WATCH_CLOCK) {
            refresh_mode_menu(false);
            set_view(WATCH_MODE_MENU);
        }
    } else if (input == WATCH_SINGLE_TAP) {
        if (current_view == WATCH_MODE_MENU) {
            if (selected_mode == WATCH_WATCH) set_view(WATCH_CLOCK);
            else if (selected_mode == WATCH_COMPASS) set_view(WATCH_COMPASS_VIEW);
            else if (selected_mode == WATCH_OPTION) {
                refresh_options(false);
                set_view(WATCH_OPTION_VIEW);
            } else open_dive_selection();
        } else if (current_view == WATCH_FACE_SELECT) {
            if(selected_face==1) {
                selected_dial=clock_style==CLOCK_ANALOG?clock_dial:digital_dial;
                show_preview(0);set_view(WATCH_DIAL_SELECT);
            } else {
                clock_style=clock_style==CLOCK_ANALOG?CLOCK_DIGITAL:CLOCK_ANALOG;
                clock_face_dirty=true;refresh_clock_style();refresh_watch_menu(false);save_watch();
            }
        } else if (current_view == WATCH_DIAL_SELECT) {
            if(clock_style==CLOCK_ANALOG)clock_dial=selected_dial;else digital_dial=selected_dial;
            clear_preview();clock_face_dirty=true;refresh_clock_style();refresh_watch_menu(false);save_watch();set_view(WATCH_FACE_SELECT);
        } else if (current_view == WATCH_DIVE_SELECT) {
            if(dive_mode == DIVE_SCUBA) {
                set_view(WATCH_SCUBA_VIEW);
                scuba_ui_enter(k_uptime_get());
            } else {
                set_view(WATCH_DIVE_VIEW);
                show_page(WATCH_MAIN,1,false);
            }

        } else if (current_view == WATCH_OPTION_VIEW) {
            if (selected_option == OPTION_BACKLIGHT) {
                selected_backlight=0;refresh_backlight(false);
                set_view(WATCH_BACKLIGHT_VIEW);
            } else if (selected_option == OPTION_WATCH) {
                selected_face = 0;
                refresh_watch_menu(false);
                set_view(WATCH_FACE_SELECT);
            } else {
                sleep_progress = 0;
                if (selected_option == OPTION_BLUETOOTH) { selected_bluetooth = 0; refresh_bluetooth(false); }
                if (selected_option == OPTION_HAPTICS) { selected_haptics = 0; refresh_haptics(false); }
                set_view(WATCH_OPTION_DETAIL);
                show_page(selected_option == OPTION_SLEEP ? WATCH_SLEEP :
                          selected_option == OPTION_HAPTICS ? WATCH_HAPTICS : WATCH_BLUETOOTH, 1, false);
            }
        } else if (current_view == WATCH_BACKLIGHT_VIEW) {
            if (selected_backlight == 0) {
                backlight_mode = (backlight_mode + 1) % 3;
                refresh_backlight(true);
            } else {
                set_view(WATCH_OPTION_DETAIL);
                show_page(selected_backlight == 1 ? WATCH_BRIGHTNESS : WATCH_SENSITIVITY, 1, false);
            }
        } else if (current_view == WATCH_OPTION_DETAIL) {
            if (current_page == WATCH_BRIGHTNESS || current_page == WATCH_SENSITIVITY) {
                refresh_backlight(false);
                set_view(WATCH_BACKLIGHT_VIEW);
            } else if (current_page == WATCH_BLUETOOTH) {
                if (selected_bluetooth == 0) bluetooth = !bluetooth;
                else bluetooth_dfu = !bluetooth_dfu;
                refresh_bluetooth(true);
            } else if (current_page == WATCH_HAPTICS) {
                if (selected_haptics == 0) haptics = !haptics;
                else pulse = (pulse + 1) % 3;
                refresh_haptics(true);
            }
        }
    }
}

void watch_ui_update(const struct watch_data *data)
{
    scuba_ui_update(data,k_uptime_get(),current_view==WATCH_SCUBA_VIEW);
    clock_data=*data;
    watch_face_update(clock_face,data);
    if(preview_face)watch_face_update(preview_face,data);
    battery_icon_update(free_battery,data->battery_percent);
    compass_ui_update(data->heading_degrees);
    apnea_settings_update(data->apnea_active);
    apnea_main_update(pages[WATCH_MAIN],data);
    apnea_stats_update(pages[WATCH_STATS],data);
    recovery_active = data->recovery_ring_visible;
    int progress = data->recovery_target_ms ?
                   (uint64_t)data->recovery_elapsed_ms * 1000 / data->recovery_target_ms : 0;
    lv_arc_set_value(recovery_arc, LV_MIN(progress, 1000));
    lv_obj_set_style_arc_color(recovery_arc, data->recovery_red ?
                              lv_color_hex(0xff0000) : lv_color_hex(0x00ff00), LV_PART_INDICATOR);
    refresh_recovery_visibility();
    ui_refresh_battery(data->battery_percent);
}

enum watch_page watch_ui_page(void) { return current_view==WATCH_SCUBA_VIEW ? (scuba_ui_screen()==SCUBA_LEAVE?WATCH_LEAVE:WATCH_SCUBA) : current_page; }
bool watch_ui_is_sleeping(void) { return sleeping; }
bool watch_ui_salt_water(void) { return apnea_settings_salt_water(); }
void watch_ui_set_brightness(uint8_t value) { brightness=LV_MIN(value,10);ui_refresh_brightness_bar(brightness);
    if(pages[WATCH_BRIGHTNESS])light_editor_set_value(pages[WATCH_BRIGHTNESS],brightness); }
uint8_t watch_ui_brightness(void) { return brightness; }
enum watch_view watch_ui_view(void) { return current_view; }
enum watch_mode watch_ui_mode(void) { return selected_mode; }
enum watch_dive_mode watch_ui_dive_mode(void) { return dive_mode; }
enum watch_clock_style watch_ui_clock_style(void) { return clock_style; }
int watch_ui_face_option(void) { return selected_face; }
enum watch_dial watch_ui_dial(void){return clock_dial;}
int watch_ui_digital_dial(void){return digital_dial;}
bool watch_ui_dial_animating(void){return outgoing!=NULL;}
int watch_ui_dial_option(void){return selected_dial;}
int watch_ui_sleep_progress(void) { return sleep_progress; }
int watch_ui_leave_progress(void) { return leave_progress; }
bool watch_ui_bluetooth(void) { return bluetooth; }
bool watch_ui_bluetooth_dfu(void) { return bluetooth_dfu; }
int watch_ui_bluetooth_option(void) { return selected_bluetooth; }
bool watch_ui_haptics(void) { return haptics; }
enum watch_pulse watch_ui_pulse(void) { return pulse; }
int watch_ui_haptics_option(void) { return selected_haptics; }
int watch_ui_option(void) { return selected_option; }
int watch_ui_backlight_option(void) { return selected_backlight; }
enum watch_backlight_mode watch_ui_backlight_mode(void) { return backlight_mode; }
uint16_t watch_ui_sensitivity(void) { return sensitivity; }
const char *watch_ui_page_name(void)
{
    if (current_view == WATCH_SCUBA_VIEW) return "SCUBA DIVING";
    if (current_view == WATCH_BOOT) return "PROJECT LOGO";
    if (current_view == WATCH_SLEEP_TRANSITION) return sleeping ? "DISPLAY OFF" : "GOING TO SLEEP";
    if (current_view == WATCH_CLOCK) return "CLOCK";
    if (current_view == WATCH_TOOL_LEAVE) return "LEAVE";
    if (current_view == WATCH_MODE_MENU) return "MAIN MENU";
    if (current_view == WATCH_COMPASS_VIEW) return "COMPASS";
    if (current_view == WATCH_OPTION_VIEW) return "OPTIONS";
    if (current_view == WATCH_BACKLIGHT_VIEW) return "BACKLIGHT";
    if (current_view == WATCH_FACE_SELECT) return "WATCH";
    if (current_view == WATCH_DIAL_SELECT) return "CADRAN";
    static const char *const names[] = {"MAIN", "STATS", "SETTINGS", "MODE", "BRIGHTNESS", "SLEEP", "SCUBADIVING", "BLUETOOTH", "LEAVE", "SENSITIVITY", "HAPTICS"};
    return names[current_page];
}

void watch_ui_set_backlight_mode(enum watch_backlight_mode mode)
{
    if(mode<=BACKLIGHT_AUTO)backlight_mode=mode;
    refresh_backlight(false);
}
void watch_ui_set_sensitivity(uint16_t value)
{
    sensitivity=LV_MIN((value+5)/10,10)*10;
    refresh_sensitivity();
}

#ifdef DIVE_PC_BENCH
void watch_ui_open_calculated_scuba(void)
{
    finish_logo();set_view(WATCH_SCUBA_VIEW);scuba_ui_enter(0);
}
#endif
