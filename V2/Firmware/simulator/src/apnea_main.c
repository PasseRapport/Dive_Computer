#include "apnea_main.h"
#include "apnea_settings.h"
#include "dive_layout.h"
#include "native_fonts.h"
#include <stdio.h>

struct apnea_main {
    lv_obj_t *depth,*unit,*left_title,*left,*time_title,*time,*bottom_title,*bottom,*direction;
    int8_t heading;
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
    lv_area_t stroke={a.x1+9,a.y1+9,a.x1+11,a.y1+47};
    lv_draw_rect(layer,&ink,&stroke);
    if(!m->heading)return;
    for(int row=0;row<9;row++) {
        int y=m->heading<0?row:56-row;
        lv_area_t left={a.x1+9-row,a.y1+y,a.x1+11-row,a.y1+y};
        lv_area_t right={a.x1+9+row,a.y1+y,a.x1+11+row,a.y1+y};
        lv_draw_rect(layer,&ink,&left);lv_draw_rect(layer,&ink,&right);
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
    lv_obj_set_pos(m->direction,150,145);lv_obj_set_size(m->direction,21,57);
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
    centred(m->time_title,data->apnea_active?"APNEA TIME":"RECOVERY",219,127);
    uint32_t seconds=data->apnea_active?data->elapsed_s:data->recovery_s;
    if(data->apnea_active||data->show_recovery)snprintf(text,sizeof(text),"%u:%02u",seconds/60,seconds%60);
    else snprintf(text,sizeof(text),"--");
    lv_obj_set_style_text_font(m->time,seconds>=6000?&lv_font_montserrat_22:&lv_font_montserrat_34,0);
    centred(m->time,text,219,157);
    lv_obj_set_style_text_color(m->time,lv_color_hex(data->show_recovery?(data->recovery_red?0xff0000:0x00ff00):0xffffff),0);
    enum apnea_bottom_field field=apnea_settings_bottom_field();
    centred(m->bottom_title,field==APNEA_SESSION_DEPTH?"SESSION DEPTH":field==APNEA_SESSION_TIME?"SESSION APNEA":"TEMPERATURE",160,219);
    if(field!=APNEA_TEMPERATURE&&!data->total_dives)snprintf(text,sizeof(text),"--");
    else if(field==APNEA_SESSION_DEPTH)snprintf(text,sizeof(text),"%u.%u m",data->max_depth_dm/10,data->max_depth_dm%10);
    else if(field==APNEA_SESSION_TIME)snprintf(text,sizeof(text),"%u:%02u",data->max_dive_s/60,data->max_dive_s%60);
    else snprintf(text,sizeof(text),"%d°C",data->temperature_c);
    lv_obj_set_style_text_font(m->bottom,field==APNEA_SESSION_TIME&&data->max_dive_s>=6000?&lv_font_montserrat_22:&lv_font_montserrat_34,0);
    centred(m->bottom,text,160,239);
    m->heading=data->apnea_active?data->apnea_direction:0;lv_obj_invalidate(m->direction);
}
