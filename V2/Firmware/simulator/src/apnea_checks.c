#include "apnea_checks.h"
#include "sim_controls.h"
#include "sim_host.h"
#include "sim_layout.h"
#include "watch_ui.h"
#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
static unsigned step;
static uint64_t clock_ms=100000;
static bool visible_text(lv_obj_t *o,const char *text)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return false;
    if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return true;
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++)if(visible_text(lv_obj_get_child(o,i),text))return true;
    return false;
}
static void expect(const char *text)
{
    __ASSERT(visible_text(lv_screen_active(),text),"Missing visible apnea value %s at step %u",text,step);
}
static void capture(const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),name);
    lv_refr_now(NULL);__ASSERT(sim_host_capture(path)==0,"Apnea capture failed");
}
bool apnea_checks_inputs(uint64_t *sample_ms)
{
    if(!sim_host_apnea_check_enabled())return false;
    *sample_ms=clock_ms;
    if(step>=47||k_uptime_get()<500+(uint64_t)step*450)return false;
    switch(step) {
    case 0:
        watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_CROWN,24);
        watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);
        watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,100);
        watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 1:break;
    case 2:clock_ms=101000;sim_controls_set_depth_dm(10);break;
    case 3:clock_ms=102000;sim_controls_set_depth_dm(11);break;
    case 4:clock_ms=122000;sim_controls_set_depth_dm(123);break;
    case 5:clock_ms=132000;break;
    case 6:clock_ms=142000;sim_controls_set_depth_dm(99);break;
    case 7:clock_ms=161000;sim_controls_set_depth_dm(3);break;
    case 8:clock_ms=162000;sim_controls_set_depth_dm(2);break;
    case 9:clock_ms=281000;sim_controls_set_depth_dm(1);break;
    case 10:clock_ms=282000;sim_controls_set_depth_dm(2);break;
    case 11:clock_ms=292000;sim_controls_set_depth_dm(0);break;
    case 12:clock_ms=823000;sim_controls_set_depth_dm(2);break;
    case 13:watch_ui_input(WATCH_CROWN,1);break;
    case 14:clock_ms=824000;break;
    case 15:watch_ui_input(WATCH_CROWN,-1);break;
    case 16:clock_ms=825000;sim_controls_set_depth_dm(50);break;
    case 17:clock_ms=1425000;sim_controls_set_depth_dm(1000);break;
    case 18:sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,1);break;
    case 19:sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,0);break;
    case 20:clock_ms=1426000;sim_controls_set_depth_dm(1000);break;
    case 21:watch_ui_input(WATCH_CROWN,3);break;
    case 23:clock_ms=1600000;watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 24:clock_ms=1700000;watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 25:clock_ms=1701000;break;
    case 26:clock_ms=1702000;sim_host_set_zoom(1);watch_ui_input(WATCH_CROWN,1);break;
    case 27:clock_ms=1703000;break;
    case 28:clock_ms=1704000;sim_controls_set_depth_dm(123);break;
    case 29:clock_ms=1714000;sim_controls_set_depth_dm(200);break;
    case 30:clock_ms=1724000;sim_controls_set_depth_dm(0);break;
    case 31:clock_ms=1725000;watch_ui_input(WATCH_CROWN,-1);break;
    case 32:expect("LAST DEPTH");expect("20.0");expect("0:01");capture("stats-main-return");clock_ms=1726000;sim_controls_set_depth_dm(50);watch_ui_input(WATCH_CROWN,1);break;
    case 33:clock_ms=1727000;break;
    case 34:clock_ms=1730000;sim_controls_set_depth_dm(0);break;
    case 35:clock_ms=1731000;sim_controls_set_depth_dm(1000);break;
    case 36:clock_ms=1791000;break;
    case 37:sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,1);break;
    case 38:sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,0);break;
    case 39:clock_ms=7791000;break;
    case 40:clock_ms=101731000;break;
    case 41:watch_ui_input(WATCH_CROWN,2);break;
    case 42:clock_ms=101732000;watch_ui_input(WATCH_CROWN,24);sim_controls_set_depth_dm(0);break;
    case 43:clock_ms=101740000;watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 44:clock_ms=101750000;watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 45:clock_ms=101751000;watch_ui_input(WATCH_CROWN,1);break;
    case 46:clock_ms=101752000;break;
    case 22:
        watch_ui_input(WATCH_CROWN,24);clock_ms=1500000;
        sim_controls_set_depth_dm(0);
        /* Sample the actual departure before re-entry in a later fixture. */
        break;
    }
    *sample_ms=clock_ms;return true;
}
void apnea_checks_observe(const struct watch_data *data)
{
    switch(step) {
    case 1:expect("--");expect("TEMPERATURE");capture("apnea-01-fresh");break;
    case 2:__ASSERT(data->total_dives==0,"1.0 m started apnea");break;
    case 3:__ASSERT(data->total_dives==1&&data->apnea_active,"1.1 m failed start");break;
    case 4:expect("12.3");expect("0:20");capture("apnea-04-descent");break;
    case 5:expect("0:30");capture("apnea-05-hold");break;
    case 6:expect("9.9");expect("12.3");capture("apnea-06-ascent");break;
    case 7:__ASSERT(data->apnea_active,"0.3 m ended apnea");break;
    case 8:expect("LAST DEPTH");expect("0:00");capture("apnea-08-surface");break;
    case 9:expect("1:59");__ASSERT(data->recovery_red,"119 s green early");capture("apnea-09-red");break;
    case 10:expect("2:00");__ASSERT(!data->recovery_red,"120 s missing green");capture("apnea-10-green");break;
    case 11:expect("2:10");__ASSERT(!data->recovery_ring_visible,"130 s ring still visible");capture("apnea-11-expired");break;
    case 12:expect("11:01");capture("apnea-12-long-recovery");break;
    case 14:__ASSERT(data->runtime_s==724&&data->total_dives==1,"Page navigation reset session");capture("apnea-14-statistics-no-ring");break;
    case 16:expect("5.0");expect("0:00");__ASSERT(data->max_depth_dm==123&&data->total_dives==2,"Second apnea lost session");break;
    case 17:expect("100.0");expect("10:00");__ASSERT(data->max_dive_s==600,"Live duration record stale");capture("apnea-17-long-x1");break;
    case 20:expect("100.0");expect("10:01");capture("apnea-20-long-x2");break;
    case 25:expect("--");__ASSERT(data->runtime_s==1&&data->total_dives==0&&data->max_depth_dm==0&&!data->show_recovery,"Re-entry retained session");capture("apnea-25-reentry");printk("APNEA MAIN PASS: thresholds, direction, live records, 119/120/130/661 s recovery, page continuity, two apneas, exit/re-entry and x1/x2 renders\n");break;
    case 27:expect("LONGEST APNEA");expect("APNEAS");expect("SESSION TIME");expect("--");expect("0");expect("0:00:03");capture("stats-01-fresh");break;
    case 29:expect("20.0");expect("0:10");expect("1");expect("0:00:14");capture("stats-02-live");break;
    case 30:expect("20.0");expect("0:20");expect("0:00:24");capture("stats-03-recovery");break;
    case 31:__ASSERT(data->show_recovery&&data->recovery_s==1,"Statistics reset recovery");break;
    case 33:expect("20.0");expect("0:20");expect("2");expect("0:00:27");capture("stats-04-second-shorter");break;
    case 36:__ASSERT(sim_host_get_zoom()==1,"Statistics x1 fixture wrong");expect("100.0");expect("1:00");expect("3");expect("0:01:31");capture("stats-05-live-record-x1");break;
    case 39:__ASSERT(sim_host_get_zoom()==2,"Statistics x2 fixture wrong");expect("100.0");expect("101:00");expect("1:41:31");capture("stats-06-live-record-x2");break;
    case 40:expect("1666:40");expect("27:47:11");capture("stats-07-long-session");break;
    case 46:expect("--");expect("0");expect("0:00:02");capture("stats-08-reentry");printk("APNEA STATISTICS PASS: initial placeholders, active records, recovery continuity, shorter second apnea, new live depth/time records, long timers, x1/x2 and re-entry\n");break;
    case 22:__ASSERT(watch_ui_view()==WATCH_MODE_MENU,"Leave control changed");break;
    }
    step++;
}
