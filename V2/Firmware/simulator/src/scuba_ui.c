#include "scuba_ui.h"
#include "scuba_dashboard.h"
#include "selection.h"
#include "native_fonts.h"
#include "v1_ui_layout.h"
#include "light_editor.h"
#include <zephyr/kernel.h>
#include <stdio.h>

LV_IMAGE_DECLARE(icon_drop);
LV_IMAGE_DECLARE(icon_wave);
static struct scuba_model model;
static enum scuba_screen screen, brightness_origin;
static lv_obj_t *root, *content, *arc, *title, *value, *metric, *detail, *hint;
static lv_obj_t *menu, *pill, *rows[3];
static lv_obj_t *alert_text, *brightness_editor, *sensitivity_editor;
static int setting, leave_progress, water_row;
static lv_obj_t *water_icons[2], *prep_alarm[2];
static uint8_t test_battery=87, brightness;
static uint32_t haptic_count, previous_notifications;
static uint64_t last_haptic_ms;
static bool last_active;
static int display_choice, options_row, backlight_row;
static lv_obj_t *choice_view, *choice_pill, *choice_anchor, *choices[SCUBA_FIELD_COUNT+1];
static lv_obj_t *menu_title, *menu_help;
static lv_obj_t *dashboard;
static struct watch_data live_data;

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
    if (flags & SCUBA_OXYGEN) return "HIGH ppO2";
    if (flags & SCUBA_DECOMPRESSION) return "DECOMPRESSION";
    if (flags & SCUBA_NDL) return "NDL RUNNING LOW";
    if (flags & SCUBA_DEPTH) return "DEPTH LIMIT";
    return "DIVE TIME LIMIT";
}
static bool selector(void)
{
    return screen==SCUBA_RING_SELECT||screen==SCUBA_FIELD_SELECT;
}
static bool options_menu(void)
{
    return screen==SCUBA_OPTIONS||screen==SCUBA_BACKLIGHT||screen==SCUBA_WATER||selector();
}
static bool carousel(void) {return selector()||screen==SCUBA_OPTIONS||screen==SCUBA_SETTINGS||screen==SCUBA_BACKLIGHT;}
static const char *choice_name(int index)
{
    static const char *const options[]={"Ring","Bottom field","Backlight","Back"};
    if(screen==SCUBA_BACKLIGHT) {
        static const char *const modes[]={"ON","OFF","AUTO"};
        return index==1?"Brightness":index==0?modes[watch_ui_backlight_mode()]:index==2?"Sensitivity":"Back";
    }
    if(screen==SCUBA_RING_SELECT&&index==SCUBA_RING_COUNT)return "Back";
    if(screen==SCUBA_FIELD_SELECT&&index==SCUBA_FIELD_COUNT)return "Back";
    if(screen==SCUBA_SETTINGS) {
        static char value[48];if(index==0)return "Water Type";
        if(index==4)return "Back";
        if(index==3){snprintf(value,sizeof(value),"START %u.%u m",model.start_depth_dm/10,model.start_depth_dm%10);return value;}
        unsigned amount=index==1?model.depth_alarm_dm/10:model.time_alarm_s/60;
        if(amount)snprintf(value,sizeof(value),index==1?"DEPTH %u m":"TIME %u min",amount);
        else snprintf(value,sizeof(value),index==1?"DEPTH OFF":"TIME OFF");
        return value;
    }
    return screen==SCUBA_OPTIONS?options[index]:screen==SCUBA_RING_SELECT?scuba_ring_name(index):scuba_field_name(index);
}
static void choice_y(void *object,int32_t y) {lv_obj_set_y(object,y);}
static void refresh_choices(bool animate)
{
    int count=screen==SCUBA_SETTINGS?5:screen==SCUBA_OPTIONS||screen==SCUBA_BACKLIGHT?4:screen==SCUBA_RING_SELECT?SCUBA_RING_COUNT+1:SCUBA_FIELD_COUNT+1;
    int selected=screen==SCUBA_SETTINGS?setting:screen==SCUBA_OPTIONS?options_row:screen==SCUBA_BACKLIGHT?backlight_row:display_choice;
    for(int i=0;i<SCUBA_FIELD_COUNT+1;i++) {
        lv_anim_delete(choices[i],choice_y);
        hide(choices[i],i>=count);
        if(i>=count)continue;
        lv_label_set_text(choices[i],choice_name(i));
        lv_obj_set_align(choices[i],LV_ALIGN_TOP_MID);
        int target=64+52*(i-selected);
        if(animate) {
            lv_anim_t animation;lv_anim_init(&animation);
            lv_anim_set_var(&animation,choices[i]);
            lv_anim_set_values(&animation,lv_obj_get_y(choices[i]),target);
            lv_anim_set_duration(&animation,250);
            lv_anim_set_path_cb(&animation,lv_anim_path_ease_in_out);
            lv_anim_set_exec_cb(&animation,choice_y);lv_anim_start(&animation);
        } else lv_obj_set_y(choices[i],target);
        selection_row_color(choices[i],NULL,i==selected);
    }
    /* Invisible measuring label keeps the capsule centred while text moves. */
    message(choice_anchor,choice_name(selected),64);
    selection_fit(choice_pill,choice_anchor,NULL,animate);
}
static void refresh_options_menu(bool animate)
{
    message(menu_title,screen==SCUBA_RING_SELECT?"RING":screen==SCUBA_FIELD_SELECT?"BOTTOM FIELD":screen==SCUBA_OPTIONS?"OPTIONS":screen==SCUBA_SETTINGS?"DIVE SETTINGS":screen==SCUBA_WATER?"WATER TYPE":"BACKLIGHT",43);
    message(menu_help,"Tap to select",257);
    hide(choice_view,!carousel());
    hide(pill,carousel());
    for(int i=0;i<3;i++)hide(rows[i],carousel());
    for(int i=0;i<2;i++)hide(water_icons[i],screen!=SCUBA_WATER);
    if(carousel()) {refresh_choices(animate);return;}
    if(screen==SCUBA_WATER) {
        static const char *const names[]={"FRESH","MARINE","Back"};
        for(int i=0;i<3;i++){
            lv_label_set_text(rows[i],names[i]);
            lv_obj_set_align(rows[i],LV_ALIGN_TOP_LEFT);lv_obj_set_pos(rows[i],i<2?130:133,100+56*i);
            selection_row_color(rows[i],i<2?water_icons[i]:NULL,i==water_row);
        }
        selection_fit(pill,rows[water_row],water_row<2?water_icons[water_row]:NULL,animate);return;
    }
    static const char *const modes[]={"ON","OFF","AUTO"};
    for(int i=0;i<3;i++) {
        message(rows[i],i==1?"Brightness":i==0?modes[watch_ui_backlight_mode()]:"Sensitivity",102+52*i);
        selection_row_color(rows[i],NULL,i==backlight_row);
    }
    selection_fit(pill,rows[backlight_row],NULL,animate);
}
static void render(void)
{
    char text[120], time[24];
    bool settings=screen==SCUBA_SETTINGS;
    bool display=options_menu();
    hide(menu,!(settings||display)); hide(content,settings||display||screen==SCUBA_MAIN||screen==SCUBA_BRIGHTNESS||screen==SCUBA_SENSITIVITY);
    hide(dashboard,screen!=SCUBA_MAIN);
    hide(arc,screen==SCUBA_MAIN||settings||display||screen==SCUBA_SETTING||screen==SCUBA_BRIGHTNESS||screen==SCUBA_SENSITIVITY||screen==SCUBA_OPTIONS_GATE||screen==SCUBA_SUMMARY||screen==SCUBA_PREP);
    lv_obj_set_style_text_font(value,&watch_montserrat_60,0);
    lv_obj_set_style_text_font(metric,&lv_font_montserrat_28,0);
    lv_obj_set_style_text_font(detail,&lv_font_montserrat_18,0);
    lv_obj_set_style_arc_color(arc,lv_color_hex(0x00bcbc),LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc,lv_color_hex(0x252b30),LV_PART_MAIN);
    lv_arc_set_value(arc,0);
    message(title,"",36); message(value,"",86); message(metric,"",163);
    message(detail,"",208); message(hint,"",250);
    hide(brightness_editor,screen!=SCUBA_BRIGHTNESS);
    hide(sensitivity_editor,screen!=SCUBA_SENSITIVITY);
    for(int i=0;i<2;i++)hide(prep_alarm[i],screen!=SCUBA_PREP);
    if (settings||display) {}
    else if (screen==SCUBA_PREP) {
        message(title,"SCUBA DIVING",30);
        lv_obj_set_style_text_font(value,&lv_font_montserrat_34,0);
        message(value,"READY",65);
        lv_obj_set_style_text_font(metric,&lv_font_montserrat_18,0);
        lv_obj_set_style_text_font(detail,&lv_font_montserrat_16,0);
        snprintf(text,sizeof(text),"AIR  |  %s",model.salt_water?"MARINE":"FRESH");
        message(metric,text,117);
        snprintf(text,sizeof(text),"Battery %u%%  |  Start %u.%u m",model.battery,model.start_depth_dm/10,model.start_depth_dm%10);
        message(detail,text,150);
        if(model.depth_alarm_dm)snprintf(text,sizeof(text),"Depth alarm: %u m",model.depth_alarm_dm/10);
        else snprintf(text,sizeof(text),"Depth alarm: OFF");
        message(prep_alarm[0],text,182);
        if(model.time_alarm_s)snprintf(text,sizeof(text),"Time alarm: %u min",model.time_alarm_s/60);
        else snprintf(text,sizeof(text),"Time alarm: OFF");
        message(prep_alarm[1],text,214);
        message(hint,"Tap for dive settings",250);
    } else if (screen==SCUBA_OPTIONS_GATE) {
        message(title,"OPTIONS",103);
        message(hint,"Tap to enter",176);
    } else if (screen==SCUBA_MAIN) {
        scuba_dashboard_update(dashboard,&model,&live_data);
    } else if (screen==SCUBA_SUMMARY) {
        const struct scuba_summary *r=&model.summary;
        message(title,"DIVE SUMMARY",40);
        lv_obj_set_style_text_font(value,&lv_font_montserrat_34,0);
        uint16_t peak=r->max_dm;
        snprintf(text,sizeof(text),"MAX %u.%u m",peak/10,peak%10); message(value,text,95);
        uint16_t average=r->average_dm;
        snprintf(text,sizeof(text),"AVG %u.%u m",average/10,average%10); message(metric,text,147);
        duration(time,sizeof(time),r->duration_s);
        snprintf(text,sizeof(text),"DURATION %s",time); message(detail,text,195);
        duration(time,sizeof(time),model.surface_ms/1000);
        snprintf(text,sizeof(text),"SURFACE TIME %s",time); message(hint,text,238);lv_obj_set_style_text_color(hint,lv_color_white(),0);
    } else if (screen==SCUBA_LEAVE) {
        message(title,"LEAVE",58);
        lv_obj_set_style_text_font(value,&lv_font_montserrat_24,0);
        message(value,"Turn the crown\nto leave",126);
        lv_arc_set_value(arc,leave_progress*1000/24);
        lv_obj_set_style_arc_color(arc,lv_color_white(),LV_PART_INDICATOR);
    } else if (screen==SCUBA_BRIGHTNESS) {
        light_editor_set_value(brightness_editor,brightness);
    } else if (screen==SCUBA_SENSITIVITY) {
        light_editor_set_value(sensitivity_editor,watch_ui_sensitivity());
    } else if (screen==SCUBA_SETTING) {
        message(title,setting==1?"DEPTH ALARM":setting==3?"START DEPTH":"DIVE TIME ALARM",51);
        uint32_t amount=setting==1?model.depth_alarm_dm/10:model.time_alarm_s/60;
        if(setting==3)snprintf(text,sizeof(text),"%u.%u",model.start_depth_dm/10,model.start_depth_dm%10);
        else if(amount) snprintf(text,sizeof(text),"%u",amount); else {lv_obj_set_style_text_font(value,&lv_font_montserrat_42,0);snprintf(text,sizeof(text),"OFF");}
        message(value,text,100); message(metric,setting==2?"minutes":"metres",171);
        message(hint,"Tap to confirm",232);
    }
    /* Low battery is represented by the coloured icon, without a text overlay.
     * Keep the model alert and haptic signal; show other warnings normally. */
    uint32_t visible_notifications=model.notifications&~SCUBA_BATTERY;
    bool alert=model.active && visible_notifications;
    hide(title,alert&&screen!=SCUBA_OPTIONS_GATE);
    hide(alert_text,!alert);
    if(alert) {
        message(alert_text,alert_name(visible_notifications),34);

    }
}
static void show(enum scuba_screen next)
{
    bool changed=screen!=next;
    screen=next;
    if(next==SCUBA_SETTINGS||options_menu())refresh_options_menu(false);
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
    lv_obj_remove_style(arc,NULL,LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_border_width(arc,0,LV_PART_KNOB);lv_obj_set_style_shadow_width(arc,0,LV_PART_KNOB);
    lv_obj_set_style_arc_rounded(arc,false,LV_PART_MAIN);lv_obj_set_style_arc_rounded(arc,true,LV_PART_INDICATOR);lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICKABLE);
    content=lv_obj_create(root);lv_obj_remove_style_all(content);lv_obj_set_size(content,320,300);lv_obj_remove_flag(content,LV_OBJ_FLAG_SCROLLABLE);
    title=label(content,36,&lv_font_montserrat_18);value=label(content,86,&watch_montserrat_60);
    metric=label(content,163,&lv_font_montserrat_28);detail=label(content,208,&lv_font_montserrat_18);hint=label(content,250,&lv_font_montserrat_14);
    lv_obj_set_style_text_color(hint,lv_color_hex(0xaaaaaa),0);
    menu=lv_obj_create(root);lv_obj_remove_style_all(menu);lv_obj_set_size(menu,320,300);lv_obj_remove_flag(menu,LV_OBJ_FLAG_SCROLLABLE);
    menu_title=label(menu,43,&lv_font_montserrat_22);message(menu_title,"DIVE SETTINGS",43);
    pill=lv_obj_create(menu);lv_obj_remove_style_all(pill);lv_obj_set_style_bg_color(pill,lv_color_hex(0x008b8b),0);lv_obj_set_style_bg_opa(pill,LV_OPA_COVER,0);
    for(int i=0;i<3;i++) rows[i]=label(menu,102+52*i,&lv_font_montserrat_22);
    water_icons[0]=lv_image_create(menu);lv_image_set_src(water_icons[0],&icon_drop);lv_obj_set_pos(water_icons[0],76,90);
    water_icons[1]=lv_image_create(menu);lv_image_set_src(water_icons[1],&icon_wave);lv_obj_set_pos(water_icons[1],76,146);
    for(int i=0;i<2;i++){lv_obj_set_style_image_recolor_opa(water_icons[i],LV_OPA_COVER,0);hide(water_icons[i],true);}
    menu_help=label(menu,257,&lv_font_montserrat_12);message(menu_help,"Tap to select",257);
    choice_view=lv_obj_create(menu);lv_obj_remove_style_all(choice_view);
    lv_obj_set_size(choice_view,320,155);lv_obj_set_pos(choice_view,0,73);
    lv_obj_remove_flag(choice_view,LV_OBJ_FLAG_SCROLLABLE);
    choice_pill=lv_obj_create(choice_view);lv_obj_remove_style_all(choice_pill);
    lv_obj_set_style_bg_color(choice_pill,lv_color_hex(0x008b8b),0);
    lv_obj_set_style_bg_opa(choice_pill,LV_OPA_COVER,0);
    choice_anchor=label(choice_view,64,&lv_font_montserrat_22);
    lv_obj_set_style_opa(choice_anchor,LV_OPA_TRANSP,0);
    for(int i=0;i<SCUBA_FIELD_COUNT+1;i++)choices[i]=label(choice_view,64,&lv_font_montserrat_22);
    hide(choice_view,true);
    dashboard=scuba_dashboard_create(root);
    brightness_editor=light_editor_create(root,"BRIGHTNESS",10);
    sensitivity_editor=light_editor_create(root,"SENSITIVITY",100);
    alert_text=label(root,16,&lv_font_montserrat_12);
    lv_obj_set_style_text_color(alert_text,lv_color_hex(0xff6655),0);
    lv_obj_set_style_bg_color(alert_text,lv_color_black(),0);lv_obj_set_style_bg_opa(alert_text,LV_OPA_COVER,0);
    for(int i=0;i<2;i++)prep_alarm[i]=label(content,182+32*i,&lv_font_montserrat_16);
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
    scuba_model_update_mm(&model,now_ms,data->depth_mm_valid?data->depth_mm:(uint32_t)data->depth_dm*100,data->temperature_c,test_battery);
    live_data=*data;
    if(model.active&&!last_active) {leave_progress=0; show(SCUBA_MAIN);}
    if(!model.active&&last_active) show(SCUBA_SUMMARY);
    if(model.active&&!scuba_model_surface(&model)&&(screen==SCUBA_LEAVE||screen==SCUBA_SETTINGS||screen==SCUBA_SETTING||screen==SCUBA_WATER)) {leave_progress=0;show(SCUBA_MAIN);}
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
        if(model.notifications&~SCUBA_BATTERY) {scuba_model_acknowledge(&model); previous_notifications=0;}
        else if(screen==SCUBA_PREP) show(SCUBA_SETTINGS);
        else if(screen==SCUBA_SETTINGS) {
            if(setting==4)show(SCUBA_PREP);
            else if(!setting) {water_row=model.salt_water?1:0;show(SCUBA_WATER);}
            else show(SCUBA_SETTING);
        } else if(screen==SCUBA_WATER) {if(water_row<2)model.salt_water=water_row==1;show(SCUBA_SETTINGS);}
        else if(screen==SCUBA_SETTING) show(SCUBA_SETTINGS);
        else if(screen==SCUBA_BRIGHTNESS) {watch_ui_set_brightness(brightness);show(brightness_origin);}
        else if(screen==SCUBA_OPTIONS_GATE) {options_row=0;show(SCUBA_OPTIONS);}
        else if(screen==SCUBA_OPTIONS) {
            if(options_row==3)show(model.active?SCUBA_MAIN:model.summary.valid?SCUBA_SUMMARY:SCUBA_PREP);
            else if(options_row==2) {backlight_row=0;show(SCUBA_BACKLIGHT);}
            else {
                display_choice=options_row==0?(int)scuba_dashboard_ring(dashboard):(int)scuba_dashboard_field(dashboard);
                if(options_row==1&&display_choice>=SCUBA_FIELD_COUNT)display_choice=0;
                show(options_row==0?SCUBA_RING_SELECT:SCUBA_FIELD_SELECT);
            }
        } else if(screen==SCUBA_BACKLIGHT) {
            if(backlight_row==3)show(SCUBA_OPTIONS);
            else if(backlight_row==0) {
                watch_ui_set_backlight_mode((watch_ui_backlight_mode()+1)%3);
                refresh_options_menu(true);
            } else if(backlight_row==1) {
                brightness_origin=SCUBA_BACKLIGHT;brightness=watch_ui_brightness();show(SCUBA_BRIGHTNESS);
            } else show(SCUBA_SENSITIVITY);
        } else if(screen==SCUBA_SENSITIVITY)show(SCUBA_BACKLIGHT);
        else if(screen==SCUBA_RING_SELECT||screen==SCUBA_FIELD_SELECT) {
            if(screen==SCUBA_RING_SELECT&&display_choice<SCUBA_RING_COUNT)scuba_dashboard_set_ring(dashboard,display_choice);
            else if(screen==SCUBA_FIELD_SELECT&&display_choice<SCUBA_FIELD_COUNT)scuba_dashboard_set_field(dashboard,display_choice);
            show(SCUBA_OPTIONS);
        }
    } else if(input==WATCH_DOUBLE_TAP) {
        if(screen==SCUBA_SETTING||screen==SCUBA_WATER) show(SCUBA_SETTINGS);
        else if(screen==SCUBA_SETTINGS) show(SCUBA_PREP);
        else if(screen==SCUBA_RING_SELECT||screen==SCUBA_FIELD_SELECT) show(SCUBA_OPTIONS);
        else if(screen==SCUBA_BACKLIGHT)show(SCUBA_OPTIONS);
        else if(screen==SCUBA_OPTIONS)show(model.active?SCUBA_MAIN:model.summary.valid?SCUBA_SUMMARY:SCUBA_PREP);
        else if(screen==SCUBA_SENSITIVITY)show(SCUBA_BACKLIGHT);
        else if(screen==SCUBA_BRIGHTNESS) {watch_ui_set_brightness(brightness);show(brightness_origin);}
        else if(model.active) {brightness_origin=screen;brightness=watch_ui_brightness();show(SCUBA_BRIGHTNESS);}
    } else if(input==WATCH_CROWN && steps) {
        if(screen==SCUBA_BRIGHTNESS) {brightness=bounded_value(brightness,steps,10);watch_ui_set_brightness(brightness);}
        else if(screen==SCUBA_SENSITIVITY)watch_ui_set_sensitivity(bounded_value(watch_ui_sensitivity(),steps*10,100));
        else if(screen==SCUBA_SETTING) {
            if(setting==1) model.depth_alarm_dm=bounded_value(model.depth_alarm_dm/10,steps,100)*10;
            else if(setting==3)model.start_depth_dm=LV_CLAMP(5,(int64_t)model.start_depth_dm+steps*5,100);
            else model.time_alarm_s=bounded_value(model.time_alarm_s/60,steps,180)*60;
        } else if(screen==SCUBA_SETTINGS) {setting=bounded_value(setting,steps,4);refresh_options_menu(true);return false;}
        else if(screen==SCUBA_WATER){water_row=bounded_value(water_row,steps,2);refresh_options_menu(true);return false;}
        else if(options_menu()) {
            if(screen==SCUBA_OPTIONS)options_row=bounded_value(options_row,steps,3);
            else if(screen==SCUBA_BACKLIGHT)backlight_row=bounded_value(backlight_row,steps,3);
            else display_choice=bounded_value(display_choice,steps,screen==SCUBA_RING_SELECT?SCUBA_RING_COUNT:SCUBA_FIELD_COUNT);
            refresh_options_menu(true);return false;
        } else if(screen==SCUBA_LEAVE) {
            if(!surface) {leave_progress=0;show(SCUBA_MAIN);}
            else if(steps<0&&!leave_progress) show(SCUBA_OPTIONS_GATE);
            else {
                leave_progress=bounded_value(leave_progress,steps,24);
                if(leave_progress==24) {scuba_model_finish(&model);model.armed=false;last_active=false;leave_progress=0;return true;}
            }
        } else {
            /* The top-level crown only changes pages. Menus require a tap. */
            int index=screen==SCUBA_OPTIONS_GATE?1:0;
            index=bounded_value(index,steps,surface?2:1);
            show(index==1?SCUBA_OPTIONS_GATE:index==2?SCUBA_LEAVE:
                model.active?SCUBA_MAIN:model.summary.valid?SCUBA_SUMMARY:SCUBA_PREP);
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

enum scuba_field scuba_ui_field(void) {return scuba_dashboard_field(dashboard);}

enum scuba_ring scuba_ui_ring(void) {return scuba_dashboard_ring(dashboard);}
