#include "analog_clock.h"
#include "battery_icon.h"
#include <stdio.h>

LV_IMAGE_DECLARE(project_logo_alpha);
static lv_obj_t *view,*logo,*date,*battery,*hands;
static enum watch_dial selected;
static struct watch_data last_data;

static lv_point_precise_t point(const lv_area_t *a,int angle,int radius,int side)
{
    return (lv_point_precise_t){
        a->x1+160+((lv_trigo_sin(angle)*radius+lv_trigo_sin(angle+90)*side)>>LV_TRIGO_SHIFT),
        a->y1+150+((-lv_trigo_sin(angle+90)*radius+lv_trigo_sin(angle)*side)>>LV_TRIGO_SHIFT)};
}
static void line(lv_layer_t *layer,const lv_area_t *a,int angle,int start,int end,int width,unsigned color)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
    d.p1=point(a,angle,start,0);d.p2=point(a,angle,end,0);
    d.width=width;d.color=lv_color_hex(color);d.opa=LV_OPA_COVER;
    lv_draw_line(layer,&d);
}
static void polygon(lv_layer_t *layer,const lv_area_t *a,int angle,const int (*p)[2],int count,unsigned color)
{
    for(int i=1;i<count-1;i++) {
        lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);
        d.p[0]=point(a,angle,p[0][0],p[0][1]);
        d.p[1]=point(a,angle,p[i][0],p[i][1]);
        d.p[2]=point(a,angle,p[i+1][0],p[i+1][1]);
        d.color=lv_color_hex(color);d.opa=LV_OPA_COVER;lv_draw_triangle(layer,&d);
    }
}
static void disc(lv_layer_t *layer,lv_point_precise_t p,int radius,unsigned color)
{
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=LV_RADIUS_CIRCLE;
    d.bg_color=lv_color_hex(color);d.bg_opa=LV_OPA_COVER;
    lv_area_t a={p.x-radius,p.y-radius,p.x+radius,p.y+radius};lv_draw_rect(layer,&d,&a);
}
static void draw_ticks(lv_event_t *e)
{
    lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    if(selected==DIAL_MINIMALIST) {
        for(int i=0;i<12;i++)line(layer,&a,i*30,i%3?122:115,133,i%3?2:4,0xffffff);
        return;
    }
    /* The numbered dials share the same sixty one-minute perimeter ticks. */
    for(int i=0;i<60;i++) {
        line(layer,&a,i*6,i%5?137:134,143,i%5?1:2,0xe8e8dd);
        if(selected==DIAL_AUTOMATIC && i%5==0 && i)
            disc(layer,point(&a,i*6,137,0),3,0xe4e8d7);
    }
    if(selected==DIAL_AUTOMATIC) {
        const int triangle[][2]={{119,-11},{137,0},{119,11}};
        polygon(layer,&a,0,triangle,3,0xe8ebdd);
    }
}
static void draw_hand(lv_layer_t *layer,const lv_area_t *a,int angle,bool minute)
{
    int tip=minute?114:87;
    if(selected==DIAL_MINIMALIST) {
        int w=minute?2:3;
        int p[][2]={{22,-w},{tip,-w},{tip,w},{22,w}};
        polygon(layer,a,angle,p,4,0xffffff);
    } else if(selected==DIAL_DAUPHINE) {
        int shoulder=minute?51:40,w=minute?6:10;
        int left[][2]={{19,0},{shoulder,-w},{tip,0}};
        int right[][2]={{19,0},{tip,0},{shoulder,w}};
        polygon(layer,a,angle,left,3,0xffffff);polygon(layer,a,angle,right,3,0x888888);
    } else {
        tip=minute?116:86;
        int shoulder=minute?48:36,w=minute?6:9;
        int outer[][2]={{-7,-2},{shoulder,-w},{tip,0},{shoulder,w},{-7,2}};
        int left[][2]={{-5,0},{shoulder,-w},{tip,0}};
        int insert[][2]={{11,-1},{shoulder,-w+3},{tip-12,0},{shoulder,w-3},{11,1}};
        polygon(layer,a,angle,outer,5,0xc4cbd0);polygon(layer,a,angle,left,3,0xf4f4ed);
        polygon(layer,a,angle,insert,5,0xe4e9d7);
    }
}
static void draw_hands(lv_event_t *e)
{
    lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    draw_hand(layer,&a,(last_data.clock_hour%12)*30+last_data.clock_minute/2,false);
    draw_hand(layer,&a,last_data.clock_minute*6+last_data.clock_second/10,true);
    if(selected==DIAL_AUTOMATIC) {
        lv_point_precise_t p=point(&a,0,0,0);disc(layer,p,6,0xe2e5dc);disc(layer,p,2,0x9ba5a8);
    }
}
static lv_obj_t *drawing(lv_event_cb_t callback)
{
    lv_obj_t *o=lv_obj_create(view);lv_obj_remove_style_all(o);lv_obj_set_size(o,320,300);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o,callback,LV_EVENT_DRAW_MAIN,NULL);return o;
}
static void number(int angle,int radius,int value,const lv_font_t *font)
{
    lv_obj_t *n=lv_label_create(view);char text[4];snprintf(text,sizeof(text),"%d",value);
    lv_label_set_text(n,text);lv_obj_set_style_text_font(n,font,0);
    lv_obj_set_style_text_color(n,lv_color_hex(0xf1f1e9),0);lv_obj_update_layout(n);
    const lv_area_t origin={0};lv_point_precise_t p=point(&origin,angle,radius,0);
    lv_obj_set_pos(n,p.x-lv_obj_get_width(n)/2,p.y-lv_obj_get_height(n)/2);
}
int analog_clock_logo_y(void){return selected==DIAL_AUTOMATIC?101:90;}
int analog_clock_logo_scale(void){return selected==DIAL_AUTOMATIC?44:60;}
lv_obj_t *analog_clock_logo(void){return logo;}
void analog_clock_set_dial(enum watch_dial dial)
{
    if(dial<0 || dial>=DIAL_COUNT)return;
    selected=dial;lv_obj_clean(view);
    lv_obj_set_style_bg_color(view,lv_color_hex(dial==DIAL_AUTOMATIC?0x1b3040:0),0);
    drawing(draw_ticks);
    if(dial==DIAL_AUTOMATIC) {
        lv_obj_t *ring=lv_obj_create(view);lv_obj_remove_style_all(ring);
        lv_obj_set_pos(ring,70,60);lv_obj_set_size(ring,180,180);
        lv_obj_set_style_border_width(ring,1,0);lv_obj_set_style_border_color(ring,lv_color_hex(0xcbd2cb),0);
        lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);lv_obj_remove_flag(ring,LV_OBJ_FLAG_SCROLLABLE);
        for(int i=0;i<12;i++)if(i!=3) {
            number(i*30,77,i?i:12,&lv_font_montserrat_16);
            if(i)number(i*30,113,i*5,&lv_font_montserrat_22);
        }
    } else if(dial==DIAL_DAUPHINE) {
        for(int i=0;i<12;i++)number(i*30,123,i?i:12,&lv_font_montserrat_20);
    }
    logo=lv_image_create(view);lv_image_set_src(logo,&project_logo_alpha);
    lv_image_set_scale(logo,analog_clock_logo_scale());lv_image_set_antialias(logo,true);
    lv_obj_set_style_image_recolor(logo,lv_color_white(),0);lv_obj_set_style_image_recolor_opa(logo,LV_OPA_COVER,0);
    lv_obj_set_pos(logo,60,analog_clock_logo_y()-209/2);
    date=lv_label_create(view);lv_obj_set_style_text_font(date,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(date,lv_color_hex(dial==DIAL_AUTOMATIC?0x1a2530:0xffffff),0);
    if(dial==DIAL_AUTOMATIC) {
        lv_obj_set_style_bg_color(date,lv_color_hex(0xf3f3e7),0);lv_obj_set_style_bg_opa(date,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(date,1,0);lv_obj_set_style_border_color(date,lv_color_hex(0x939e9e),0);
        lv_obj_set_style_pad_hor(date,4,0);lv_obj_set_style_pad_ver(date,2,0);
    }
    battery=battery_icon_create(view,142,218);
    if(dial==DIAL_AUTOMATIC)lv_obj_add_flag(battery,LV_OBJ_FLAG_HIDDEN);
    hands=drawing(draw_hands);analog_clock_update(&last_data);
}
lv_obj_t *analog_clock_create(lv_obj_t *parent)
{
    view=lv_obj_create(parent);lv_obj_remove_style_all(view);lv_obj_set_size(view,320,300);
    lv_obj_set_style_bg_opa(view,LV_OPA_COVER,0);lv_obj_remove_flag(view,LV_OBJ_FLAG_SCROLLABLE);
    analog_clock_set_dial(DIAL_MINIMALIST);return view;
}
void analog_clock_update(const struct watch_data *data)
{
    bool changed=data->clock_hour!=last_data.clock_hour || data->clock_minute!=last_data.clock_minute ||
                 data->clock_second!=last_data.clock_second;
    last_data=*data;
    static const char *const days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
    lv_label_set_text_fmt(date,"%s %02u",days[data->clock_weekday%7],data->clock_day);lv_obj_update_layout(date);
    lv_obj_set_pos(date,(selected==DIAL_AUTOMATIC?236:233)-lv_obj_get_width(date)/2,selected==DIAL_AUTOMATIC?139:141);
    battery_icon_update(battery,data->battery_percent);
    if(changed)lv_obj_invalidate(hands);
}
