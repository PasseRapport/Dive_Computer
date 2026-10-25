#include "scuba_ui.h"
#include "selection.h"
#include "native_fonts.h"
#include "compass_ui.h"
#include "v1_ui_layout.h"
#include <zephyr/kernel.h>
#include <stdio.h>

static struct scuba_model model;
static enum scuba_screen screen, brightness_origin;
static lv_obj_t *root, *content, *arc, *title, *value, *metric, *detail, *hint;
static lv_obj_t *menu, *pill, *rows[3], *compass, *compass_depth, *compass_time;
static lv_obj_t *alert_text, *alert_hint, *brightness_bar, *depth_unit, *compass_unit;
static int setting, leave_progress;
static uint8_t test_battery=87, brightness;
static uint32_t haptic_count, previous_notifications;
static uint64_t last_haptic_ms;
static bool last_active;
static uint16_t heading;

static int bounded_value(int value, int steps, int maximum)
{
    return LV_CLAMP(0,(int64_t)value+steps,maximum);
}
static lv_obj_t *label(lv_obj_t *parent, int y, const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(parent);
    lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);
    return o;
}
static void message(lv_obj_t *o, const char *s, int y)
{
    lv_label_set_text(o,s);
    lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);
}
/* Centre the complete depth reading, with a smaller unit on its baseline. */
static void depth_reading(lv_obj_t *digits, lv_obj_t *unit, int y,
                          const lv_font_t *digits_font, const lv_font_t *unit_font)
{
    char text[24];
    snprintf(text,sizeof(text),"%u.%u",model.depth_dm/10,model.depth_dm%10);
    lv_obj_set_style_text_font(digits,digits_font,0);
    lv_label_set_text(digits,text);
    lv_obj_set_style_text_font(unit,unit_font,0);
    lv_label_set_text(unit,"m");
    lv_obj_set_align(digits,LV_ALIGN_TOP_LEFT);
    lv_obj_set_align(unit,LV_ALIGN_TOP_LEFT);
    lv_obj_update_layout(digits);
    lv_obj_update_layout(unit);
    int width=lv_obj_get_width(digits)+6+lv_obj_get_width(unit);
    int left=(320-width)/2;
    lv_obj_set_pos(digits,left,y);
    lv_obj_set_pos(unit,left+lv_obj_get_width(digits)+6,
                   y+digits_font->line_height-digits_font->base_line
                    -unit_font->line_height+unit_font->base_line);
}
static void hide(lv_obj_t *o, bool hidden)
{
    if (hidden) lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);
}
static void duration(char *out, size_t size, uint32_t seconds)
{
    snprintf(out,size,"%02u:%02u",seconds/60,seconds%60);
}
static const char *alert_name(uint32_t flags)
{
    if (flags & SCUBA_CEILING) return "BELOW CEILING REQUIRED";
    if (flags & SCUBA_ASCENT) return "ASCENT TOO FAST";
    if (flags & SCUBA_BATTERY) return "LOW BATTERY";
    if (flags & SCUBA_NDL) return "NDL RUNNING LOW";
    if (flags & SCUBA_DEPTH) return "DEPTH LIMIT";
    return "DIVE TIME LIMIT";
}
static void refresh_menu(bool animate)
{
    char text[48];
    message(rows[0],model.salt_water ? "MARINE" : "FRESH",102);
    if (model.depth_alarm_dm) snprintf(text,sizeof(text),"DEPTH  %u m",model.depth_alarm_dm/10);
    else snprintf(text,sizeof(text),"DEPTH  OFF");
    message(rows[1],text,154);
    if (model.time_alarm_s) snprintf(text,sizeof(text),"TIME  %u min",model.time_alarm_s/60);
    else snprintf(text,sizeof(text),"TIME  OFF");
    message(rows[2],text,206);
    selection_fit(pill,rows[setting],NULL,animate);
    for (int i=0;i<3;i++) selection_row_color(rows[i],NULL,i==setting);
}
static void render(void)
{
    char text[120], time[24];
    bool settings=screen==SCUBA_SETTINGS;
    bool face=screen==SCUBA_COMPASS;
    hide(menu,!settings); hide(compass,!face); hide(content,settings||face);
    hide(depth_unit,screen!=SCUBA_MAIN);
    hide(arc,settings||face||screen==SCUBA_SETTING||screen==SCUBA_BRIGHTNESS||screen==SCUBA_STATS||screen==SCUBA_SUMMARY||screen==SCUBA_PREP);
    lv_obj_set_style_text_font(value,&watch_montserrat_60,0);
    lv_obj_set_style_text_font(metric,&lv_font_montserrat_28,0);
    lv_obj_set_style_text_font(detail,&lv_font_montserrat_18,0);
    lv_obj_set_style_arc_color(arc,lv_color_hex(0x00bcbc),LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc,lv_color_hex(0x252b30),LV_PART_MAIN);
    lv_arc_set_value(arc,0);
    message(title,"",36); message(value,"",86); message(metric,"",163);
    message(detail,"",208); message(hint,"",250);
    hide(brightness_bar,screen!=SCUBA_BRIGHTNESS);
    if (settings) {}
    else if (screen==SCUBA_PREP) {
        message(title,"SCUBA DIVING",42);
        lv_obj_set_style_text_font(value,&lv_font_montserrat_42,0);
        message(value,"READY",100);
        snprintf(text,sizeof(text),"AIR  |  %s",model.salt_water?"MARINE":"FRESH");
        message(metric,text,164);
        snprintf(text,sizeof(text),"Battery %u%%  |  Start at 2 m",model.battery);
        message(detail,text,211);
        message(hint,"Tap for dive settings",250);
    } else if (screen==SCUBA_MAIN) {
        bool surface=scuba_model_surface(&model);
        uint16_t ceiling=scuba_model_ceiling(&model);
        uint32_t target=scuba_model_stop_target(&model);
        bool safety=!surface && !ceiling && target && model.depth_dm<=60;
        message(title,surface?"SURFACE":ceiling?"DECOMPRESSION":safety?"SAFETY STOP":"",36);
        depth_reading(value,depth_unit,76,&watch_montserrat_60,&lv_font_montserrat_22);
        duration(time,sizeof(time),model.dive_ms/1000);
        message(metric,time,146);
        lv_obj_set_style_text_font(metric,&lv_font_montserrat_34,0);
        if (ceiling) {
            snprintf(text,sizeof(text),"CEILING %u m  |  TTS %u min",ceiling/10,(scuba_model_tts(&model)+59)/60);
            message(detail,text,194);
            duration(time,sizeof(time),(target-model.stop_ms+999)/1000);
            snprintf(text,sizeof(text),"STOP %s  %s",time,scuba_model_in_stop(&model)?"":"PAUSED");
            message(hint,text,231);
        } else if (safety) {
            duration(time,sizeof(time),(target-model.stop_ms+999)/1000);
            snprintf(text,sizeof(text),"STOP %s",time); message(detail,text,195);
            message(hint,model.stop_ms==target?"STOP COMPLETE":scuba_model_in_stop(&model)?"Stay between 3 and 6 m":"Return to stop zone",234);
        } else if (surface) {
            snprintf(text,sizeof(text),"END IN %u:%02u",(300-model.surface_ms/1000)/60,(300-model.surface_ms/1000)%60);
            message(detail,text,195); message(hint,"Scroll to leave",234);
        } else {
            snprintf(text,sizeof(text),model.deco_done?"DECO COMPLETE":"NDL %u min",(model.ndl_s+59)/60);
            message(detail,text,195);
            lv_obj_set_style_text_font(detail,&lv_font_montserrat_28,0);
            snprintf(text,sizeof(text),"%d°C  |  AIR",model.temperature); message(hint,text,238);
        }
        uint32_t progress=model.deco_done?1000:surface?0:target&&(ceiling||safety)?model.stop_ms*1000/target:LV_MIN(model.ndl_s*1000/model.ndl_budget_s,1000);
        lv_arc_set_value(arc,progress);
        uint32_t color=model.deco_done||(target&&model.stop_ms==target)?0x33cc77:ceiling?0xffa438:model.ndl_s<=300?0xffa438:0x00bcbc;
        lv_obj_set_style_arc_color(arc,lv_color_hex(color),LV_PART_INDICATOR);
        if (ceiling && model.depth_dm < ceiling) {
            lv_obj_set_style_arc_color(arc,lv_color_hex(0xff5555),LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(arc,lv_color_hex(0xff5555),LV_PART_MAIN);
        } else if (safety && !scuba_model_in_stop(&model) && model.stop_ms < target) {
            lv_obj_set_style_arc_color(arc,lv_color_hex(0xffa438),LV_PART_MAIN);
        }
    } else if (face) {
        compass_face_update(compass,heading);
        depth_reading(compass_depth,compass_unit,78,&lv_font_montserrat_34,&lv_font_montserrat_18);
        duration(time,sizeof(time),model.dive_ms/1000); message(compass_time,time,185);
    } else if (screen==SCUBA_STATS || screen==SCUBA_SUMMARY) {
        bool summary=screen==SCUBA_SUMMARY;
        const struct scuba_summary *r=&model.summary;
        message(title,summary?"DIVE SUMMARY":"DIVE STATS",40);
        lv_obj_set_style_text_font(value,&lv_font_montserrat_34,0);
        uint16_t peak=summary?r->max_dm:model.peak_dm;
        snprintf(text,sizeof(text),"MAX %u.%u m",peak/10,peak%10); message(value,text,95);
        uint16_t average=summary?r->average_dm:model.underwater_ms?model.depth_integral/model.underwater_ms:0;
        snprintf(text,sizeof(text),"AVG %u.%u m",average/10,average%10); message(metric,text,147);
        duration(time,sizeof(time),summary?r->duration_s:model.dive_ms/1000);
        snprintf(text,sizeof(text),"%s  |  MIN %d°C",time,summary?r->min_temperature:model.min_temperature); message(detail,text,199);
        message(hint,summary?(r->missed_stop?"Stop not completed":"Dive ended"):"AIR",246);
    } else if (screen==SCUBA_LEAVE) {
        message(title,"LEAVE",58);
        lv_obj_set_style_text_font(value,&lv_font_montserrat_24,0);
        message(value,"Turn the crown\nto leave",126);
        lv_arc_set_value(arc,leave_progress*1000/24);
        lv_obj_set_style_arc_color(arc,lv_color_white(),LV_PART_INDICATOR);
    } else if (screen==SCUBA_BRIGHTNESS) {
        message(title,"BRIGHTNESS",51);
        lv_bar_set_value(brightness_bar,brightness,LV_ANIM_OFF);
        message(hint,"Tap to confirm",220);
    } else if (screen==SCUBA_SETTING) {
        message(title,setting==1?"DEPTH ALARM":"DIVE TIME ALARM",51);
        uint32_t amount=setting==1?model.depth_alarm_dm/10:model.time_alarm_s/60;
        if(amount) snprintf(text,sizeof(text),"%u",amount); else {lv_obj_set_style_text_font(value,&lv_font_montserrat_42,0);snprintf(text,sizeof(text),"OFF");}
        message(value,text,100); message(metric,setting==1?"metres":"minutes",171);
        message(hint,"Tap to confirm",232);
    }
    bool alert=model.active && model.alerts;
    hide(title,alert);
    hide(alert_text,!alert); hide(alert_hint,!alert||!model.notifications);
    if(alert) {
        message(alert_text,model.notifications?alert_name(model.notifications):"!  ALERT ACTIVE",34);
        message(alert_hint,"Tap to acknowledge",266);
    }
}
static void show(enum scuba_screen next)
{
    bool changed=screen!=next;
    screen=next;
    if(next==SCUBA_SETTINGS) refresh_menu(false);
    render();
    if(changed) lv_obj_fade_in(root,180,0);
}
lv_obj_t *scuba_ui_create(lv_obj_t *parent)
{
    scuba_model_init(&model);
    root=lv_obj_create(parent); lv_obj_remove_style_all(root); lv_obj_set_size(root,320,300);
    lv_obj_set_style_bg_color(root,lv_color_black(),0); lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    arc=lv_arc_create(root);lv_obj_set_size(arc,296,296);lv_obj_set_pos(arc,12,2);
    lv_arc_set_bg_angles(arc,0,360);lv_arc_set_rotation(arc,90);lv_arc_set_range(arc,0,1000);
    lv_obj_set_style_arc_width(arc,8,LV_PART_MAIN);lv_obj_set_style_arc_width(arc,8,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc,lv_color_hex(0x252b30),LV_PART_MAIN);
    lv_obj_remove_style(arc,NULL,LV_PART_KNOB);lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICKABLE);
    content=lv_obj_create(root);lv_obj_remove_style_all(content);lv_obj_set_size(content,320,300);lv_obj_remove_flag(content,LV_OBJ_FLAG_SCROLLABLE);
    title=label(content,36,&lv_font_montserrat_18);value=label(content,86,&watch_montserrat_60);
    metric=label(content,163,&lv_font_montserrat_28);detail=label(content,208,&lv_font_montserrat_18);hint=label(content,250,&lv_font_montserrat_14);
    depth_unit=label(content,0,&lv_font_montserrat_22);
    lv_obj_set_style_text_color(hint,lv_color_hex(0xaaaaaa),0);
    menu=lv_obj_create(root);lv_obj_remove_style_all(menu);lv_obj_set_size(menu,320,300);lv_obj_remove_flag(menu,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *heading_label=label(menu,43,&lv_font_montserrat_22);message(heading_label,"DIVE SETTINGS",43);
    pill=lv_obj_create(menu);lv_obj_remove_style_all(pill);lv_obj_set_style_bg_color(pill,lv_color_hex(0x008b8b),0);lv_obj_set_style_bg_opa(pill,LV_OPA_COVER,0);
    for(int i=0;i<3;i++) rows[i]=label(menu,102+52*i,&lv_font_montserrat_22);
    lv_obj_t *help=label(menu,257,&lv_font_montserrat_12);message(help,"Tap to select | Double tap: back",257);
    compass=compass_face_create(root);
    compass_face_set_direction_visible(compass,false);
    compass_depth=label(compass,78,&lv_font_montserrat_34);
    compass_unit=label(compass,0,&lv_font_montserrat_18);
    compass_time=label(compass,185,&lv_font_montserrat_34);
    lv_obj_set_style_bg_color(compass_depth,lv_color_black(),0);lv_obj_set_style_bg_opa(compass_depth,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(compass_time,lv_color_black(),0);lv_obj_set_style_bg_opa(compass_time,LV_OPA_COVER,0);
    brightness_bar=lv_bar_create(root);lv_obj_set_size(brightness_bar,180,18);lv_obj_set_pos(brightness_bar,70,135);
    lv_bar_set_range(brightness_bar,0,10);lv_obj_set_style_bg_color(brightness_bar,lv_color_hex(0x00bcbc),LV_PART_INDICATOR);
    alert_text=label(root,16,&lv_font_montserrat_12);alert_hint=label(root,266,&lv_font_montserrat_12);
    lv_obj_set_style_text_color(alert_text,lv_color_hex(0xff6655),0);
    lv_obj_set_style_bg_color(alert_text,lv_color_black(),0);lv_obj_set_style_bg_opa(alert_text,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(alert_hint,lv_color_hex(0xffaa66),0);
    render(); return root;
}
void scuba_ui_enter(uint64_t now_ms)
{
    scuba_model_arm(&model,now_ms);leave_progress=0;
    show(model.active?SCUBA_MAIN:SCUBA_PREP);
}
void scuba_ui_update(const struct watch_data *data, uint64_t now_ms, bool visible)
{
    if(!visible) {model.last_ms=now_ms;return;}
    scuba_model_update(&model,now_ms,data->depth_dm,data->temperature_c,test_battery);
    heading=data->heading_degrees;
    if(model.active&&!last_active) {leave_progress=0; show(SCUBA_MAIN);}
    if(!model.active&&last_active) show(SCUBA_SUMMARY);
    if(model.active&&!scuba_model_surface(&model)&&(screen==SCUBA_LEAVE||screen==SCUBA_SETTINGS||screen==SCUBA_SETTING)) {leave_progress=0;show(SCUBA_MAIN);}
    last_active=model.active;
    uint32_t fresh=model.notifications & ~previous_notifications;
    if(fresh&&watch_ui_haptics()) {haptic_count++;last_haptic_ms=now_ms;}
    previous_notifications=model.notifications;
    render();
}
bool scuba_ui_input(enum watch_input input, int steps)
{
    bool surface=scuba_model_surface(&model);
    if(input==WATCH_SINGLE_TAP) {
        if(model.notifications) {scuba_model_acknowledge(&model); previous_notifications=0;}
        else if(screen==SCUBA_PREP) show(SCUBA_SETTINGS);
        else if(screen==SCUBA_SETTINGS) {
            if(!setting) {model.salt_water=!model.salt_water;refresh_menu(true);}
            else show(SCUBA_SETTING);
        } else if(screen==SCUBA_SETTING) show(SCUBA_SETTINGS);
        else if(screen==SCUBA_BRIGHTNESS) {watch_ui_set_brightness(brightness);show(brightness_origin);}
    } else if(input==WATCH_DOUBLE_TAP) {
        if(screen==SCUBA_SETTING) show(SCUBA_SETTINGS);
        else if(screen==SCUBA_SETTINGS) show(SCUBA_PREP);
        else if(screen==SCUBA_BRIGHTNESS) {watch_ui_set_brightness(brightness);show(brightness_origin);}
        else if(model.active) {brightness_origin=screen;brightness=watch_ui_brightness();show(SCUBA_BRIGHTNESS);}
    } else if(input==WATCH_CROWN && steps) {
        if(screen==SCUBA_BRIGHTNESS) {brightness=bounded_value(brightness,steps,10);watch_ui_set_brightness(brightness);}
        else if(screen==SCUBA_SETTING) {
            if(setting==1) model.depth_alarm_dm=bounded_value(model.depth_alarm_dm/10,steps,100)*10;
            else model.time_alarm_s=bounded_value(model.time_alarm_s/60,steps,180)*60;
        } else if(screen==SCUBA_SETTINGS) {setting=bounded_value(setting,steps,2);refresh_menu(true);return false;}
        else if(screen==SCUBA_LEAVE) {
            if(!surface) {leave_progress=0;show(SCUBA_STATS);}
            else if(steps<0&&!leave_progress) show(model.active?SCUBA_STATS:model.summary.valid?SCUBA_SUMMARY:SCUBA_PREP);
            else {
                leave_progress=bounded_value(leave_progress,steps,24);
                if(leave_progress==24) {scuba_model_finish(&model);model.armed=false;last_active=false;leave_progress=0;return true;}
            }
        } else if(!model.active) {
            int index=screen==SCUBA_SUMMARY?1:0;
            index=bounded_value(index,steps,model.summary.valid?2:1);
            show(index==0?SCUBA_PREP:index==1&&model.summary.valid?SCUBA_SUMMARY:SCUBA_LEAVE);
        } else {
            int index=screen==SCUBA_COMPASS?1:screen==SCUBA_STATS?2:0;
            index=bounded_value(index,steps,surface?3:2);
            show(index==0?SCUBA_MAIN:index==1?SCUBA_COMPASS:index==2?SCUBA_STATS:SCUBA_LEAVE);
        }
    }
    render();return false;
}
void scuba_ui_set_scenario(enum scuba_scenario scenario) {scuba_model_scenario(&model,scenario,k_uptime_get());last_active=false;show(SCUBA_PREP);}
void scuba_ui_set_speed(uint8_t speed) {if(speed==1||speed==10||speed==60) model.speed=speed;}
void scuba_ui_manual(void) {model.scenario=SCUBA_MANUAL;}
void scuba_ui_set_battery(uint8_t battery) {test_battery=battery;}
const struct scuba_model *scuba_ui_model(void) {return &model;}
enum scuba_screen scuba_ui_screen(void) {return screen;}
uint32_t scuba_ui_haptic_count(void) {return haptic_count;}
bool scuba_ui_haptic_active(void) {return haptic_count && k_uptime_get()-last_haptic_ms < (watch_ui_pulse()==PULSE_SHORT?120:watch_ui_pulse()==PULSE_LONG?600:300)?haptic_count:0;}
