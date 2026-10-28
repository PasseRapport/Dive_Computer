#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <lvgl.h>
#include "watch_ui.h"
#include "sim_controls.h"
#include "sim_host.h"
#include "sim_layout.h"
#include "sim_checks.h"
#include "sim_dive.h"
#include "apnea_checks.h"
#include "scuba_checks.h"
#include "appearance_checks.h"
#ifdef DIVE_PC_BENCH
#include "pc_bench_ui.h"
#include "pc_bench.h"
#endif

int main(void)
{
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        printk("Display unavailable\n");
        return 1;
    }

    struct watch_data data = {
        .depth_dm = 0, .temperature_c = 18, .battery_percent = 87,
    };
    watch_ui_create(sim_controls_create());
    sim_host_init();
#ifdef DIVE_PC_BENCH
    if (sim_host_bench_fresh()!=0) return 1;
    pc_bench_ui_create();
#endif
    unsigned hour,minute,day,month,weekday,second;
    sim_host_get_clock(&hour, &minute);
    data.clock_hour = hour;
    data.clock_minute=minute;
    sim_host_get_date(&day,&month,&weekday,&second);
    data.clock_day=day;data.clock_month=month;data.clock_weekday=weekday;data.clock_second=second;
    watch_ui_update(&data);
    lv_timer_handler();
    int ret = display_blanking_off(display);
    if (ret < 0 && ret != -ENOSYS) {
        printk("Display blanking error: %d\n", ret);
        return 1;
    }
    printk("LVGL UI ready: 320x300, RGB565, simulated data\n");
    uint64_t last_update = UINT64_MAX;
    struct sim_dive dive = {0};
    int check_stage=0,scale_stage=0,stress_stage=0;
    bool check_done = false;
    bool was_animating = false;
    while (1) {
        appearance_checks_poll(&data);
        sim_checks_poll();
        scuba_checks_poll();
        /* Exercise real SDL events, including wheel outside the crown zone. */
        if (sim_host_check_enabled() && check_stage < 7 &&
            k_uptime_get() >= 100 + check_stage * 120) {
            switch (check_stage++) {
            case 0: sim_host_push_wheel(100, 100, 5); break;
            case 1: sim_host_push_wheel(SIM_PANEL_X + 30, SIM_CROWN_Y + 30, 1); break;
            case 2: sim_host_push_button(SIM_PANEL_X + 30, SIM_TAP_Y + 20, 1); break;
            case 3: sim_host_push_button(SIM_PANEL_X + 30, SIM_TAP_Y + 20, 0); break;
            case 4: sim_host_push_button(SIM_PANEL_X + 30, SIM_DOUBLE_Y + 20, 1); break;
            case 5: sim_host_push_button(SIM_PANEL_X + 30, SIM_DOUBLE_Y + 20, 0); break;
            case 6: sim_host_push_wheel(SIM_PANEL_X + 30, SIM_CROWN_Y + 30, -1); break;
            }
        }
        sim_controls_poll();
#ifdef DIVE_PC_BENCH
        pc_bench_ui_poll();
        pc_bench_checks_poll();
#endif
        if (sim_host_check_enabled() && !check_done && k_uptime_get() >= 1100) {
            /* +1 then -1 should cancel; finish at +1 to check both directions. */
            sim_host_push_wheel(SIM_PANEL_X + 30, SIM_CROWN_Y + 30, 1);
            sim_controls_poll();
            const char *path = sim_host_capture_path();
            if (path) {
                int capture_ret = sim_host_capture(path);
                printk("Screenshot result: %d\n", capture_ret);
            }
            __ASSERT(sim_controls_check_passed(), "Simulator control check failed");
            printk("CONTROL CHECK PASS: crown zone, both directions, single and double tap\n");
            check_done = true;
        }
        if(sim_host_scale_check_enabled()&&scale_stage<9&&k_uptime_get()>=2200+scale_stage*250){
            switch(scale_stage++){
            case 0:__ASSERT(sim_host_get_zoom()==1&&sim_host_zoom_matches(1),"Default scale must be x1");sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,1);break;
            case 1:sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,0);break;
            case 2:__ASSERT(sim_host_get_zoom()==2&&sim_host_zoom_matches(2),"Main Scale button failed");break;
            case 3:watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_CROWN,24);watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_SINGLE_TAP,0);break;
            case 4:__ASSERT(watch_ui_view()==WATCH_SCUBA_VIEW,"Scale test scuba missing");break;
            case 5:sim_host_push_button(SIM_PANEL_X+170,394,1);break;
            case 6:sim_host_push_button(SIM_PANEL_X+170,394,0);break;
            case 7:__ASSERT(sim_host_get_zoom()==1&&sim_host_zoom_matches(1),"Scuba Scale button failed");break;
            case 8:printk("SCALE BUTTON PASS: real SDL clicks and window dimensions in both panels\n");break;
            }
        }
        if(sim_host_scale_check_enabled()&&scale_stage==9&&stress_stage<180&&k_uptime_get()>=4500+stress_stage*40){
            unsigned n=stress_stage++,cycle=n/3;
            switch(n%3){
            case 0:sim_host_push_button(SIM_PANEL_X+170,394,1);break;
            case 1:sim_host_push_button(SIM_PANEL_X+170,394,0);break;
            case 2:
                __ASSERT(sim_host_get_zoom()==(cycle%2?1:2)&&sim_host_zoom_matches(cycle%2?1:2),"Scale clicks stopped responding");
                __ASSERT(sim_host_frame_visible(),"Scale produced a black window");break;
            }
            if(stress_stage==180)printk("SCALE STRESS PASS: 60 real clicks, visible frames and responsive controls\n");
        }
        uint64_t now_ms = k_uptime_get(),sample_ms=now_ms;
        bool apnea_step=apnea_checks_inputs(&sample_ms);
        if (apnea_step || last_update == UINT64_MAX || now_ms - last_update >= 50 ||
            dive.session_active != (watch_ui_view()==WATCH_DIVE_VIEW) ||
            data.depth_dm != sim_controls_depth_dm() ||
            data.heading_degrees != sim_controls_heading_degrees()) {
            data.heading_degrees = sim_controls_heading_degrees();
            /* Session time uses the watch clock, independent of exposure/bench time.
             * Only the freediving tracker resets here; scuba keeps receiving samples. */
            bool freediving=watch_ui_view()==WATCH_DIVE_VIEW;
            sim_dive_session(&dive,&data,sample_ms,freediving);
            data.depth_dm=sim_controls_depth_dm();
            data.depth_mm=sim_controls_depth_mm();data.depth_mm_valid=true;
            if(freediving)sim_dive_update(&dive,&data,sample_ms,data.depth_dm);
            sim_host_get_clock(&hour, &minute);
            data.clock_hour = hour;
            data.clock_minute=minute;
            sim_host_get_date(&day,&month,&weekday,&second);
            data.clock_day=day;data.clock_month=month;data.clock_weekday=weekday;data.clock_second=second;
            watch_ui_update(&data);
            if(apnea_step)apnea_checks_observe(&data);
            last_update = now_ms;
        }
        /* Desktop-only scaled canvas: redraw during transitions and one
         * final frame to avoid dirty-area rounding leaving cursor trails. */
        bool animating = lv_anim_count_running() != 0;
        if (animating || was_animating) {
            lv_obj_invalidate(lv_screen_active());
        }
        was_animating = animating;
        lv_timer_handler();
        /* Scale changes can happen inside an input callback. Repaint after
         * that callback, and again for delayed native resize/expose events. */
        if(sim_host_take_redraw()) {
            lv_obj_invalidate(lv_screen_active());
            lv_refr_now(NULL);
        }
        k_sleep(K_MSEC(10));
    }
    return 0;
}
