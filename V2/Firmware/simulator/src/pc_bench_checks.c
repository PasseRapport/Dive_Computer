/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "pc_bench_checkpoint_file.h"
#include "water_settings.h"
#include "pc_bench_ui.h"
#include "sim_host.h"
#include "sim_controls.h"
#include "scuba_ui.h"
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <math.h>

/* Public bench inputs plus real watch menu commands share one recording. */
static void recording_checks(void)
{
    __ASSERT(pc_bench_fresh(1,1000,NULL,0)==0,"recording fixture failed");
    const struct bench_view *v=pc_bench_read();
    const struct scuba_model *m=scuba_ui_model();
    struct watch_data data={.temperature_c=18,.battery_percent=87};
    scuba_ui_enter(0);
    scuba_ui_input(WATCH_SINGLE_TAP,0);
    scuba_ui_input(WATCH_CROWN,-100);scuba_ui_input(WATCH_CROWN,3);
    scuba_ui_input(WATCH_SINGLE_TAP,0);scuba_ui_input(WATCH_CROWN,2);
    __ASSERT(v->recording.start_depth_dm==30 && m->start_depth_dm==30,"watch setting missed controller");
    scuba_ui_input(WATCH_DOUBLE_TAP,0);scuba_ui_input(WATCH_DOUBLE_TAP,0);
    pc_bench_manual(2.5f,0);pc_bench_poll(1000);
    __ASSERT(!v->recording.active && !m->active,"watch started below selected threshold");
    pc_bench_manual(3.2f,0);pc_bench_poll(1000);
    scuba_ui_update(&data,0,true);
    __ASSERT(v->recording.active && m->active && v->recording.started_seconds==2 &&
        scuba_ui_screen()==SCUBA_MAIN,"shared recording did not open main screen");
    pc_bench_manual(0,0);pc_bench_poll(299000);
    __ASSERT(v->recording.active && m->active && m->surface_ms==299000,"brief return completed recording");
    pc_bench_manual(3.2f,0);pc_bench_poll(1000);
    __ASSERT(v->recording.dives_started==1 && m->surface_ms==0,"brief return started another dive");
    pc_bench_manual(0,0);pc_bench_poll(1000);
    ostc4_observation before=v->result;
    watch_ui_update(&data);
    scuba_ui_input(WATCH_CROWN,2);
    __ASSERT(scuba_ui_input(WATCH_CROWN,24),"surface manual exit failed");
    __ASSERT(!v->recording.active && !v->recording.armed && !m->active && !m->armed &&
        m->summary.valid && m->summary.duration_s==1,"manual exit missed shared lifecycle");
    __ASSERT(v->result.timestamp_seconds==before.timestamp_seconds &&
        v->result.nitrogen_bar[0]==before.nitrogen_bar[0],"manual completion changed exposure");
    scuba_ui_enter(0);
    pc_bench_manual(3.2f,0);pc_bench_poll(1000);
    scuba_ui_update(&data,0,true);
    __ASSERT(v->recording.dives_started==2 && m->active,"second recording did not start");
    pc_bench_manual(0,0);pc_bench_poll(300000);
    scuba_ui_update(&data,0,true);
    __ASSERT(!v->recording.active && !m->active && m->summary.valid &&
        v->recording.dives_completed==2 && scuba_ui_screen()==SCUBA_SUMMARY,
        "five-minute completion did not open shared summary");
    __ASSERT(pc_bench_start_depth(20)==0,"restore default start threshold failed");
    printk("RECORDING LIFECYCLE PASS: watch start setting, main/summary, brief surface, manual/automatic completion, second dive\n");
}

/* Deterministic synthetic integration recipe; the current result, watch state
 * and exact accepted pressure trace are captured together after rendering. */
static void calculated_checks(void)
{
    static unsigned stage;
    static uint64_t after;
    static enum scuba_screen battery_screen;
    static uint32_t battery_seconds;
    uint64_t now=k_uptime_get();
    if(now<500 || (after && now<after))return;
    const struct bench_view *v=pc_bench_read();
    const struct scuba_model *m=scuba_ui_model();
    if(stage%2==0) {
        switch(stage/2) {
        case 0:
            recording_checks();
            __ASSERT(pc_bench_fresh(1.01325f,1025,NULL,0)==0,"fresh failed");
            pc_bench_manual(5,0);pc_bench_poll(1000);
            __ASSERT(m->calculated && m->calculated_state==SCUBA_CALCULATED_UNAVAILABLE,"missing output concealed");break;
        case 1: {
            const ostc4_sample shallow[]={{0,1.01325f},{60,1.81325f},{180,1.81325f}};
            __ASSERT(pc_bench_fresh(1.01325f,1025,shallow,3)==0,"shallow fixture failed");
            pc_bench_poll(120000);
            __ASSERT((m->calculated_state==SCUBA_CALCULATED_AVAILABLE||m->calculated_state==SCUBA_CALCULATED_LIMITED) && m->ndl_s==(unsigned)v->result.ndl_seconds,"NDL mapping failed");
            scuba_ui_input(WATCH_CROWN,1);scuba_ui_input(WATCH_SINGLE_TAP,0);
            scuba_ui_input(WATCH_CROWN,1);scuba_ui_input(WATCH_SINGLE_TAP,0);
            scuba_ui_input(WATCH_CROWN,FIELD_TTS);scuba_ui_input(WATCH_SINGLE_TAP,0);
            scuba_ui_input(WATCH_DOUBLE_TAP,0);
            __ASSERT(scuba_ui_field()==FIELD_TTS,"TTS footer selection failed");break;
        }
        case 2: {
            const ostc4_sample deep[]={{0,1.01325f},{120,4.01325f},{2220,4.01325f}};
            __ASSERT(pc_bench_fresh(1.01325f,1025,deep,3)==0,"deco fixture failed");
            pc_bench_poll(2220000);
            __ASSERT(m->calculated_stop_dm && scuba_model_tts(m)==(unsigned)v->result.tts_seconds,"calculated deco missing");break;
        }
        case 3: {
            __ASSERT(pc_bench_auto(true)==0,"auto enable failed");
            pc_bench_pause(false);
            unsigned guard=0;
            do {pc_bench_poll(1000);} while((!scuba_model_in_stop(m)||v->rate_m_min!=0) && ++guard<300);
            __ASSERT(guard<300 && v->automatic && m->calculated_stop_dm && scuba_model_in_stop(m),"auto did not reach calculated stop");break;
        }
        case 4:
            pc_bench_manual(1,0);pc_bench_pause(false);pc_bench_poll(1000);
            __ASSERT(!v->automatic && (m->alerts&SCUBA_CEILING),"manual violation missing");break;
        case 5:
            scuba_ui_input(WATCH_SINGLE_TAP,0);
            __ASSERT(!(m->notifications&SCUBA_CEILING) && (m->alerts&SCUBA_CEILING) && m->calculated_stop_dm,"ack cleared obligation");break;
        case 6:
            pc_bench_mark_missing();
            __ASSERT(m->calculated_state==SCUBA_CALCULATED_INVALID,"fault not displayed");break;
        case 7:
            __ASSERT(sim_host_bench_fresh()==0,"two-dive fixture failed");pc_bench_poll(3600000);
            __ASSERT(v->seconds==3600 && v->result.nitrogen_bar[5]>0.9f,"surface interval lost loading");break;
        case 8:
            pc_bench_pause(false);pc_bench_poll(1320000);
            __ASSERT(v->seconds==4920 && m->active && m->ndl_s==(unsigned)v->result.ndl_seconds,"second dive mapping failed");break;
        case 9: {
            const ostc4_sample deep[]={{0,1.01325f},{120,4.01325f},{2220,4.01325f}};
            __ASSERT(pc_bench_fresh(1.01325f,1025,deep,3)==0,"ceiling fixture failed");
            pc_bench_poll(2220000);pc_bench_manual(8,0);pc_bench_poll(1000);
            __ASSERT(!m->calculated_ceiling_violation && m->calculated_stop_dm>m->depth_dm,
                "ceiling/stop boundary fixture failed");break;
        }
        case 10:sim_host_push_button(1100,706,1);break;
        case 11:sim_host_push_button(1100,706,0);break;
        case 12:
            __ASSERT(v->automatic && !v->replay && v->paused,"SDL auto control failed");
            printk("CALCULATED WATCH PASS: NDL, deco, automatic stop, manual violation, acknowledgement, invalidity, two dives, SDL auto control\n");break;
        case 13:
            __ASSERT(v->paused && m->battery==87,"battery fixture must be paused");
            battery_screen=scuba_ui_screen();battery_seconds=v->seconds;
            sim_host_push_wheel(400,285,1);break;
        case 14:
            __ASSERT(m->battery==88,"battery wheel must increment by one");
            sim_host_push_wheel(400,285,-1);break;
        case 15:
            __ASSERT(m->battery==87,"battery wheel must decrement by one");
            sim_host_push_wheel(400,285,100);break;
        case 16:
            __ASSERT(m->battery==100,"battery must clamp at 100 percent");
            sim_host_push_wheel(400,285,-101);break;
        case 17:
            __ASSERT(m->battery==0 && (m->alerts&SCUBA_BATTERY),"battery must clamp at zero and warn while paused");
            sim_host_push_wheel(400,285,1);break;
        case 18:
            __ASSERT(m->battery==1,"battery wheel must leave zero one percent at a time");
            sim_host_push_wheel(400,285,86);break;
        case 19:
            __ASSERT(m->battery==87 && !(m->alerts&SCUBA_BATTERY),"battery must clear low-battery warning");
            __ASSERT(scuba_ui_screen()==battery_screen && v->seconds==battery_seconds && v->paused,
                "battery wheel changed the watch page or exposure");
            printk("BATTERY WHEEL PASS: real SDL events, one-percent steps, 0/100 bounds, paused updates, crown isolation\n");break;
        case 20:
            __ASSERT(pc_bench_fresh(1.01325f,1025,NULL,0)==0,"apnea fixture failed");
            pc_bench_manual(5,0);pc_bench_pause(false);pc_bench_poll(1000);
            /* Return to the mode selector at the surface, then select apnea. */
            pc_bench_manual(0,0);pc_bench_poll(1000);
            watch_ui_input(WATCH_CROWN,100);watch_ui_input(WATCH_CROWN,24);
            __ASSERT(watch_ui_view()==WATCH_MODE_MENU,"cannot leave scuba fixture");
            watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);
            watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,1);
            watch_ui_input(WATCH_SINGLE_TAP,0);
            __ASSERT(watch_ui_view()==WATCH_DIVE_VIEW && watch_ui_dive_mode()==DIVE_FREE,"apnea selection failed");
            pc_bench_manual(5,0);pc_bench_poll(1000);pc_bench_pause(true);
            sim_host_push_button(420,744,1);break;
        case 21:sim_host_push_button(420,744,0);break;
        case 22:
            __ASSERT(v->rate_m_min==1 && !v->replay,"apnea Rate +1 control failed");
            pc_bench_pause(false);pc_bench_poll(60000);pc_bench_pause(true);
            __ASSERT(sim_controls_depth_dm()==60 && sim_controls_depth_rate()==1,"apnea depth did not follow rate");
            sim_host_push_button(530,744,1);break;
        case 23:sim_host_push_button(530,744,0);break;
        case 24:
            __ASSERT(v->depth_m>5.99f && v->depth_m<6.01f && v->rate_m_min==0,"apnea Hold control failed");
            pc_bench_pause(false);pc_bench_poll(1000);pc_bench_pause(true);
            __ASSERT(sim_controls_depth_dm()==60 && sim_controls_depth_rate()==0,"apnea Hold changed depth");break;
        case 25:
            __ASSERT(watch_ui_view()==WATCH_DIVE_VIEW,"bench controls navigated away from apnea");
            __ASSERT(sim_host_bench_capture(11)==0,"apnea Hold capture failed");
            printk("APNEA BENCH PASS: real Rate/Hold clicks drive the shared depth, paused exposure and watch view preserved\n");break;
        default:return;
        }
        if(stage/2<10)pc_bench_pause(true);
        after=now+300;++stage;
    } else {
        if(stage/2==22) __ASSERT(sim_host_bench_capture(10)==0,"apnea rate capture failed");
        if(stage/2<10) {
            /* Render the observer's current model, then finish any fade. */
            struct watch_data data={.temperature_c=18,.battery_percent=87};
            watch_ui_update(&data);lv_refr_now(NULL);
            __ASSERT(v->seconds==v->result.timestamp_seconds,"exposure and watch clocks diverged");
            __ASSERT(sim_host_bench_capture(stage/2)==0,"calculated capture failed");
        }
        after=now+300;++stage;
    }
}

/* Every fault is deliberate PC adapter input, not a physical power-loss test. */
static void restart_checks(void)
{
    static unsigned stage;
    static uint64_t after;
    uint64_t now=k_uptime_get();
    if(now<500 || now<after || stage>=18)return;
    unsigned scenario=stage/2;
    const struct bench_view *v=pc_bench_read();
    if(stage%2==0) {
        const ostc4_sample deep[]={{0,1.01325f},{120,4.01325f},{2220,4.01325f}};
        __ASSERT(pc_bench_fresh(1.01325f,1025,deep,3)==0,"fresh failed");
        if(scenario<6) {
            pc_bench_poll(1920000);
            __ASSERT(pc_bench_checkpoint()==0,"save failed");
            pc_bench_poll(300000);
            ostc4_observation control=v->result;
            const ostc4_restart_status expected[]={OSTC4_RESTORED,OSTC4_RESTART_EXPOSURE_UNKNOWN,
                OSTC4_RESTART_TIME_UNKNOWN,OSTC4_CHECKPOINT_CORRUPT,
                OSTC4_CHECKPOINT_INCOMPATIBLE,OSTC4_CHECKPOINT_STALE};
            __ASSERT(pc_bench_restart((enum pc_restart_mode)scenario)==expected[scenario],"wrong restart reason");
            if(!scenario) {
                __ASSERT(v->result.nitrogen_bar[0]==control.nitrogen_bar[0] &&
                    v->result.tts_seconds==control.tts_seconds && v->result.tts==OSTC4_AVAILABLE,"recovery changed exposure");
            } else __ASSERT(scuba_ui_model()->calculated_state==SCUBA_CALCULATED_INVALID,"restart concealed invalidity");
        }
        if(scenario==7) {
            pc_bench_poll(2220000);pc_bench_sensor_loss();pc_bench_poll(125000);
            __ASSERT(v->data_gap.time_known && v->data_gap.duration_seconds==125 &&
                v->elapsed_seconds==2345 && v->seconds==2220,"sensor-loss timing wrong");
        }
        if(scenario==8) {
            const ostc4_sample missing_profile[]={{0,1.01325f},{4000,2.01325f}};
            __ASSERT(pc_bench_fresh(1.01325f,1025,missing_profile,2)==0,"missing fixture failed");
            pc_bench_poll(1000);
            __ASSERT(v->data_gap.duration_seconds==4000 && v->elapsed_seconds==4000 &&
                v->seconds==0,"known replay gap timing wrong");
        }
        pc_bench_pause(true);after=now+300;
    } else {
        struct watch_data data={.temperature_c=18,.battery_percent=87};
        watch_ui_update(&data);lv_refr_now(NULL);
        __ASSERT(sim_host_bench_capture(scenario)==0,"restart capture failed");
        if(scenario==8)printk("RESTART WATCH PASS: complete recovery, missing exposure, unknown time, corrupt, incompatible, stale, explicit fresh fixture\n");
        after=now+300;
    }
    ++stage;
}

/* Rendering contract at the public LVGL display boundary: exact text and
 * actual native glyph bounds, without depending on dashboard object layout. */
static lv_obj_t *visible_label(lv_obj_t *parent,const char *text)
{
    if(!lv_obj_is_visible(parent))return NULL;
    if(lv_obj_check_type(parent,&lv_label_class) && !strcmp(lv_label_get_text(parent),text))return parent;
    for(unsigned i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *match=visible_label(lv_obj_get_child(parent,i),text);
        if(match)return match;
    }
    return NULL;
}
static void paired_footer_render(const char *expected)
{
    lv_obj_t *title=visible_label(lv_screen_active(),"CEIL GF LOW / HIGH");
    lv_obj_t *value=visible_label(lv_screen_active(),expected);
    __ASSERT(title && value,"both endpoint labels/rounded values must render together");
    lv_area_t area;lv_obj_get_coords(value,&area);
    __ASSERT(area.y1>=250 && area.y2<330,"paired footer left the main watch bottom field");
    const int x[]={area.x1,area.x2},y[]={area.y1,area.y2};
    for(unsigned i=0;i<2;++i)for(unsigned j=0;j<2;++j) {
        int dx=x[i]-182,dy=y[j]-182;
        __ASSERT(dx*dx+dy*dy<148*148,"endpoint typography clips the native round watch geometry");
    }
}

/* Public watch commands demonstrate atomic preparation editing and start lock. */
/* Same accepted pressure trace: density changes physical presentation only.
 * Independent decimal values use g=9.80665; binary32 depth tolerance 1e-4m.
 * Bounds are checked at existing sensor-millimetre resolution, not rounded dm. */
static void pressure_water_checks(void)
{
    const struct bench_view *v=pc_bench_read();
    const struct scuba_model *m=scuba_ui_model();
    ostc4_observation fresh_result;
    ostc4_sample fresh_trace[3];
    float fresh_depth=0;
    for(unsigned salt=0;salt<2;++salt) {
        __ASSERT(!pc_bench_fresh(1.01325f,salt?1025:1000,NULL,0),"pressure water fixture failed");
        __ASSERT(!pc_bench_set_gf_pair(30,70),"pressure water pair failed");
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){120,4.01325f}) &&
            !pc_bench_accept_sample(&(ostc4_sample){2220,4.01325f}),"pressure trace rejected");
        if(!salt) {
            fresh_result=v->result;fresh_depth=v->depth_m;
            memcpy(fresh_trace,v->exposure,sizeof(fresh_trace));
        } else {
            __ASSERT(!memcmp(&fresh_result,&v->result,sizeof(fresh_result)) &&
                !memcmp(fresh_trace,v->exposure,sizeof(fresh_trace)),"Density rewrote accepted trace/tissues/results");
            __ASSERT(fabsf(fresh_depth-v->depth_m*1.025f)<0.0001f,"Pressure depth density ratio failed");
        }
        __ASSERT(fabsf(m->calculated_gf_low_ceiling_m-(salt?13.29321634f:13.62554675f))<0.0001f &&
            fabsf(m->calculated_gf_high_ceiling_m-(salt?7.21170658f:7.39199925f))<0.0001f,
            "GF endpoints must be physical metres");
        __ASSERT(m->calculated_ceiling_dm==(salt?72:74) && m->calculated_stop_dm==(salt?119:122),
            "Operational ceiling/mandatory stop physical rounding failed");
        pc_bench_pause(true);pc_bench_ui_poll();
        struct watch_data data={.temperature_c=18,.battery_percent=87};
        scuba_ui_update(&data,(uint64_t)v->seconds*1000,true);lv_refr_now(NULL);
        __ASSERT(!sim_host_bench_capture(13+salt),"Same-pressure capture failed");
        /* Public replay retains exact supplied pressures, including stop edges. */
        int slot=OSTC4_STOPS-1;
        while(slot>=0 && !v->result.stop_seconds[slot])--slot;
        __ASSERT(slot>=0,"pressure fixture has no stop");
        float lower=1.01325f+0.3f*(slot+1),upper=1.01325f+0.3f*(slot+2);
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){2221,upper}) &&
            scuba_model_in_stop(m),"Exact upper pressure edge excluded");
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){2222,upper+0.001f}) &&
            !scuba_model_in_stop(m),"Above upper pressure edge credited");
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){2223,lower}) &&
            scuba_model_in_stop(m),"Exact lower pressure edge excluded");
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){2224,lower-0.001f}) &&
            !scuba_model_in_stop(m),"Below lower pressure edge credited");
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){2225,4.01325f}) && !pc_bench_auto(true),
            "Physical automatic target setup failed");
        pc_bench_pause(false);unsigned guard=0;
        do {pc_bench_poll(1000);} while((!scuba_model_in_stop(m)||v->rate_m_min!=0) && ++guard<300);
        __ASSERT(guard<300 && fabsf(v->depth_m-m->calculated_stop_lower_m)<0.0001f,
            "Automatic pressure target disagrees with watch stop");
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){v->seconds+1,7.01325f}) &&
            m->calculated_state!=SCUBA_CALCULATED_INVALID,
            "Valid +6bar pressure envelope invalidated physical watch");
        __ASSERT(pc_bench_accept_sample(&(ostc4_sample){v->seconds+1,7.01425f})==-1,
            "Pressure above +6bar envelope accepted");
    }
    printk("PRESSURE WATER PASS: identical trace/tissues/raw results, physical GF endpoints and inclusive pressure stop edges in Fresh/Marine\n");
}

static void gf_checks(void)
{
    static unsigned stage;
    static uint64_t after;
    uint64_t now=k_uptime_get();
    if(stage>=26 || now<500 || (after && now<after))return;
    const struct bench_view *v=pc_bench_read();
    struct watch_data data={.temperature_c=18,.battery_percent=87};
    if(!(stage%2)) {
        switch(stage/2) {
        case 0:
            __ASSERT(pc_bench_fresh(1.01325f,1025,NULL,0)==0,"GF fresh failed");
            pc_bench_pause(true);scuba_ui_enter(0);break;
        case 1:
            scuba_ui_input(WATCH_SINGLE_TAP,0);
            scuba_ui_input(WATCH_CROWN,-100);scuba_ui_input(WATCH_CROWN,4);
            scuba_ui_input(WATCH_SINGLE_TAP,0);
            __ASSERT(scuba_ui_screen()==SCUBA_GF_EDIT && v->gf_editing,"GF editor did not open");
            scuba_ui_input(WATCH_CROWN,10);
            __ASSERT(v->draft_gf.low==40 && v->preferred_gf.low==30,"draft leaked into committed settings");break;
        case 2:
            scuba_ui_input(WATCH_SINGLE_TAP,0);scuba_ui_input(WATCH_CROWN,15);
            scuba_ui_input(WATCH_SINGLE_TAP,0);
            __ASSERT(v->draft_gf.high==85 && v->preferred_gf.high==70,"high draft leaked");break;
        case 3:
            scuba_ui_input(WATCH_SINGLE_TAP,0);scuba_ui_input(WATCH_DOUBLE_TAP,0);
            __ASSERT(v->preferred_gf.low==40 && v->preferred_gf.high==85 &&
                !v->gf_editing && scuba_ui_screen()==SCUBA_PREP,"atomic editor commit failed");break;
        case 4:
            scuba_ui_input(WATCH_SINGLE_TAP,0);scuba_ui_input(WATCH_CROWN,-100);
            scuba_ui_input(WATCH_CROWN,4);scuba_ui_input(WATCH_SINGLE_TAP,0);
            scuba_ui_input(WATCH_CROWN,-5);
            __ASSERT(pc_bench_accept_sample(&(ostc4_sample){60,1.31325f})==0,"GF start input failed");
            __ASSERT(v->gf_locked && !v->gf_editing && scuba_ui_screen()==SCUBA_MAIN &&
                v->active_gf.low==40 && v->active_gf.high==85,"start failed to discard draft/lock committed pair");
            __ASSERT(pc_bench_set_gf_pair(30,70)==-1 &&
                ostc4_exposure_set_gf_pair(30,70)==OSTC4_BAD_INPUT,"direct setting bypassed lock");break;
        case 5:
            __ASSERT(pc_bench_accept_sample(&(ostc4_sample){90,1.01325f})==0,"surface failed");
            __ASSERT(v->gf_locked,"brief return unlocked pair");
            __ASSERT(pc_bench_recording_finish()==0,"manual completion failed");
            scuba_ui_update(&data,(uint64_t)v->seconds*1000,true);
            scuba_ui_enter((uint64_t)v->seconds*1000);
            __ASSERT(!v->gf_locked && v->preferred_gf.low==40 && scuba_ui_screen()==SCUBA_PREP,
                "completion did not unlock retained pair");break;
        case 6:
            __ASSERT(pc_bench_fresh(1.01325f,1025,NULL,0)==0,"endpoint fresh failed");
            __ASSERT(pc_bench_set_gf_pair(30,70)==0,"endpoint pair failed");
            __ASSERT(pc_bench_accept_sample(&(ostc4_sample){60,1.31325f})==0,"endpoint shallow failed");
            scuba_ui_update(&data,(uint64_t)v->seconds*1000,true);
            scuba_ui_input(WATCH_CROWN,1);scuba_ui_input(WATCH_SINGLE_TAP,0);
            scuba_ui_input(WATCH_CROWN,1);scuba_ui_input(WATCH_SINGLE_TAP,0);
            scuba_ui_input(WATCH_CROWN,-100);scuba_ui_input(WATCH_CROWN,FIELD_GF_CEILINGS);
            scuba_ui_input(WATCH_SINGLE_TAP,0);scuba_ui_input(WATCH_DOUBLE_TAP,0);
            __ASSERT(scuba_ui_screen()==SCUBA_MAIN && scuba_ui_field()==FIELD_GF_CEILINGS,
                "paired endpoint choice missing from existing Bottom field menu");break;
        case 7:
            __ASSERT(pc_bench_fresh(1.01325f,1025,NULL,0)==0,"endpoint deep fresh failed");
            __ASSERT(pc_bench_set_gf_pair(30,70)==0,"endpoint deep pair failed");
            __ASSERT(pc_bench_accept_sample(&(ostc4_sample){120,4.01325f})==0,"endpoint descent failed");
            __ASSERT(pc_bench_accept_sample(&(ostc4_sample){2220,4.01325f})==0,"endpoint loading failed");
            scuba_ui_update(&data,(uint64_t)v->seconds*1000,true);
            __ASSERT(scuba_ui_field()==FIELD_GF_CEILINGS && scuba_ui_model()->calculated_stop_dm,
                "mandatory stop hid selected endpoints");break;
        case 8: {
            __ASSERT(pc_bench_auto(true)==0,"endpoint auto failed");pc_bench_pause(false);
            unsigned guard=0;
            do {pc_bench_poll(1000);} while((!scuba_model_in_stop(scuba_ui_model())||v->rate_m_min!=0) && ++guard<300);
            __ASSERT(guard<300 && scuba_ui_field()==FIELD_GF_CEILINGS &&
                scuba_ui_model()->calculated_stop_remaining_ms,"endpoint stop countdown missing");
            scuba_ui_input(WATCH_SINGLE_TAP,0);break;
        }
        case 9:
            pc_bench_manual(1,0);pc_bench_pause(false);pc_bench_poll(1000);
            __ASSERT(scuba_ui_model()->notifications&SCUBA_CEILING,"endpoint display suppressed operational alert");break;
        case 10:
            scuba_ui_input(WATCH_SINGLE_TAP,0);
            __ASSERT(!(scuba_ui_model()->notifications&SCUBA_CEILING) &&
                (scuba_ui_model()->alerts&SCUBA_CEILING),"endpoint acknowledgement changed obligation");break;
        case 11:
            pc_bench_mark_missing();
            __ASSERT(!scuba_ui_model()->calculated_gf_low_ceiling_valid &&
                !scuba_ui_model()->calculated_gf_high_ceiling_valid,"invalid endpoint fabricated zero");break;
        case 12:
            __ASSERT(pc_bench_fresh(1.01325f,1025,NULL,0)==0,"equal endpoint fresh failed");
            __ASSERT(pc_bench_set_gf_pair(1,1)==0,"equal endpoint pair failed");
            __ASSERT(pc_bench_accept_sample(&(ostc4_sample){120,7.01325f})==0,"equal endpoint descent failed");
            __ASSERT(pc_bench_accept_sample(&(ostc4_sample){3720,7.01325f})==0,"equal endpoint loading failed");
            scuba_ui_update(&data,(uint64_t)v->seconds*1000,true);
            __ASSERT(v->result.gf_low_ceiling_meters>30 &&
                v->result.gf_low_ceiling_meters==v->result.gf_high_ceiling_meters &&
                scuba_ui_field()==FIELD_GF_CEILINGS,"large/equal endpoint display missing");break;

        }
        pc_bench_pause(true);after=now+350;
    } else {
        watch_ui_update(&data);lv_refr_now(NULL);
        /* Values chosen from independent endpoint fixtures/reference, with
         * decimal expectations written explicitly rather than reformatting C. */
        if(stage/2==6)paired_footer_render("0.0 / 0.0 m");
        if(stage/2==7)paired_footer_render("13.3 / 7.2 m");
        if(stage/2==11)paired_footer_render("-- / -- m");
        if(stage/2==12)paired_footer_render("44.1 / 44.1 m");
        __ASSERT(sim_host_bench_capture(stage/2)==0,"GF demonstration capture failed");
        if(stage==25)printk("PAIRED CEILINGS WATCH PASS: simultaneous fixed Low/High, zero, equality, large values, mandatory stop/countdown, operational alerts and acknowledgement, invalidity\n");
        if(stage==11)printk("CONFIGURABLE GF WATCH PASS: draft, atomic commit, preparation pair, first-sample lock, direct rejection, completion unlock\n");
        after=now+150;
    }
    ++stage;
}

/* Native rendering check, driven by real SDL button events. Numerical checks
 * remain in tests/bench with no LVGL or Zephyr dependency. */
/* Real SDL clicks exercise explicit disk controls and their clock assertion. */
static void checkpoint_file_checks(void)
{
    static unsigned stage;static uint64_t after;
    uint64_t now=k_uptime_get();if(now<500 || (after && now<after))return;
    after=now+200;
    const struct bench_view *v=pc_bench_read();
    switch(stage++) {
    case 0:
        __ASSERT(pc_bench_fresh(1,1000,NULL,0)==0,"disk fresh failed");
        __ASSERT(pc_bench_set_gf_pair(40,85)==0,"disk pair failed");
        __ASSERT(pc_bench_accept_sample(&(ostc4_sample){60,4})==0,"disk descent failed");
        __ASSERT(pc_bench_accept_sample(&(ostc4_sample){1260,4})==0,"disk hold failed");
        pc_bench_pause(true);sim_host_push_button(694,780,1);break;
    case 1:sim_host_push_button(694,780,0);break;
    case 2:
        __ASSERT(v->recovery && strstr(v->recovery,"Disk checkpoint saved"),"disk save button failed");
        sim_host_push_button(834,780,1);break;
    case 3:sim_host_push_button(834,780,0);break;
    case 4:
        __ASSERT(v->gap && v->result.tissues==OSTC4_INVALID &&
            strstr(v->recovery,"Unknown restart time"),"disk load invented restart clock");
        lv_refr_now(NULL);__ASSERT(sim_host_bench_capture(0)==0,"disk fault capture failed");break;
    case 5:
        __ASSERT(pc_bench_fresh(1,1025,NULL,0)==0,"disk preferred fresh failed");
        __ASSERT(pc_bench_set_gf_pair(20,90)==0,"disk preferred pair failed");
        pc_bench_pause(true);sim_host_push_button(1040,780,1);break;
    case 6:sim_host_push_button(1040,780,0);break;
    case 7:lv_refr_now(NULL);__ASSERT(sim_host_bench_capture(1)==0,"assertion capture failed");
        sim_host_push_button(834,780,1);break;
    case 8:sim_host_push_button(834,780,0);break;
    case 9:
        __ASSERT(v->gf_locked && v->active_gf.low==40 && v->preferred_gf.low==20 &&
            v->seconds==1260 && !v->error,"disk recovered lock/pair failed");
        __ASSERT(v->density_kg_m3==1000 && v->recording.density_kg_m3==1000 && !water_settings_salt(),"disk active water not restored");
        lv_refr_now(NULL);__ASSERT(sim_host_bench_capture(2)==0,"disk locked capture failed");break;
    case 10:
        __ASSERT(pc_bench_accept_sample(&(ostc4_sample){1440,1})==0,"disk surface failed");
        __ASSERT(v->gf_locked,"brief surface unlocked recovered dive");
        __ASSERT(pc_bench_recording_finish()==0,"disk finish failed");
        __ASSERT(!v->gf_locked && pc_bench_set_gf_pair(20,90)==0,"disk later unlock failed");
        __ASSERT(v->density_kg_m3==1025 && water_settings_salt(),"completion lost next water preference");break;
    case 11:
        lv_refr_now(NULL);__ASSERT(sim_host_bench_capture(3)==0,"disk unlocked capture failed");
        printk("DISK CHECKPOINT UI PASS: visible Save disk/Load disk; default unknown clock invalid; explicit saved-time assertion; recovered40/85 locked with next20/90 preference; surface completion unlock\n");break;
    }
}
static void startup_checks(void)
{
    static unsigned stage;
    const uint64_t times[]={500,1500,2300,10000,10500,11000,11200,11400,22000,22500,23000};
    uint64_t now=k_uptime_get();
    if(stage>=sizeof(times)/sizeof(times[0]) || now<times[stage])return;
    const struct bench_view *v=pc_bench_read();
    const struct scuba_model *m=scuba_ui_model();
    if(stage<8)__ASSERT(v->paused && v->seconds==0 && v->depth_m==0 &&
        !v->recording.active && !m->active,"Startup advanced exposure without Start");
    switch(stage++) {
    case 0:case 1:
        __ASSERT(watch_ui_view()==WATCH_BOOT,"Startup skipped logo animation");break;
    case 2:case 3:
        __ASSERT(watch_ui_view()==WATCH_CLOCK,"Startup did not remain on clock");break;
    case 4:
        watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_CROWN,24);
        watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);
        watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);
        watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 5:
        __ASSERT(watch_ui_view()==WATCH_SCUBA_VIEW && scuba_ui_screen()==SCUBA_PREP,
            "Selecting scuba should wait at preparation");break;
    case 6:sim_host_push_button(60,742,1);break;
    case 7:sim_host_push_button(60,742,0);break;
    case 8:
        __ASSERT(!v->paused && v->seconds>0 && v->depth_m>0 &&
            v->recording.active && m->active,"Explicit Start did not launch dive");
        printk("STARTUP PASS: logo animation, clock idle, preparation idle, explicit SDL Start\n");break;
    case 9:pc_bench_pause(true);pc_bench_ui_poll();break;
    case 10:
        lv_refr_now(NULL);
        __ASSERT(!sim_host_bench_capture(10),"settled startup capture failed");
        printk("STARTUP SETTLED CAPTURE PASS: imported pressure profile observation after explicit Start\n");break;
    }
    if(stage<=4)__ASSERT(!sim_host_bench_capture(stage),"Startup capture failed");
}

/* Independent hydrostatic examples from SPEC-densite-eau A05-A08.
 * Pressure tolerance 1e-6 bar accounts for binary32 input; physical depth
 * tolerance 1e-4 m precedes existing decimetre display truncation. */
static void water_select(int row, bool commit)
{
    scuba_ui_enter(0);
    scuba_ui_input(WATCH_SINGLE_TAP,0);
    scuba_ui_input(WATCH_CROWN,-100);
    scuba_ui_input(WATCH_SINGLE_TAP,0);
    scuba_ui_input(WATCH_CROWN,-100);
    scuba_ui_input(WATCH_CROWN,row);
    scuba_ui_input(commit?WATCH_SINGLE_TAP:WATCH_DOUBLE_TAP,0);
    scuba_ui_input(WATCH_DOUBLE_TAP,0);
}
/* Independent pressure literals: physical millimetre neighbours of the 2 m
 * start, 10 m alarm and .5 m surface boundaries; velocities use 11 m after
 * ten seconds and 10.9 m one second later. No conversion helper is the oracle. */
static void water_recording_fixture(unsigned salt,unsigned samples)
{
    const float pressures[2][8]={
        {1.1960349335f,1.196133f,1.9805669335f,1.980665f,
         2.0787315f,2.06892485f,1.04903325f,1.0489351835f},
        {1.2009358068375f,1.201036325f,2.0050811068375f,2.005181625f,
         2.1056997875f,2.09564797125f,1.05025908125f,1.0501585630875f}};
    const unsigned seconds[]={1,2,3,4,14,15,16,17};
    const struct bench_view *v=pc_bench_read();
    const struct scuba_model *m=scuba_ui_model();
    __ASSERT(!pc_bench_fresh(1,salt?1025:1000,NULL,0),"Threshold fixture rejected");
    __ASSERT(!pc_bench_start_depth(20),"Start threshold rejected");
    scuba_ui_enter(0);scuba_ui_input(WATCH_SINGLE_TAP,0);
    scuba_ui_input(WATCH_CROWN,-100);scuba_ui_input(WATCH_CROWN,1);
    scuba_ui_input(WATCH_SINGLE_TAP,0);scuba_ui_input(WATCH_CROWN,-100);
    scuba_ui_input(WATCH_CROWN,10);scuba_ui_input(WATCH_DOUBLE_TAP,0);
    scuba_ui_input(WATCH_DOUBLE_TAP,0);
    __ASSERT(m->depth_alarm_dm==100,"Public 10 m depth alarm edit failed");
    for(unsigned i=0;i<samples;++i) {
        __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){seconds[i],pressures[salt][i]}),"Threshold sample rejected");
        if(i==0)__ASSERT(!v->recording.active && !m->active,"1.999 m started recording");
        if(i==1)__ASSERT(v->recording.active && m->active && m->depth_dm==20,"2.000 m failed inclusive start");
        if(i==2)__ASSERT(m->depth_dm==99 && !(m->alerts&SCUBA_DEPTH),"9.999 m triggered 10 m alarm");
        if(i==3)__ASSERT(m->depth_dm==100 && (m->alerts&SCUBA_DEPTH),"10.000 m missed inclusive alarm");
        if(i==4)__ASSERT(m->depth_dm==110 && m->ascent_dm_min==-60,"Physical descent rate differs from 6 m/min");
        if(i==5)__ASSERT(m->depth_dm==109 && m->ascent_dm_min==60,"Physical ascent rate differs from 6 m/min");
        if(i==6)__ASSERT(!v->recording.surface_seconds && pc_bench_recording_finish()==-1,".500 m treated as surface");
        if(i==7)__ASSERT(v->recording.surface_seconds==1 && v->recording.active,".499 m missed surface or completed early");
    }
}
static void water_checks(void)
{
    static bool done;
    static unsigned frame;
    static uint64_t capture_after;
    uint64_t now=k_uptime_get();
    if(done) {
        if(frame>=14)return;
        struct watch_data data={.temperature_c=18,.battery_percent=87};
        if(!capture_after) {
            if(frame<4) {
                float surface=frame<2?1.01325f:0.9f;
                __ASSERT(!pc_bench_fresh(surface,1025,NULL,0),"capture fixture rejected");
                watch_ui_open_calculated_scuba();water_select(frame%2,true);
                __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){2,surface+1.96133f}),"capture sample rejected");
            } else {
                static const unsigned counts[]={2,4,5,6,8};
                watch_ui_open_calculated_scuba();
                water_recording_fixture((frame-4)/5,counts[(frame-4)%5]);
            }
            pc_bench_pause(true);scuba_ui_update(&data,pc_bench_read()->seconds*1000,true);
            capture_after=now+350;return;
        }
        if(now<capture_after)return;
        pc_bench_ui_poll();scuba_ui_update(&data,pc_bench_read()->seconds*1000,true);lv_refr_now(NULL);
        __ASSERT(!sim_host_bench_capture(frame++),"Water pressure capture failed");
        capture_after=0;
        if(frame==14) {
            __ASSERT(!pc_bench_recording_finish(),"Capture surface completion failed");
            watch_ui_input(WATCH_CROWN,2);watch_ui_input(WATCH_CROWN,24);
        }
        return;
    }
    if(now<500)return;
    done=true;
    watch_ui_open_calculated_scuba();
    const struct bench_view *v=pc_bench_read();
    const struct scuba_model *m=scuba_ui_model();
    struct watch_data data={.temperature_c=18,.battery_percent=87};
    __ASSERT(v->density_kg_m3==1025 && watch_ui_salt_water(),"Marine must be the shared initial choice");
    for(unsigned surface_case=0;surface_case<2;++surface_case) {
        float surface=surface_case?0.9f:1.01325f;
        for(int salt=0;salt<2;++salt) {
            __ASSERT(!pc_bench_fresh(surface,1025,NULL,0),"surface fixture rejected");
            water_select(salt,true);
            __ASSERT(v->density_kg_m3==(salt?1025:1000) && watch_ui_salt_water()==(bool)salt,
                "Confirmed choice must configure bench and both modes");
            water_select(!salt,false);
            __ASSERT(watch_ui_salt_water()==(bool)salt,"Cancelled draft changed water");
            __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){1,surface}),"surface input rejected");
            __ASSERT(v->depth_m==0 && m->depth_dm==0,"Explicit surface must map to zero");
            __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){2,surface+1.96133f}),"pressure rejected");
            __ASSERT(fabsf(v->depth_m-(salt?19.51219512f:20.0f))<0.0001f &&
                m->depth_dm==(salt?195:200),"Physical watch and bench depths disagree");
            scuba_ui_update(&data,2000,true);
            __ASSERT(pc_bench_set_water(!salt)==-1,"Water changed during scuba recording");
            __ASSERT(!pc_bench_accept_sample(&(ostc4_sample){3,surface}),"return rejected");
            __ASSERT(v->recording.active && pc_bench_set_water(!salt)==-1,
                "Brief surface return unlocked unfinished scuba dive");
            __ASSERT(!pc_bench_recording_finish(),"surface completion failed");
            ostc4_observation before=v->result;
            ostc4_sample trace[4];memcpy(trace,v->exposure,sizeof(trace));
            size_t accepted=v->history_count;
            water_select(!salt,true);
            __ASSERT(v->density_kg_m3==(salt?1000:1025),"Finished dive kept water locked");
            __ASSERT(!memcmp(&before,&v->result,sizeof(before)) && accepted==v->history_count &&
                !memcmp(trace,v->exposure,sizeof(trace)),"Preference changed exposure history");
            water_select(salt,true);
            pc_bench_manual(20,0);pc_bench_poll(1000);
            __ASSERT(fabsf(v->depth_m-20)<0.0001f && m->depth_dm==200 &&
                fabsf(v->result.absolute_pressure_bar-(surface+(salt?2.01036325f:1.96133f)))<0.000001f,
                "Commanded physical 20 m changed meaning");
            pc_bench_manual(20,6);pc_bench_poll(1000);
            __ASSERT(fabsf(v->depth_m-20.1f)<0.0001f,"Manual physical rate changed meaning");
            pc_bench_manual(60,0);
            __ASSERT(salt?v->depth_m<21:v->depth_m==60,"Manual command bypassed density-dependent pressure envelope");
        }
    }
    for(unsigned salt=0;salt<2;++salt) {
        water_recording_fixture(salt,8);
        __ASSERT(!pc_bench_recording_finish(),"Physical surface finish failed");
    }
    printk("WATER RECORDING PASS: physical start/alarm/surface millimetre boundaries, watch/graph agreement, +/-6 m/min rates in both waters\n");
    __ASSERT(pc_bench_fresh(1,1010,NULL,0)==-1,"Unsupported third density accepted");
    __ASSERT(!pc_bench_fresh(1,1025,NULL,0),"apnea fixture rejected");
    water_select(0,true);
    watch_ui_input(WATCH_CROWN,2);watch_ui_input(WATCH_CROWN,24);
    watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);
    watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);
    watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);
    __ASSERT(watch_ui_view()==WATCH_DIVE_VIEW && !watch_ui_salt_water(),"Shared Fresh missing on apnea entry");
    watch_ui_input(WATCH_CROWN,2);watch_ui_input(WATCH_SINGLE_TAP,0);
    watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_SINGLE_TAP,0);
    watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);
    __ASSERT(!watch_ui_salt_water() && pc_bench_set_water(true)==-1,"Initial apnea surface unlocked water");
    watch_ui_input(WATCH_DOUBLE_TAP,0);watch_ui_input(WATCH_DOUBLE_TAP,0);
    watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_CROWN,24);
    __ASSERT(watch_ui_view()==WATCH_MODE_MENU && !pc_bench_set_water(true),"Finished apnea kept preference locked");
    printk("WATER SHARED CHOICE PASS: two surfaces, hydrostatic examples, UI commit/cancel, physical manual commands, scuba and entire apnea session locks, unchanged exposure\n");
}

void pc_bench_checks_poll(void)
{
    int mode=sim_host_bench_check_enabled();
    if(mode==7){water_checks();return;}
    if(mode==6){startup_checks();return;}
    /* Scripted checks explicitly start their fixtures; normal startup stays idle. */
    static bool fixture_started;
    if(mode && !fixture_started) {
        fixture_started=true;watch_ui_open_calculated_scuba();pc_bench_pause(false);
    }
    if(sim_host_bench_check_enabled()==5){checkpoint_file_checks();return;}
    if(sim_host_bench_check_enabled()==4){static bool water_done;if(!water_done){water_done=true;pressure_water_checks();}gf_checks();return;}
    if(sim_host_bench_check_enabled()==3){restart_checks();return;}
    if(sim_host_bench_check_enabled()==2){calculated_checks();return;}
    if(!sim_host_bench_check_enabled())return;
    static unsigned stage, milestone, post_stage;
    static uint64_t post_after;
    static uint32_t frozen;
    static uint64_t capture_after;
    static const uint32_t milestones[]={1920,3600,4920,5040};
    const struct bench_view *v=pc_bench_read();
    uint64_t now=k_uptime_get();
    if(stage<10 && now>=100+stage*120) {
        switch(stage++) {
        case 0:sim_host_push_button(60,742,1);break;
        case 1:sim_host_push_button(60,742,0);break;
        case 2:__ASSERT(v->paused,"Bench pause SDL click failed");frozen=v->seconds;break;
        case 3:sim_host_push_button(180,742,1);break;
        case 4:sim_host_push_button(180,742,0);break;
        case 5:sim_host_push_button(180,742,1);break;
        case 6:sim_host_push_button(180,742,0);break;
        case 7:__ASSERT(v->seconds==frozen && v->speed==60,"Pause/speed changed exposure");sim_host_push_button(60,742,1);break;
        case 8:sim_host_push_button(60,742,0);break;
        case 9:__ASSERT(!v->paused,"Resume SDL click failed");break;
        }
    }
    if(milestone==4) {
        if(!post_after)post_after=now;
        if(post_stage<11 && now>=post_after+post_stage*120) switch(post_stage++) {
        case 0:sim_host_push_button(530,742,1);break;
        case 1:sim_host_push_button(530,742,0);break;
        case 2:__ASSERT(!v->replay && !v->ended && v->result.nitrogen_bar[5]>0.9f,"Hold reset retained exposure");sim_host_push_button(750,742,1);break;
        case 3:sim_host_push_button(750,742,0);break;
        case 4:__ASSERT(v->gap && v->result.tissues==OSTC4_INVALID && v->result.ndl==OSTC4_INVALID && v->result.tts==OSTC4_INVALID,"Gap did not invalidate results");frozen=v->seconds;break;
        case 5:__ASSERT(v->seconds==frozen,"Invalid exposure advanced");lv_refr_now(NULL);__ASSERT(sim_host_bench_capture(4)==0,"Gap capture failed");sim_host_push_button(880,742,1);break;
        case 6:sim_host_push_button(880,742,0);break;
        case 7:__ASSERT(v->replay && !v->gap && v->seconds<=1 && v->speed==1,"Explicit fresh replay failed");break;
        case 8:
            /* A one-second excursion between indices retained by the old stride
             * must remain visible after a long manual surface history. */
            __ASSERT(pc_bench_fresh(1,1000,NULL,0)==0,"Manual fixture failed");
            pc_bench_poll(999000);pc_bench_manual(30,0);pc_bench_poll(1000);
            pc_bench_manual(0,0);pc_bench_poll(1001000);pc_bench_pause(true);break;
        case 9:__ASSERT(v->seconds==2001 && v->history_count==2002,"Manual history missing");lv_refr_now(NULL);__ASSERT(sim_host_bench_capture(5)==0,"Manual excursion capture failed");break;
        case 10:printk("PC BENCH CONTROLS PASS: manual hold retained loading, gap invalidation, explicit fresh replay\n");break;
        }
        return;
    }
    if(stage<10)return;
    if(!capture_after && v->seconds>=milestones[milestone]) {
        pc_bench_pause(true);frozen=v->seconds;capture_after=now+250;
    }
    if(capture_after && now>=capture_after) {
        __ASSERT(v->seconds==frozen && v->result.timestamp_seconds==frozen,"Graph/exposure clock diverged");
        __ASSERT(v->result.tissues==OSTC4_AVAILABLE && !v->error,"Replay invalid");
        if(milestone>0)__ASSERT(v->result.nitrogen_bar[5]>0.9f,"Repetitive loading missing");
        lv_refr_now(NULL);
        __ASSERT(sim_host_bench_capture(milestone)==0,"Bench capture failed");
        printk("PC BENCH milestone %u: time=%u tissues=available\n",milestone,frozen);
        ++milestone;capture_after=0;pc_bench_pause(false);
        if(milestone==4)printk("PC BENCH VISIBLE PASS: SDL pause, x60, two dives, retained tissues, synchronized graph\n");
    }
}
