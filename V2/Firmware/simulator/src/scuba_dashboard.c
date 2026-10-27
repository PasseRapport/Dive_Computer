#include "scuba_dashboard.h"
#include "scuba_presentation.h"
#include "native_fonts.h"
#include "battery_icon.h"
#include "compass_ring.h"
#include <stdio.h>

#define CYAN 0x33dbd4
#define GREEN 0x43e986
#define YELLOW 0xffe34a
#define ORANGE 0xffa438
#define RED 0xff4444

enum dive_phase { PHASE_SURFACE, PHASE_NDL, PHASE_DECO, PHASE_STOP, PHASE_COMPLETE };
struct dashboard {
    lv_obj_t *compass,*battery,*arc,*depth_title,*depth,*unit,*zone,*stop_mark;
    lv_obj_t *left_box,*left_title,*left_value,*left_seconds,*left_note;
    lv_obj_t *time_title,*time_minutes,*time_seconds,*footer_title,*footer_value;
    lv_obj_t *ascent_bg,*ascent[5],*rails[2];
    enum scuba_field field, preferred_field;
    enum scuba_ring ring;
    enum dive_phase phase;
    bool has_phase;
};
static void dashboard_deleted(lv_event_t *event){lv_free(lv_event_get_user_data(event));}
static lv_obj_t *text(lv_obj_t *parent,const lv_font_t *font,int x,int y,const char *value)
{
    lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,value);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_white(),0);
    lv_obj_set_pos(o,x,y);return o;
}
static void centred(lv_obj_t *o,const char *value,int centre,int y)
{
    lv_label_set_text(o,value);lv_obj_set_align(o,LV_ALIGN_TOP_LEFT);lv_obj_update_layout(o);
    lv_obj_set_pos(o,centre-lv_obj_get_width(o)/2,y);
}
static void hidden(lv_obj_t *o,bool flag)
{
    if(flag)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);
}
static void color(lv_obj_t *o,uint32_t rgb){lv_obj_set_style_text_color(o,lv_color_hex(rgb),0);}
static void depth_group(struct dashboard *d,uint16_t dm)
{
    char value[20],whole[16];snprintf(value,sizeof(value),"%u.%u",dm/10,dm%10);
    snprintf(whole,sizeof(whole),"%u",dm/10);
    lv_label_set_text(d->depth,value);lv_obj_update_layout(d->depth);lv_obj_update_layout(d->unit);
    bool marker=!lv_obj_has_flag(d->stop_mark,LV_OBJ_FLAG_HIDDEN);
    int marker_width=marker?26:0;
    /* Centre the visible decimal point on the screen; integers grow left. */
    lv_point_t prefix;
    lv_text_get_size(&prefix,whole,&watch_montserrat_60,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    lv_font_glyph_dsc_t dot;
    lv_font_get_glyph_dsc(&watch_montserrat_60,&dot,'.','0'+dm%10);
    int x=160-prefix.x-dot.ofs_x-dot.box_w/2+1; /* Optical alignment of the dot bitmap. */
    if(marker)lv_obj_set_pos(d->stop_mark,x-marker_width,68);
    lv_obj_set_pos(d->depth,x,47);
    const lv_font_t *small=&lv_font_montserrat_18;
    lv_obj_set_pos(d->unit,x+lv_obj_get_width(d->depth)+5,
        47+watch_montserrat_60.line_height-watch_montserrat_60.base_line-small->line_height+small->base_line);
}
static void timer(lv_obj_t *minutes,lv_obj_t *seconds,unsigned value,int cx)
{
    char a[24],b[12];unsigned mins=value/60;
    snprintf(a,sizeof(a),"%u",mins);snprintf(b,sizeof(b),"'%02u",value%60);
    lv_obj_set_style_text_font(minutes,mins>99?&lv_font_montserrat_34:&lv_font_montserrat_48,0);
    lv_label_set_text(minutes,a);lv_label_set_text(seconds,b);lv_obj_update_layout(minutes);lv_obj_update_layout(seconds);
    int left=cx-(lv_obj_get_width(minutes)+3+lv_obj_get_width(seconds))/2;
    const lv_font_t *large=mins>99?&lv_font_montserrat_34:&lv_font_montserrat_48;
    const lv_font_t *small=&lv_font_montserrat_22;
    lv_obj_set_pos(minutes,left,150);
    lv_obj_set_pos(seconds,left+lv_obj_get_width(minutes)+3,150+large->line_height-large->base_line-small->line_height+small->base_line);
}
/* Draw equal pixel spans on both sides of the centre axis. This avoids
 * rasterizer bias where two joined thick line segments meet at the apex. */
static void draw_stop_mark(lv_event_t *event)
{
    lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(event),&a);
    lv_draw_rect_dsc_t ink;lv_draw_rect_dsc_init(&ink);
    ink.bg_color=lv_color_white();ink.bg_opa=LV_OPA_COVER;
    lv_layer_t *layer=lv_event_get_layer(event);
    for(int row=0;row<8;row++) {
        lv_area_t up={a.x1+row,a.y1+row,a.x1+15-row,a.y1+row};
        lv_area_t down={a.x1+7-row,a.y1+14+row,a.x1+8+row,a.y1+14+row};
        lv_draw_rect(layer,&ink,&up);lv_draw_rect(layer,&ink,&down);
    }
}
static void draw_chevron(lv_event_t *event)
{
    lv_obj_t *o=lv_event_get_target_obj(event);lv_area_t area;lv_obj_get_coords(o,&area);
    lv_draw_rect_dsc_t ink;lv_draw_rect_dsc_init(&ink);ink.bg_opa=LV_OPA_COVER;
    ink.bg_color=lv_obj_get_style_text_color(o,0);lv_layer_t *layer=lv_event_get_layer(event);
    if(!lv_obj_get_user_data(o)) {lv_area_t dash={area.x1+7,area.y1+3,area.x1+12,area.y1+5};lv_draw_rect(layer,&ink,&dash);return;}
    for(int y=0;y<8;y++) {
        int left=8-y,right=10-y;
        lv_area_t a={area.x1+left,area.y1+y,area.x1+right,area.y1+y};
        lv_area_t b={area.x1+19-right,area.y1+y,area.x1+19-left,area.y1+y};
        lv_draw_rect(layer,&ink,&a);lv_draw_rect(layer,&ink,&b);
    }
}
static void draw_rail(lv_event_t *event)
{
    lv_area_t area;lv_obj_get_coords(lv_event_get_target_obj(event),&area);
    /* At the lower latitude, move both ends inwards to preserve the same
     * radial gap to the round display edge as the upper separator. */
    int inset=(int)(uintptr_t)lv_event_get_user_data(event);
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_draw_rect_dsc_t ink;lv_draw_rect_dsc_init(&ink);ink.bg_color=lv_color_hex(CYAN);ink.bg_opa=LV_OPA_COVER;
    lv_area_t middle={area.x1+67+inset,area.y1,area.x1+252-inset,area.y1};lv_draw_rect(layer,&ink,&middle);
    for(int x=40;x<67;x++) {
        int fixed=x<=52?512+(52-x)*1536/12:(67-x)*512/15;
        int y=fixed/256,fraction=fixed%256;
        for(int side=0;side<2;side++) {
            int px=area.x1+(side?319-x-inset:x+inset);
            lv_area_t pixel={px,area.y1+y,px,area.y1+y};
            ink.bg_opa=255-fraction;lv_draw_rect(layer,&ink,&pixel);
            if(fraction) {pixel.y1++;pixel.y2++;ink.bg_opa=fraction;lv_draw_rect(layer,&ink,&pixel);}
        }
    }
}
static void ascent(struct dashboard *d,const struct scuba_model *m)
{
    struct scuba_ascent_display state=scuba_ascent_display(m->ascent_dm_min,m->fast_ascent_ms);
    uint32_t colors[]={0x777777,GREEN,YELLOW,RED};
    hidden(d->ascent_bg,!state.highlighted);
    for(unsigned i=0;i<5;i++) {
        bool active=i<state.levels;
        lv_obj_set_user_data(d->ascent[i],(void *)(uintptr_t)active);
        lv_obj_set_style_text_color(d->ascent[i],lv_color_hex(active?(state.highlighted?0xffffff:colors[state.color]):0x777777),0);
        lv_obj_invalidate(d->ascent[i]);
    }
}
static void footer(struct dashboard *d,const struct scuba_model *m,const struct watch_data *data)
{
    char value[48];const char *caption="";
    uint32_t ink=0xffffff;
    lv_obj_set_style_text_font(d->footer_value,&lv_font_montserrat_34,0);
    switch(d->field) {
    case FIELD_TEMPERATURE:caption="TEMPERATURE";snprintf(value,sizeof(value),"%d°C",m->temperature);break;
    case FIELD_MAX_DEPTH:caption="MAX DEPTH";snprintf(value,sizeof(value),"%u.%u m",m->peak_dm/10,m->peak_dm%10);break;
    case FIELD_CLOCK:caption="CLOCK";snprintf(value,sizeof(value),"%02u:%02u",data->clock_hour,data->clock_minute);break;
    case FIELD_BATTERY:caption="BATTERY";snprintf(value,sizeof(value),"%u%%",m->battery);ink=battery_icon_color(m->battery);break;
    case FIELD_SAFETY_STOP:caption="SAFETY STOP";snprintf(value,sizeof(value),m->safety_required?"3.0 m":"-- m");break;
    case FIELD_TTS:caption="TTS";snprintf(value,sizeof(value),"%u min",(scuba_model_tts(m)+59)/60);break;
    case FIELD_PPO2: {
        unsigned pp=scuba_demo_ppo2(m->depth_dm);caption="ppO2";
        snprintf(value,sizeof(value),"%u.%02u bar",pp/100,pp%100);
        if(pp>160)ink=RED;else if(pp>140)ink=YELLOW;
        break;
    }
    case FIELD_HEADING: {
        static const char *const directions[]={"N","NE","E","SE","S","SW","W","NW"};
        caption="HEADING";snprintf(value,sizeof(value),"%03u° %s",data->heading_degrees,directions[((data->heading_degrees+22)/45)%8]);
        lv_obj_set_style_text_font(d->footer_value,&lv_font_montserrat_28,0);break;
    }
    case FIELD_CEILING:lv_obj_set_style_text_font(d->footer_value,&lv_font_montserrat_42,0);caption="CEILING";snprintf(value,sizeof(value),"%u.%u m",scuba_model_ceiling(m)/10,scuba_model_ceiling(m)%10);break;
    case FIELD_STOP_COMPLETE:caption="STOP COMPLETE";snprintf(value,sizeof(value),"DONE");ink=GREEN;break;
    }
    lv_obj_set_style_text_font(d->footer_title,d->field==FIELD_CEILING?&lv_font_montserrat_16:&lv_font_montserrat_12,0);
    if(d->field!=FIELD_CEILING&&(m->notifications&~SCUBA_BATTERY))lv_obj_set_style_text_font(d->footer_value,&lv_font_montserrat_28,0);
    centred(d->footer_title,caption,160,219);centred(d->footer_value,value,160,d->field==FIELD_CEILING?237:(m->notifications&~SCUBA_BATTERY)?235:239);
    color(d->footer_value,ink);
}
lv_obj_t *scuba_dashboard_create(lv_obj_t *parent)
{
    lv_obj_t *view=lv_obj_create(parent);lv_obj_remove_style_all(view);lv_obj_set_size(view,320,300);
    lv_obj_remove_flag(view,LV_OBJ_FLAG_SCROLLABLE);
    struct dashboard *d=lv_malloc_zeroed(sizeof(*d));LV_ASSERT_MALLOC(d);lv_obj_set_user_data(view,d);
    d->arc=lv_arc_create(view);lv_obj_set_size(d->arc,296,296);lv_obj_set_pos(d->arc,12,2);
    lv_arc_set_bg_angles(d->arc,0,360);lv_arc_set_rotation(d->arc,90);lv_arc_set_range(d->arc,0,1000);
    lv_obj_set_style_arc_width(d->arc,7,LV_PART_MAIN);lv_obj_set_style_arc_width(d->arc,7,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(d->arc,lv_color_hex(0x282e32),LV_PART_MAIN);
    lv_obj_remove_style(d->arc,NULL,LV_PART_KNOB);
    lv_obj_set_style_bg_opa(d->arc,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_border_width(d->arc,0,LV_PART_KNOB);lv_obj_set_style_shadow_width(d->arc,0,LV_PART_KNOB);
    lv_obj_set_style_arc_rounded(d->arc,false,LV_PART_MAIN);lv_obj_set_style_arc_rounded(d->arc,true,LV_PART_INDICATOR);lv_obj_remove_flag(d->arc,LV_OBJ_FLAG_CLICKABLE);
    d->stop_mark=lv_obj_create(view);lv_obj_remove_style_all(d->stop_mark);lv_obj_set_size(d->stop_mark,16,22);
    lv_obj_remove_flag(d->stop_mark,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(d->stop_mark,draw_stop_mark,LV_EVENT_DRAW_MAIN,NULL);hidden(d->stop_mark,true);
    d->depth_title=text(view,&lv_font_montserrat_12,0,0,"DEPTH");color(d->depth_title,0xffffff);
    d->depth=text(view,&watch_montserrat_60,0,47,"");d->unit=text(view,&lv_font_montserrat_18,0,0,"m");
    d->zone=text(view,&lv_font_montserrat_10,0,108,"");color(d->zone,YELLOW);
    d->left_box=lv_obj_create(view);lv_obj_remove_style_all(d->left_box);lv_obj_set_pos(d->left_box,52,127);lv_obj_set_size(d->left_box,94,77);
    lv_obj_set_style_radius(d->left_box,10,0);lv_obj_set_style_bg_opa(d->left_box,LV_OPA_COVER,0);lv_obj_remove_flag(d->left_box,LV_OBJ_FLAG_SCROLLABLE);
    d->left_title=text(view,&lv_font_montserrat_12,0,127,"");
    d->left_value=text(view,&lv_font_montserrat_48,0,150,"");d->left_seconds=text(view,&lv_font_montserrat_22,0,171,"");
    d->left_note=text(view,&lv_font_montserrat_14,0,204,"");color(d->left_note,CYAN);
    d->time_title=text(view,&lv_font_montserrat_12,0,127,"DIVE TIME");color(d->time_title,0xffffff);centred(d->time_title,"DIVE TIME",219,127);
    d->time_minutes=text(view,&lv_font_montserrat_48,0,150,"");d->time_seconds=text(view,&lv_font_montserrat_22,0,171,"");
    d->footer_title=text(view,&lv_font_montserrat_12,0,219,"");color(d->footer_title,0xffffff);
    d->footer_value=text(view,&lv_font_montserrat_34,0,239,"");
    for(int i=0;i<2;i++) {
        d->rails[i]=lv_obj_create(view);lv_obj_remove_style_all(d->rails[i]);
        lv_obj_set_pos(d->rails[i],0,i?215:118);lv_obj_set_size(d->rails[i],320,10);
        lv_obj_remove_flag(d->rails[i],LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(d->rails[i],draw_rail,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)(i?23:0));
    }
    d->ascent_bg=lv_obj_create(view);lv_obj_remove_style_all(d->ascent_bg);lv_obj_set_pos(d->ascent_bg,148,141);lv_obj_set_size(d->ascent_bg,24,64);
    lv_obj_set_style_radius(d->ascent_bg,8,0);lv_obj_set_style_bg_color(d->ascent_bg,lv_color_hex(RED),0);lv_obj_set_style_bg_opa(d->ascent_bg,LV_OPA_COVER,0);
    for(int i=0;i<5;i++) {
        d->ascent[i]=lv_obj_create(view);lv_obj_remove_style_all(d->ascent[i]);
        lv_obj_set_pos(d->ascent[i],150,194-12*i);lv_obj_set_size(d->ascent[i],20,8);
        lv_obj_remove_flag(d->ascent[i],LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(d->ascent[i],draw_chevron,LV_EVENT_DRAW_MAIN,NULL);
    }
    d->compass=compass_ring_create(view);hidden(d->compass,true);
    d->battery=battery_icon_create(view,142,16);
    lv_obj_add_event_cb(view,dashboard_deleted,LV_EVENT_DELETE,d);
    return view;
}
void scuba_dashboard_update(lv_obj_t *view,const struct scuba_model *m,const struct watch_data *data)
{
    struct dashboard *d=lv_obj_get_user_data(view);
    bool surface=scuba_model_surface(m);uint16_t ceiling=scuba_model_ceiling(m);
    uint32_t target=scuba_model_stop_target(m);
    bool stop=!surface&&target&&(scuba_model_in_stop(m)||(!ceiling&&m->depth_dm<30));
    bool complete=!surface&&(m->deco_done||(target&&m->stop_ms==target));
    enum dive_phase phase=surface?PHASE_SURFACE:complete?PHASE_COMPLETE:stop?PHASE_STOP:ceiling?PHASE_DECO:PHASE_NDL;
    if(!d->has_phase||phase!=d->phase) {
        if(ceiling)d->field=FIELD_CEILING;
        else if(stop)d->field=FIELD_SAFETY_STOP;
        else if(complete)d->field=FIELD_STOP_COMPLETE;
        else d->field=d->preferred_field;
        d->phase=phase;d->has_phase=true;
    }
    hidden(d->depth_title,true);
    /* Leave the perimeter labels above the battery in compass mode. */
    lv_obj_set_pos(d->battery,142,d->ring==RING_COMPASS?30:16);
    battery_icon_update(d->battery,m->battery);
    hidden(d->stop_mark,!(stop&&!complete));
    depth_group(d,m->depth_dm);
    char text[48];
    if(stop)snprintf(text,sizeof(text),ceiling?"STOP %u-%u m":"STOP 3-6 m",ceiling/10,ceiling/10+3);
    else if(ceiling&&m->depth_dm<ceiling)snprintf(text,sizeof(text),"DESCEND TO %u m",ceiling/10);
    else text[0]=0;
    centred(d->zone,text,160,107);
    hidden(d->left_box,true);hidden(d->left_seconds,true);
    lv_obj_set_style_text_font(d->left_value,&lv_font_montserrat_48,0);
    color(d->left_title,0xffffff);color(d->left_value,0xffffff);color(d->left_seconds,0xffffff);
    centred(d->left_note,"",101,197);
    uint32_t ring_color=GREEN,progress=0;
    if(surface) {
        centred(d->left_title,"SURFACE",101,127);scuba_surface_text(text,sizeof(text),m->surface_ms/1000);
        lv_obj_set_style_text_font(d->left_value,&lv_font_montserrat_28,0);centred(d->left_value,text,101,158);
    } else if(stop&&!complete) {
        centred(d->left_title,"STOP",101,127);hidden(d->left_seconds,false);
        timer(d->left_value,d->left_seconds,(target-m->stop_ms+999)/1000,101);
        ring_color=YELLOW;progress=scuba_ring_stop(m->stop_ms,target);
        color(d->left_title,YELLOW);color(d->left_value,YELLOW);color(d->left_seconds,YELLOW);
        if(!scuba_model_in_stop(m))centred(d->left_note,"PAUSED",101,197);
    } else if(ceiling&&!complete) {
        centred(d->left_title,"DECO",101,127);snprintf(text,sizeof(text),"%u'",(scuba_model_tts(m)+59)/60);centred(d->left_value,text,101,150);
        snprintf(text,sizeof(text),"CEIL %u m",ceiling/10);centred(d->left_note,text,101,197);
        ring_color=ORANGE;progress=scuba_ring_tts(scuba_model_tts(m));
        color(d->left_title,ORANGE);
    } else {
        centred(d->left_title,"NO DECO",101,127);
        if(complete){centred(d->left_value,"--",101,150);progress=0;}
        else {
            if(m->ndl_s/60>99)lv_obj_set_style_text_font(d->left_value,&lv_font_montserrat_42,0);
            scuba_ndl_text(text,sizeof(text),m->ndl_s);char number[64];snprintf(number,sizeof(number),"%s'",text);centred(d->left_value,number,101,150);
            progress=scuba_ring_ndl(m->ndl_s);
            if(m->ndl_s<300)ring_color=ORANGE;
            if(m->ndl_s<=300){hidden(d->left_box,false);lv_obj_set_style_bg_color(d->left_box,lv_color_hex(YELLOW),0);color(d->left_title,0);color(d->left_value,0);}
        }
    }
    /* Explicit ring choices stay selected; the alert overlay and ceiling track
     * remain independent of the chosen data source. */
    if(d->ring==RING_NDL) {
        progress=scuba_ring_ndl(m->ndl_s);
        ring_color=m->ndl_s<300?ORANGE:GREEN;
    } else if(d->ring==RING_TTS) {
        progress=scuba_ring_tts(scuba_model_tts(m));
        ring_color=ORANGE;
    } else if(d->ring==RING_STOP) {
        progress=scuba_ring_stop(m->stop_ms,target);
        ring_color=complete?GREEN:YELLOW;
    }
    hidden(d->arc,d->ring==RING_COMPASS);hidden(d->compass,d->ring!=RING_COMPASS);
    if(d->ring==RING_COMPASS)compass_ring_update(d->compass,data->heading_degrees);
    lv_arc_set_value(d->arc,progress);lv_obj_set_style_arc_color(d->arc,lv_color_hex(ring_color),LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(d->arc,lv_color_hex(ceiling&&m->depth_dm<ceiling?RED:0x282e32),LV_PART_MAIN);
    timer(d->time_minutes,d->time_seconds,m->dive_ms/1000,219);
    /* Anchor the minute units at a fixed right edge. Additional minute
     * digits grow towards the centre, preserving the space for seconds. */
    lv_obj_update_layout(d->time_minutes);
    lv_obj_set_x(d->time_minutes,239-lv_obj_get_width(d->time_minutes));
    lv_obj_set_x(d->time_seconds,242);
    ascent(d,m);footer(d,m,data);
}
enum scuba_field scuba_dashboard_field(lv_obj_t *view){return ((struct dashboard *)lv_obj_get_user_data(view))->field;}

const char *scuba_field_name(enum scuba_field field)
{
    static const char *const names[]={"Temperature","Max depth","Clock","Battery","Safety stop","TTS","ppO2","Heading"};
    return field<SCUBA_FIELD_COUNT?names[field]:"Temperature";
}
const char *scuba_ring_name(enum scuba_ring ring)
{
    static const char *const names[]={"Auto","No deco","TTS","Stop duration","Compass"};
    return ring<SCUBA_RING_COUNT?names[ring]:"Auto";
}
void scuba_dashboard_set_field(lv_obj_t *view,enum scuba_field field)
{
    struct dashboard *d=lv_obj_get_user_data(view);
    if(field<SCUBA_FIELD_COUNT)d->field=d->preferred_field=field;
}
void scuba_dashboard_set_ring(lv_obj_t *view,enum scuba_ring ring)
{
    struct dashboard *d=lv_obj_get_user_data(view);
    if(ring<SCUBA_RING_COUNT)d->ring=ring;
}
enum scuba_ring scuba_dashboard_ring(lv_obj_t *view)
{
    return ((struct dashboard *)lv_obj_get_user_data(view))->ring;
}
