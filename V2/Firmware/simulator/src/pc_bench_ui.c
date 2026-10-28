/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench_ui.h"
#include "pc_bench.h"
#include "scuba_ui.h"
#include "sim_host.h"
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <stdio.h>

enum bench_action { BENCH_PAUSE, BENCH_SPEED, BENCH_ASCEND_RATE, BENCH_DESCEND_RATE,
    BENCH_HOLD, BENCH_SURFACE, BENCH_AUTO, BENCH_SHALLOWER, BENCH_DEEPER, BENCH_FRESH, BENCH_GAP, BENCH_SAVE, BENCH_RESTART, BENCH_RESTART_MODE };

static lv_obj_t *summary, *tissues[16], *stops, *graph, *clock_label, *pause_label, *speed_label, *auto_label;
static lv_obj_t *time_ticks[5];
static uint64_t last_host_ms, last_paint;
static lv_obj_t *recovery_label,*restart_mode_label;
static enum pc_restart_mode restart_mode;
static const char *restart_modes[]={"Next: known history","Next: missing exposure","Next: unknown clock",
    "Next: corrupt data","Next: wrong schema","Next: stale data"};
static lv_obj_t *label(const char *text,int x,int y)
{
    lv_obj_t *o=lv_label_create(lv_screen_active());
    lv_label_set_text(o,text); lv_obj_set_pos(o,x,y); return o;
}
static void command(lv_event_t *e)
{
    const struct bench_view *v=pc_bench_read();
    enum bench_action action=(enum bench_action)(intptr_t)lv_event_get_user_data(e);
    switch (action) {
    case BENCH_PAUSE: pc_bench_pause(!v->paused); break;
    case BENCH_SPEED: pc_bench_speed(v->speed==1?10:v->speed==10?60:1); break;
    case BENCH_ASCEND_RATE: pc_bench_manual(v->depth_m,v->rate_m_min-1); break;
    case BENCH_DESCEND_RATE: pc_bench_manual(v->depth_m,v->rate_m_min+1); break;
    case BENCH_HOLD: pc_bench_manual(v->depth_m,0); break;
    case BENCH_SURFACE: pc_bench_manual(0,0); break;
    case BENCH_SHALLOWER: pc_bench_manual(v->depth_m>1?v->depth_m-1:0,0); break;
    case BENCH_DEEPER: pc_bench_manual(v->depth_m+1,0); break;
    case BENCH_AUTO: pc_bench_auto(!v->automatic); break;
    case BENCH_FRESH: sim_host_bench_fresh(); break;
    case BENCH_GAP: pc_bench_sensor_loss(); break;
    case BENCH_SAVE: pc_bench_checkpoint(); break;
    case BENCH_RESTART: pc_bench_restart(restart_mode); break;
    case BENCH_RESTART_MODE: restart_mode=(restart_mode+1)%6; break;
    }
}
static lv_obj_t *button(const char *text,int x,int y,int width,enum bench_action action)
{
    lv_obj_t *o=lv_button_create(lv_screen_active());
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,34);
    lv_obj_add_event_cb(o,command,LV_EVENT_CLICKED,(void *)(intptr_t)action);
    lv_obj_t *l=lv_label_create(o);lv_label_set_text(l,text);lv_obj_center(l);return l;
}
static lv_point_precise_t plot_point(struct bench_point p,const lv_area_t *a,uint32_t duration)
{
    float x,y;pc_bench_plot(p,duration,60,&x,&y);
    return (lv_point_precise_t){a->x1+40+x*(lv_area_get_width(a)-50),a->y1+22+y*(lv_area_get_height(a)-44)};
}
static void line(lv_layer_t *layer,lv_point_precise_t a,lv_point_precise_t b,unsigned color,bool dashed)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
    d.p1=a;d.p2=b;d.color=lv_color_hex(color);d.width=2;
    if(dashed){d.dash_width=5;d.dash_gap=5;}
    lv_draw_line(layer,&d);
}
static void draw_graph(lv_event_t *e)
{
    lv_layer_t *layer=lv_event_get_layer(e);lv_area_t a;lv_obj_get_coords(graph,&a);
    const struct bench_view *v=pc_bench_read();
    uint32_t duration=LV_MAX(v->duration,v->elapsed_seconds);
    if(!duration)duration=60;
    for(unsigned i=0;i<5;++i) {
        uint32_t seconds=(uint32_t)((uint64_t)duration*i/4);
        float x,y;pc_bench_plot((struct bench_point){seconds,0,false},duration,60,&x,&y);
        float px=a.x1+40+x*(lv_area_get_width(&a)-50);
        line(layer,(lv_point_precise_t){px,a.y2-22},(lv_point_precise_t){px,a.y2-18},0x8793a7,false);
    }
    for(int m=0;m<=60;m+=20) {
        line(layer,plot_point((struct bench_point){0,m,0},&a,duration),
            plot_point((struct bench_point){duration,m,0},&a,duration),0x394351,false);
    }
    if(v->replay) for(size_t i=1;i<v->profile_count;++i) {
        struct bench_point p=v->profile[i];
        if(p.gap) {
            /* No interpolation through an unknown interval. */
            lv_point_precise_t left=plot_point(v->profile[i-1],&a,duration);
            lv_point_precise_t right=plot_point(p,&a,duration);
            line(layer,(lv_point_precise_t){left.x,a.y1+8},(lv_point_precise_t){right.x,a.y1+8},0xff746b,true);
        } else line(layer,plot_point(v->profile[i-1],&a,duration),plot_point(p,&a,duration),0x8793a7,true);
    }
    /* Collapse at most two horizontal pixels, retaining every depth extremum.
     * Fixed-stride sampling can erase a short manual excursion completely. */
    lv_point_precise_t previous={0};bool have_previous=false;
    for(size_t i=0;i<v->history_count;) {
        lv_point_precise_t first=plot_point(v->history[i],&a,duration),last=first;
        float shallow=first.y,deep=first.y;
        int column=(int)first.x/2;
        ++i;
        while(i<v->history_count) {
            lv_point_precise_t next=plot_point(v->history[i],&a,duration);
            if((int)next.x/2!=column)break;
            shallow=LV_MIN(shallow,next.y);deep=LV_MAX(deep,next.y);last=next;++i;
        }
        if(have_previous)line(layer,previous,first,0x54d9cf,false);
        if(first.x!=last.x)line(layer,first,last,0x54d9cf,false);
        if(shallow!=deep)line(layer,(lv_point_precise_t){first.x,shallow},
            (lv_point_precise_t){first.x,deep},0x54d9cf,false);
        previous=last;have_previous=true;
    }
    struct bench_point current=v->history[v->history_count-1];
    if(v->data_gap.ongoing)current.seconds=v->elapsed_seconds;
    lv_point_precise_t p=plot_point(current,&a,duration);
    line(layer,(lv_point_precise_t){p.x,a.y1+18},(lv_point_precise_t){p.x,a.y2-18},v->gap?0xff746b:0xffd16a,false);
}
void pc_bench_ui_create(void)
{
    label("EXPERIMENTAL OSTC4 / SYNTHETIC EXPOSURE",636,14);
    label("Virtual watch: CALCULATED / 10 m per bar",22,422);
    pc_bench_set_observer(scuba_ui_calculated);
    scuba_ui_calculated(pc_bench_read());
    watch_ui_open_calculated_scuba();
    sim_host_bench_watch(scuba_ui_model());
    summary=label("",636,44);lv_obj_set_width(summary,548);
    recovery_label=label("",636,132);lv_obj_set_width(recovery_label,548);
    label("NITROGEN TISSUES (bar) - all 16 compartments",636,158);
    for(int i=0;i<16;++i)tissues[i]=label("",636+(i/8)*270,184+(i%8)*22);
    lv_obj_t *stop_box=lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(stop_box);lv_obj_set_pos(stop_box,636,368);lv_obj_set_size(stop_box,548,68);
    stops=lv_label_create(stop_box);lv_obj_set_width(stops,528);
    button("Save checkpoint",22,764,146,BENCH_SAVE);
    restart_mode_label=button(restart_modes[0],178,764,238,BENCH_RESTART_MODE);
    button("Restart",426,764,138,BENCH_RESTART);
    label("Depth (m, down) versus elapsed time (min:sec, right)",22,446);
    graph=lv_obj_create(lv_screen_active());lv_obj_set_pos(graph,22,466);lv_obj_set_size(graph,1156,224);
    lv_obj_remove_flag(graph,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(graph,lv_color_hex(0x171e28),0);
    lv_obj_add_event_cb(graph,draw_graph,LV_EVENT_DRAW_MAIN,NULL);
    label("0 m",26,482);label("20",26,542);label("40",26,602);label("60",26,662);
    for(unsigned i=0;i<5;++i) {
        time_ticks[i]=label("",0,670);lv_obj_set_width(time_ticks[i],90);
        lv_obj_set_style_text_font(time_ticks[i],&lv_font_montserrat_12,0);
        lv_obj_set_style_text_align(time_ticks[i],i==0?LV_TEXT_ALIGN_LEFT:i==4?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_CENTER,0);
    }
    clock_label=label("",22,696);
    pause_label=button("Pause",22,726,102,BENCH_PAUSE);
    speed_label=button("Time x1",134,726,110,BENCH_SPEED);
    button("Rate -1",254,726,108,BENCH_ASCEND_RATE);button("Rate +1",372,726,108,BENCH_DESCEND_RATE);
    button("Hold",490,726,88,BENCH_HOLD);button("Surface",588,726,100,BENCH_SURFACE);
    button("Depth -1",698,726,106,BENCH_SHALLOWER);button("Depth +1",814,726,106,BENCH_DEEPER);
    button("Sensor loss",930,726,110,BENCH_GAP);button("FRESH replay",1050,726,128,BENCH_FRESH);
    auto_label=button("Auto ascent",1048,690,130,BENCH_AUTO);
    label("Cyan: actual | Dashed: replay | Yellow: position | Red: gap",578,774);
    last_host_ms=k_uptime_get();
}
static void result_time(char *buffer,size_t size,int seconds,ostc4_availability validity)
{
    if(validity==OSTC4_AVAILABLE||validity==OSTC4_LIMITED)
        snprintf(buffer,size,"%d s (%s)",seconds,ostc4_availability_name(validity));
    else snprintf(buffer,size,"%s",ostc4_availability_name(validity));
}
void pc_bench_ui_poll(void)
{
    uint64_t now=k_uptime_get();pc_bench_poll(now-last_host_ms);last_host_ms=now;
    if(now-last_paint<100)return;
    last_paint=now;
    const struct bench_view *v=pc_bench_read();
    char ndl[60],tts[60];result_time(ndl,sizeof(ndl),v->result.ndl_seconds,v->result.ndl);
    result_time(tts,sizeof(tts),v->result.tts_seconds,v->result.tts);
    if(v->result.tts==OSTC4_AVAILABLE && v->result.tts_seconds==0)
        snprintf(tts,sizeof(tts),"0 s (NDL; travel excluded)");
    lv_label_set_text_fmt(summary,"%s | exposure: %s\nNDL: %s | TTS: %s\nCeiling: %.2f m (%s)\n%.5f bar absolute | surface %.5f bar | water %.0f kg/m3\nGF 30/70; ascent 9 m/min; watch/engine depths: 10 m/bar",
        v->error?v->error:v->ended?"REPLAY COMPLETE":v->paused?"PAUSED":v->replay?"PRESSURE REPLAY":v->automatic?"AUTO ASCENT":"MANUAL",
        ostc4_availability_name(v->result.tissues),ndl,tts,(double)v->result.ceiling_meters,
        ostc4_availability_name(v->result.ceiling),(double)v->result.absolute_pressure_bar,
        (double)v->surface_bar,(double)v->density_kg_m3);
    for(int i=0;i<16;++i)lv_label_set_text_fmt(tissues[i],"N2 %02d: %.6f  [%s]",i+1,
        (double)v->result.nitrogen_bar[i],ostc4_availability_name(v->result.tissues));
    char buffer[1024];int n=snprintf(buffer,sizeof(buffer),"Mandatory stops (%s):",ostc4_availability_name(v->result.stops));
    bool any=false;
    if(v->result.stops==OSTC4_AVAILABLE) for(int i=OSTC4_STOPS-1;i>=0;--i) {
        if(!v->result.stop_seconds[i])continue;
        any=true;
        if(n<(int)sizeof(buffer)-32)n+=snprintf(buffer+n,sizeof(buffer)-n," %dm:%us",(i+1)*3,v->result.stop_seconds[i]);
    }
    if(!any&&v->result.stops==OSTC4_AVAILABLE)snprintf(buffer+n,sizeof(buffer)-n," none");
    lv_label_set_text(stops,buffer);
    uint32_t duration=LV_MAX(v->duration,v->elapsed_seconds);
    if(!duration)duration=60;
    for(unsigned i=0;i<5;++i) {
        uint32_t seconds=(uint32_t)((uint64_t)duration*i/4);
        int x=62+(int)((uint64_t)seconds*1106/duration);
        lv_obj_set_x(time_ticks[i],i==0?x:i==4?x-90:x-45);
        lv_label_set_text_fmt(time_ticks[i],"%u:%02u",seconds/60,seconds%60);
    }
    char gap[80]="";
    if(v->data_gap.present) {
        if(v->data_gap.time_known)snprintf(gap,sizeof(gap)," | %s %u:%02u",
            v->data_gap.ongoing?"NO DATA":"HISTORY GAP",
            v->data_gap.duration_seconds/60,v->data_gap.duration_seconds%60);
        else snprintf(gap,sizeof(gap)," | GAP DURATION UNKNOWN");
    }
    lv_label_set_text_fmt(clock_label,"Elapsed %u:%02u | depth %.2f m | last sample %u:%02u%s",
        v->elapsed_seconds/60,v->elapsed_seconds%60,
        (double)v->depth_m,v->seconds/60,v->seconds%60,gap);
    lv_label_set_text(recovery_label,v->recovery?v->recovery:"FRESH synthetic fixture; no recovered history");
    lv_label_set_text(restart_mode_label,restart_modes[restart_mode]);
    lv_label_set_text(auto_label,v->automatic?"Auto: ON":"Auto ascent");
    lv_label_set_text(pause_label,v->paused?"Resume":"Pause");
    lv_label_set_text_fmt(speed_label,"Time x%u",v->speed);
    lv_obj_invalidate(graph);
}
