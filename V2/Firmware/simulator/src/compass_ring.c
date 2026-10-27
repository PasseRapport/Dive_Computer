#include "compass_ring.h"
struct compass_ring {unsigned heading;lv_obj_t *cardinals[4];};
static lv_point_precise_t point(int angle,int radius)
{
    return (lv_point_precise_t){160+((lv_trigo_sin(angle)*radius)>>LV_TRIGO_SHIFT),150-((lv_trigo_sin(angle+90)*radius)>>LV_TRIGO_SHIFT)};
}
static void draw(lv_event_t *e)
{
    lv_obj_t *o=lv_event_get_target_obj(e);struct compass_ring *s=lv_obj_get_user_data(o);
    lv_area_t area;lv_obj_get_coords(o,&area);lv_layer_t *layer=lv_event_get_layer(e);
    for(int i=0;i<72;i++) {
        lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
        d.p1=point(i*5-(int)s->heading,i%18==0?138:i%6==0?141:144);
        d.p2=point(i*5-(int)s->heading,147);
        d.p1.x+=area.x1;d.p1.y+=area.y1;d.p2.x+=area.x1;d.p2.y+=area.y1;
        d.width=i%18==0?2:1;d.color=i==0?lv_color_hex(0xff4444):lv_color_white();
        lv_draw_line(layer,&d);
    }
    lv_draw_line_dsc_t marker;lv_draw_line_dsc_init(&marker);
    marker.p1=(lv_point_precise_t){area.x1+160,area.y1+1};marker.p2=(lv_point_precise_t){area.x1+160,area.y1+9};
    marker.width=2;marker.color=lv_color_hex(0xff4444);lv_draw_line(layer,&marker);
}
static void deleted(lv_event_t *e){lv_free(lv_event_get_user_data(e));}
void compass_ring_update(lv_obj_t *ring,unsigned heading)
{
    struct compass_ring *s=lv_obj_get_user_data(ring);heading%=360;
    if(s->heading==heading)return;
    s->heading=heading;
    for(int i=0;i<4;i++) {
        lv_point_precise_t p=point(i*90-(int)heading,132);lv_obj_update_layout(s->cardinals[i]);
        int x=p.x-lv_obj_get_width(s->cardinals[i])/2;
        int y=p.y-lv_obj_get_height(s->cardinals[i])/2;
        lv_obj_set_pos(s->cardinals[i],x,y);
    }
    lv_obj_invalidate(ring);
}
lv_obj_t *compass_ring_create(lv_obj_t *parent)
{
    lv_obj_t *ring=lv_obj_create(parent);lv_obj_remove_style_all(ring);lv_obj_set_size(ring,320,300);
    lv_obj_remove_flag(ring,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(ring,LV_OBJ_FLAG_CLICKABLE);
    struct compass_ring *s=lv_malloc_zeroed(sizeof(*s));LV_ASSERT_MALLOC(s);s->heading=UINT32_MAX;
    lv_obj_set_user_data(ring,s);lv_obj_add_event_cb(ring,deleted,LV_EVENT_DELETE,s);lv_obj_add_event_cb(ring,draw,LV_EVENT_DRAW_MAIN,NULL);
    const char *const names[]={"N","E","S","W"};
    for(int i=0;i<4;i++) {s->cardinals[i]=lv_label_create(ring);lv_label_set_text(s->cardinals[i],names[i]);lv_obj_set_style_text_font(s->cardinals[i],&lv_font_montserrat_10,0);lv_obj_set_style_text_color(s->cardinals[i],lv_color_white(),0);}
    compass_ring_update(ring,0);return ring;
}
