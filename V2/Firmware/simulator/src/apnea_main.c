#include "apnea_main.h"
#include "apnea_settings.h"
#include "dive_layout.h"
#include "native_fonts.h"
#include <stdio.h>

struct apnea_main {
    lv_obj_t *depth,*unit,*left_title,*left,*time_title,*time,*bottom_title,*bottom,*direction;
};
static void deleted(lv_event_t *event){lv_free(lv_event_get_user_data(event));}
static lv_obj_t *label(lv_obj_t *parent,const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(parent);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_white(),0);
    return o;
}
static void centred(lv_obj_t *o,const char *text,int x,int y)
{
    lv_label_set_text(o,text);lv_obj_update_layout(o);
    lv_obj_set_pos(o,x-lv_obj_get_width(o)/2,y);
}
static void direction(lv_event_t *event)
{
    struct apnea_main *m=lv_event_get_user_data(event);
    lv_area_t a;lv_obj_get_coords(m->direction,&a);
    lv_draw_rect_dsc_t ink;lv_draw_rect_dsc_init(&ink);
    ink.bg_color=lv_color_white();ink.bg_opa=LV_OPA_COVER;
    lv_layer_t *layer=lv_event_get_layer(event);
    /* Same five inactive dashes, positions, dimensions and colour as scuba. */
    ink.bg_color=lv_color_hex(0x777777);
    for(int i=0;i<5;i++) {
        lv_area_t dash={a.x1+7,a.y1+3+12*i,a.x1+12,a.y1+5+12*i};
        lv_draw_rect(layer,&ink,&dash);
    }
}
lv_obj_t *apnea_main_create(lv_obj_t *parent)
{
    lv_obj_t *view=lv_obj_create(parent);lv_obj_remove_style_all(view);
    lv_obj_set_size(view,320,300);lv_obj_remove_flag(view,LV_OBJ_FLAG_SCROLLABLE);
    struct apnea_main *m=lv_malloc_zeroed(sizeof(*m));LV_ASSERT_MALLOC(m);
    lv_obj_set_user_data(view,m);lv_obj_add_event_cb(view,deleted,LV_EVENT_DELETE,m);
    m->depth=label(view,&watch_montserrat_60);m->unit=label(view,&lv_font_montserrat_18);lv_label_set_text(m->unit,"m");
    m->left_title=label(view,&lv_font_montserrat_12);m->left=label(view,&lv_font_montserrat_34);
    m->time_title=label(view,&lv_font_montserrat_12);m->time=label(view,&lv_font_montserrat_34);
    m->bottom_title=label(view,&lv_font_montserrat_12);m->bottom=label(view,&lv_font_montserrat_34);
    dive_layout_rails(view,0x33dbd4);
    m->direction=lv_obj_create(view);lv_obj_remove_style_all(m->direction);
    lv_obj_set_pos(m->direction,150,146);lv_obj_set_size(m->direction,20,56);
    lv_obj_remove_flag(m->direction,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m->direction,direction,LV_EVENT_DRAW_MAIN,m);
    return view;
}
void apnea_main_update(lv_obj_t *view,const struct watch_data *data)
{
    struct apnea_main *m=lv_obj_get_user_data(view);char text[48];
    dive_layout_depth(m->depth,m->unit,data->depth_dm);
    centred(m->left_title,data->apnea_active?"MAX DEPTH":"LAST DEPTH",101,127);
    uint16_t peak=data->apnea_active?data->current_max_depth_dm:data->last_depth_dm;
    if(data->apnea_active||data->show_recovery)snprintf(text,sizeof(text),"%u.%u",peak/10,peak%10);
    else snprintf(text,sizeof(text),"--");
    lv_obj_set_style_text_font(m->left,peak>=1000?&lv_font_montserrat_28:&lv_font_montserrat_34,0);
    centred(m->left,text,101,157);
    centred(m->time_title,data->apnea_active?"APNEA TIME":"LAST APNEA",219,127);
    uint32_t seconds=data->elapsed_s;
    if(data->apnea_active||data->show_recovery)
        snprintf(text,sizeof(text),"%u:%02u",seconds/60,seconds%60);
    else snprintf(text,sizeof(text),"--");
    lv_obj_set_style_text_font(m->time,seconds>=6000?&lv_font_montserrat_22:&lv_font_montserrat_34,0);
    centred(m->time,text,219,157);
    enum apnea_bottom_field field=apnea_settings_bottom_field();
    static const char *const captions[]={"TEMPERATURE","SESSION DEPTH","UNDERWATER TIME","CLOCK","APNEAS"};
    centred(m->bottom_title,captions[field],160,219);
    switch(field) {
    case APNEA_SESSION_DEPTH:
        if(data->total_dives)snprintf(text,sizeof(text),"%u.%u m",data->max_depth_dm/10,data->max_depth_dm%10);
        else snprintf(text,sizeof(text),"--");
        break;
    case APNEA_SESSION_TIME:
        seconds=data->total_underwater_s;
        snprintf(text,sizeof(text),"%u:%02u:%02u",seconds/3600,seconds/60%60,seconds%60);break;
    case APNEA_CLOCK:snprintf(text,sizeof(text),"%02u:%02u",data->clock_hour,data->clock_minute);break;
    case APNEA_DIVE_COUNT:snprintf(text,sizeof(text),"%u",data->total_dives);break;
    default:snprintf(text,sizeof(text),"%d°C",data->temperature_c);break;
    }
    lv_obj_set_style_text_font(m->bottom,field==APNEA_SESSION_TIME&&data->total_underwater_s>=36000?&lv_font_montserrat_22:&lv_font_montserrat_34,0);
    centred(m->bottom,text,160,239);
}
