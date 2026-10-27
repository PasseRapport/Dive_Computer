/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pc_bench.h"
#include "pc_bench_ui.h"
#include "sim_host.h"
#include <lvgl.h>
#include <zephyr/kernel.h>

/* Native rendering check, driven by real SDL button events. Numerical checks
 * remain in tests/bench with no LVGL or Zephyr dependency. */
void pc_bench_checks_poll(void)
{
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
