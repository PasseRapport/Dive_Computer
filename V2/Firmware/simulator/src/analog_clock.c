#include "analog_clock.h"
#include "battery_icon.h"
#include "native_fonts.h"
#include <stdio.h>

LV_IMAGE_DECLARE(project_logo_alpha);
struct analog_face {
    lv_obj_t *view,*logo,*date,*battery,*hands;
    enum watch_dial selected;
    struct watch_data data;
};

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
    struct analog_face *c=lv_event_get_user_data(e);
    lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    if(c->selected==DIAL_MINIMALIST) {
        for(int i=0;i<12;i++)line(layer,&a,i*30,i%3?122:115,133,i%3?2:4,0xffffff);
        return;
    }
    /* The numbered dials share the same sixty one-minute perimeter ticks. */
    for(int i=0;i<60;i++) {
        line(layer,&a,i*6,i%5?137:134,143,i%5?1:2,0xe8e8dd);
        if(c->selected==DIAL_AUTOMATIC && i%5==0 && i)
            disc(layer,point(&a,i*6,137,0),3,0xe4e8d7);
    }
    if(c->selected==DIAL_AUTOMATIC) {
        const int triangle[][2]={{119,-11},{137,0},{119,11}};
        polygon(layer,&a,0,triangle,3,0xe8ebdd);
    }
}
static void draw_hand(struct analog_face *c,lv_layer_t *layer,const lv_area_t *a,int angle,bool minute)
{
    int tip=minute?114:87;
    if(c->selected==DIAL_MINIMALIST) {
        int w=minute?2:3;
        int p[][2]={{22,-w},{tip,-w},{tip,w},{22,w}};
        polygon(layer,a,angle,p,4,0xffffff);
    } else if(c->selected==DIAL_DAUPHINE) {
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
    struct analog_face *c=lv_event_get_user_data(e);
    lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    draw_hand(c,layer,&a,(c->data.clock_hour%12)*30+c->data.clock_minute/2,false);
    draw_hand(c,layer,&a,c->data.clock_minute*6+c->data.clock_second/10,true);
    if(c->selected==DIAL_AUTOMATIC) {
        lv_point_precise_t p=point(&a,0,0,0);disc(layer,p,6,0xe2e5dc);disc(layer,p,2,0x9ba5a8);
    }
}
static lv_obj_t *drawing(struct analog_face *c,lv_event_cb_t callback)
{
    lv_obj_t *o=lv_obj_create(c->view);lv_obj_remove_style_all(o);lv_obj_set_size(o,320,300);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o,callback,LV_EVENT_DRAW_MAIN,c);return o;
}
static void number(struct analog_face *c,int angle,int radius,int value,const lv_font_t *font)
{
    lv_obj_t *n=lv_label_create(c->view);char text[4];snprintf(text,sizeof(text),"%d",value);
    lv_label_set_text(n,text);lv_obj_set_style_text_font(n,font,0);
    lv_obj_set_style_text_color(n,lv_color_hex(0xf1f1e9),0);lv_obj_update_layout(n);
    const lv_area_t origin={0};lv_point_precise_t p=point(&origin,angle,radius,0);
    lv_obj_set_pos(n,p.x-lv_obj_get_width(n)/2,p.y-lv_obj_get_height(n)/2);
}
int analog_clock_logo_y(lv_obj_t *view){struct analog_face *c=lv_obj_get_user_data(view);return c->selected==DIAL_AUTOMATIC?101:90;}
int analog_clock_logo_scale(lv_obj_t *view){struct analog_face *c=lv_obj_get_user_data(view);return c->selected==DIAL_AUTOMATIC?44:60;}
lv_obj_t *analog_clock_logo(lv_obj_t *view){struct analog_face *c=lv_obj_get_user_data(view);return c->logo;}
void analog_clock_set_dial(lv_obj_t *view,enum watch_dial dial)
{
    struct analog_face *c=lv_obj_get_user_data(view);
    if(dial<0 || dial>DIAL_AUTOMATIC)return;
    c->selected=dial;lv_obj_clean(c->view);
    lv_obj_set_style_bg_color(c->view,lv_color_hex(dial==DIAL_AUTOMATIC?0x1b3040:0),0);
    drawing(c,draw_ticks);
    if(dial==DIAL_AUTOMATIC) {
        lv_obj_t *ring=lv_obj_create(c->view);lv_obj_remove_style_all(ring);
        lv_obj_set_pos(ring,70,60);lv_obj_set_size(ring,180,180);
        lv_obj_set_style_border_width(ring,1,0);lv_obj_set_style_border_color(ring,lv_color_hex(0xcbd2cb),0);
        lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);lv_obj_remove_flag(ring,LV_OBJ_FLAG_SCROLLABLE);
        for(int i=0;i<12;i++)if(i!=3) {
            number(c,i*30,77,i?i:12,&lv_font_montserrat_16);
            if(i)number(c,i*30,113,i*5,&lv_font_montserrat_22);
        }
    } else if(dial==DIAL_DAUPHINE) {
        for(int i=0;i<12;i++) {
            if(i%3==0)number(c,i*30,121,i?i:12,&watch_montserrat_bold_28);
            else number(c,i*30,123,i*5,&lv_font_montserrat_16);
        }
    }
    c->logo=lv_image_create(c->view);lv_image_set_src(c->logo,&project_logo_alpha);
    lv_image_set_scale(c->logo,analog_clock_logo_scale(view));lv_image_set_antialias(c->logo,true);
    lv_obj_set_style_image_recolor(c->logo,lv_color_white(),0);lv_obj_set_style_image_recolor_opa(c->logo,LV_OPA_COVER,0);
    lv_obj_set_pos(c->logo,60,analog_clock_logo_y(view)-209/2);
    c->date=lv_label_create(c->view);lv_obj_set_style_text_font(c->date,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(c->date,lv_color_hex(dial==DIAL_AUTOMATIC?0x1a2530:0xffffff),0);
    if(dial==DIAL_AUTOMATIC) {
        lv_obj_set_style_bg_color(c->date,lv_color_hex(0xf3f3e7),0);lv_obj_set_style_bg_opa(c->date,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(c->date,1,0);lv_obj_set_style_border_color(c->date,lv_color_hex(0x939e9e),0);
        lv_obj_set_style_pad_hor(c->date,4,0);lv_obj_set_style_pad_ver(c->date,2,0);
    }
    c->battery=battery_icon_create(c->view,142,dial==DIAL_AUTOMATIC?198:dial==DIAL_DAUPHINE?214:218);
    c->hands=drawing(c,draw_hands);analog_clock_update(view,&c->data);
}
static void release(lv_event_t *e){lv_free(lv_event_get_user_data(e));}
lv_obj_t *analog_clock_create(lv_obj_t *parent)
{
    struct analog_face *c=lv_malloc_zeroed(sizeof(*c));LV_ASSERT_MALLOC(c);
    c->view=lv_obj_create(parent);lv_obj_set_user_data(c->view,c);
    lv_obj_add_event_cb(c->view,release,LV_EVENT_DELETE,c);
    lv_obj_remove_style_all(c->view);lv_obj_set_size(c->view,320,300);
    lv_obj_set_style_bg_opa(c->view,LV_OPA_COVER,0);lv_obj_remove_flag(c->view,LV_OBJ_FLAG_SCROLLABLE);
    analog_clock_set_dial(c->view,DIAL_MINIMALIST);return c->view;
}
void analog_clock_update(lv_obj_t *view,const struct watch_data *data)
{
    struct analog_face *c=lv_obj_get_user_data(view);
    bool changed=data->clock_hour!=c->data.clock_hour || data->clock_minute!=c->data.clock_minute ||
                 data->clock_second!=c->data.clock_second;
    c->data=*data;
    static const char *const days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
    lv_label_set_text_fmt(c->date,"%s %02u",days[data->clock_weekday%7],data->clock_day);lv_obj_update_layout(c->date);
    lv_obj_set_pos(c->date,(c->selected==DIAL_AUTOMATIC?236:233)-lv_obj_get_width(c->date)/2,c->selected==DIAL_AUTOMATIC?139:141);
    battery_icon_update(c->battery,data->battery_percent);
    if(changed)lv_obj_invalidate(c->hands);
}
