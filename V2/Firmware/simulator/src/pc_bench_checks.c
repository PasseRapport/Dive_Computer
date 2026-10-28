/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "pc_bench_ui.h"
#include "sim_host.h"
#include "sim_controls.h"
#include "scuba_ui.h"
#include <lvgl.h>
#include <zephyr/kernel.h>

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
            sim_host_push_button(860,744,1);break;
        case 23:sim_host_push_button(860,744,0);break;
        case 24:
            __ASSERT(v->depth_m>6.99f && v->depth_m<7.01f && v->rate_m_min==0,"apnea Depth +1 control failed");
            pc_bench_pause(false);pc_bench_poll(1000);pc_bench_pause(true);
            __ASSERT(sim_controls_depth_dm()==70 && sim_controls_depth_rate()==0,"apnea depth input disconnected");break;
        case 25:
            __ASSERT(watch_ui_view()==WATCH_DIVE_VIEW,"bench controls navigated away from apnea");
            __ASSERT(sim_host_bench_capture(11)==0,"apnea depth capture failed");
            printk("APNEA BENCH PASS: real Rate/Depth clicks drive the shared depth, paused exposure and watch view preserved\n");break;
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

/* Native rendering check, driven by real SDL button events. Numerical checks
 * remain in tests/bench with no LVGL or Zephyr dependency. */
void pc_bench_checks_poll(void)
{
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
        case 2:__ASSERT(!v->replay && !v->ended && v->result.nitrogen_bar[5]>0.9f,"Hold reset retained exposure");sim_host_push_button(980,742,1);break;
        case 3:sim_host_push_button(980,742,0);break;
        case 4:__ASSERT(v->gap && v->result.tissues==OSTC4_INVALID && v->result.ndl==OSTC4_INVALID && v->result.tts==OSTC4_INVALID,"Gap did not invalidate results");frozen=v->seconds;break;
        case 5:__ASSERT(v->seconds==frozen,"Invalid exposure advanced");lv_refr_now(NULL);__ASSERT(sim_host_bench_capture(4)==0,"Gap capture failed");sim_host_push_button(1100,742,1);break;
        case 6:sim_host_push_button(1100,742,0);break;
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
