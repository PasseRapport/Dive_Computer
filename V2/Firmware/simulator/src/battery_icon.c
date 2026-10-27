#include "battery_icon.h"
uint32_t battery_icon_color(unsigned p) {return p<=10?0xff3333:p<=50?0xffdc00:0xffffff;}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
    return o;
}
void battery_icon_update(lv_obj_t *icon,unsigned percentage)
{
    percentage=LV_MIN(percentage,100);
    if((uintptr_t)lv_obj_get_user_data(icon)==percentage+1)return;
    lv_obj_set_user_data(icon,(void *)(uintptr_t)(percentage+1));
    lv_color_t ink=lv_color_hex(battery_icon_color(percentage));
    lv_obj_t *body=lv_obj_get_child(icon,0),*terminal=lv_obj_get_child(icon,1),*fill=lv_obj_get_child(body,0);
    lv_obj_set_style_border_color(body,ink,0);lv_obj_set_style_bg_color(terminal,ink,0);
    lv_obj_set_style_bg_color(fill,ink,0);
    if(!percentage)lv_obj_add_flag(fill,LV_OBJ_FLAG_HIDDEN);
    else {lv_obj_remove_flag(fill,LV_OBJ_FLAG_HIDDEN);lv_obj_set_width(fill,LV_MAX(1,30*percentage/100));}
}
lv_obj_t *battery_icon_create(lv_obj_t *parent,int x,int y)
{
    lv_obj_t *icon=box(parent,x,y,36,16),*body=box(icon,0,1,32,14);
    lv_obj_set_style_border_width(body,1,0);
    lv_obj_set_style_radius(body,3,0);lv_obj_set_style_clip_corner(body,true,0);
    lv_obj_set_style_bg_color(body,lv_color_black(),0);lv_obj_set_style_bg_opa(body,LV_OPA_COVER,0);
    lv_obj_t *terminal=box(icon,32,5,3,6);lv_obj_set_style_bg_opa(terminal,LV_OPA_COVER,0);lv_obj_set_style_radius(terminal,1,0);
    lv_obj_t *fill=box(body,0,0,30,12);lv_obj_set_style_bg_opa(fill,LV_OPA_COVER,0);lv_obj_set_style_radius(fill,2,0);
    battery_icon_update(icon,100);return icon;
}
