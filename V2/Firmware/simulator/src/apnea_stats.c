#include "apnea_stats.h"
#include "dive_layout.h"
#include "native_fonts.h"
#include <stdio.h>

struct apnea_stats {lv_obj_t *depth,*unit,*longest,*count,*runtime;};
static void deleted(lv_event_t *event){lv_free(lv_event_get_user_data(event));}
static void centred(lv_obj_t *o,const char *text,int x,int y)
{
    lv_label_set_text(o,text);lv_obj_update_layout(o);lv_obj_set_pos(o,x-lv_obj_get_width(o)/2,y);
}
static lv_obj_t *label(lv_obj_t *parent,const lv_font_t *font,const char *text,int x,int y)
{
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_white(),0);centred(o,text,x,y);return o;
}
lv_obj_t *apnea_stats_create(lv_obj_t *parent)
{
    lv_obj_t *view=lv_obj_create(parent);lv_obj_remove_style_all(view);
    lv_obj_set_size(view,320,300);lv_obj_remove_flag(view,LV_OBJ_FLAG_SCROLLABLE);
    struct apnea_stats *s=lv_malloc_zeroed(sizeof(*s));LV_ASSERT_MALLOC(s);
    lv_obj_set_user_data(view,s);lv_obj_add_event_cb(view,deleted,LV_EVENT_DELETE,s);
    s->depth=label(view,&watch_montserrat_60,"--",160,47);
    s->unit=label(view,&lv_font_montserrat_18,"m",0,0);
    label(view,&lv_font_montserrat_12,"MAX TIME",101,127);
    s->longest=label(view,&lv_font_montserrat_34,"--",101,157);
    label(view,&lv_font_montserrat_12,"DIVES",219,127);
    s->count=label(view,&lv_font_montserrat_34,"0",219,157);
    label(view,&lv_font_montserrat_12,"SESSION TIME",160,219);
    s->runtime=label(view,&lv_font_montserrat_34,"0:00:00",160,239);
    dive_layout_rails(view,0xffa438);
    return view;
}
void apnea_stats_update(lv_obj_t *view,const struct watch_data *data)
{
    struct apnea_stats *s=lv_obj_get_user_data(view);char text[48];
    if(data->total_dives) {
        lv_obj_remove_flag(s->unit,LV_OBJ_FLAG_HIDDEN);
        dive_layout_depth(s->depth,s->unit,data->max_depth_dm);
        snprintf(text,sizeof(text),"%u:%02u",data->max_dive_s/60,data->max_dive_s%60);
    } else {
        lv_obj_add_flag(s->unit,LV_OBJ_FLAG_HIDDEN);centred(s->depth,"--",160,47);
        snprintf(text,sizeof(text),"--");
    }
    lv_obj_set_style_text_font(s->longest,data->max_dive_s>=6000?&lv_font_montserrat_22:&lv_font_montserrat_34,0);
    centred(s->longest,text,101,157);
    snprintf(text,sizeof(text),"%u",data->total_dives);
    lv_obj_set_style_text_font(s->count,data->total_dives>=10000?&lv_font_montserrat_22:&lv_font_montserrat_34,0);
    centred(s->count,text,219,157);
    snprintf(text,sizeof(text),"%u:%02u:%02u",data->runtime_s/3600,data->runtime_s/60%60,data->runtime_s%60);
    lv_obj_set_style_text_font(s->runtime,data->runtime_s>=360000?&lv_font_montserrat_28:&lv_font_montserrat_34,0);
    centred(s->runtime,text,160,239);
}
