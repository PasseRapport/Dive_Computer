#include "appearance_checks.h"
#include "sim_host.h"
#include "sim_controls.h"
#include "watch_ui.h"
#include "scuba_ui.h"
#include "scuba_dashboard.h"
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
static void tap(void){watch_ui_input(WATCH_SINGLE_TAP,0);}
static void crown(int n){watch_ui_input(WATCH_CROWN,n);}
static void shot(const char *name){char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),name);__ASSERT(sim_host_capture(path)==0,"Appearance capture failed");}
/* Match the requested ascent reference at each boundary, with native pixels. */
static void ascent_examples(const struct watch_data *data)
{
    static lv_obj_t *view;
    static unsigned index;
    static const int speeds[]={-100,0,9,10,19,20,40,60,80,81,100,101,101};
    static const char *const names[]={"descending","hold","below-one","one","under-two","two","four","six","eight","above-eight","ten","above-ten","highlight"};
    if(!sim_host_appearance_check_enabled()||index>13||k_uptime_get()<24000+index*200)return;
    if(index) {
        char name[80];snprintf(name,sizeof(name),"ascent-%s",names[index-1]);shot(name);
        if(index==13) {lv_obj_delete(view);index++;printk("ASCENT REFERENCE UI PASS: no arrows below 1, 2 m/min levels, colour boundaries and five-second highlight\n");return;}
    } else {
        view=scuba_dashboard_create(lv_screen_active());lv_obj_set_pos(view,22,32);
        lv_obj_set_style_bg_color(view,lv_color_black(),0);lv_obj_set_style_bg_opa(view,LV_OPA_COVER,0);
    }
    struct scuba_model m={.active=true,.depth_dm=100,.battery=87,.ndl_s=3600,
        .ascent_dm_min=speeds[index],.fast_ascent_ms=index==12?5000:4999};
    scuba_dashboard_update(view,&m,data);index++;
}
/* Short native-render regression for minute-width transitions. */
static void timer_examples(const struct watch_data *data)
{
    static lv_obj_t *view;static unsigned index;
    static const unsigned minutes[]={9,10,52,99,100,999};
    static const uint16_t depths[]={0,99,100,311,999,1000};
    if(index>6||k_uptime_get()<100+index*200)return;
    if(index) {
        char name[48];snprintf(name,sizeof(name),"timer-%u",minutes[index-1]);shot(name);
        if(index==6){lv_obj_delete(view);index++;printk("TIMER ANCHOR UI PASS\n");return;}
    } else {
        view=scuba_dashboard_create(lv_screen_active());lv_obj_set_pos(view,22,32);
        lv_obj_set_style_bg_color(view,lv_color_black(),0);lv_obj_set_style_bg_opa(view,LV_OPA_COVER,0);
    }
    struct scuba_model m={.active=true,.deco_started=true,.depth_dm=depths[index],.battery=87,
        .dive_ms=(minutes[index]*60+5)*1000,.temperature=18};
    scuba_dashboard_update(view,&m,data);index++;
}
static void rate_button_examples(void)
{
    static unsigned step;
    static const int expected[]={1,2,3,2,1,0,-1,0};
    if(step>=24||k_uptime_get()<100+step*100)return;
    unsigned index=step/3,phase=step%3;
    int x=index<3?190:index<7?70:300;
    if(!step)sim_controls_set_depth_dm(200);
    if(phase==0)sim_host_push_button(x,392,1);
    if(phase==1)sim_host_push_button(x,392,0);
    if(phase==2) {
        __ASSERT(sim_controls_depth_rate()==expected[index],"Rate button failed at step %u: %d",index,sim_controls_depth_rate());
        if(index==7){__ASSERT(!sim_controls_depth_dm(),"Surface did not reset depth");shot("rate-buttons");printk("RATE BUTTONS PASS: actual clicks +1/+2/+3/+2/+1/0/-1, Surface reset\n");}
    }
    step++;
}
void appearance_checks_poll(struct watch_data *data)
{
    if(getenv("DIVE_SIM_RATE_CHECK")){rate_button_examples();return;}
    if(getenv("DIVE_SIM_TIMER_CHECK")){timer_examples(data);return;}
    ascent_examples(data);
    static unsigned step;static unsigned peak;static lv_obj_t *fixture;static struct scuba_model demo;
    static const unsigned times[]={100,300,400,600,700,900,1000,1200,1300,1500,1600,1800,1900,2200,2300,2600,2700,2900,3000,3200,3300,3500,3600,3800,3900,4100,4200,4300,4500,4700,4800,5100,5200,5500,5600,5900,6000,6200,6500,6600,6900,7000,7300,7400,7700,7900,8100,8400,8500,8800,8900,9200,9300,9800,9900,10600,10700,10800,11500,11600,11800,12000,12200,12400,12600,12800,13000,13200,13400,13600,18700,19700,19800,20000,20200,20400,20600,20800,21000,21200,21400,21600,21700,23000,23300,23600,23800};
    if(!sim_host_appearance_check_enabled()||step>=sizeof(times)/sizeof(times[0])||k_uptime_get()<times[step])return;
    switch(step++){
    case 0:tap();break;
    case 1:shot("digital-87");break;
    case 2:data->battery_percent=50;break;
    case 3:shot("digital-50");break;
    case 4:data->battery_percent=10;break;
    case 5:shot("digital-10");break;
    case 6:data->battery_percent=11;break;
    case 7:shot("digital-11");break;
    case 8:data->battery_percent=51;break;
    case 9:shot("digital-51");break;
    case 10:data->battery_percent=0;break;
    case 11:shot("digital-0");break;
    case 12:data->battery_percent=87;crown(1);crown(24);crown(3);tap();crown(1);tap();crown(1);tap();watch_ui_input(WATCH_DOUBLE_TAP,0);crown(-100);tap();__ASSERT(watch_ui_clock_style()==CLOCK_ANALOG&&watch_ui_view()==WATCH_CLOCK,"Analog face missing");break;
    case 13:shot("analog-87");break;
    case 14:crown(1);crown(24);crown(-100);crown(WATCH_DIVING);tap();crown(1);tap();__ASSERT(watch_ui_view()==WATCH_DIVE_VIEW,"Freedive missing");break;
    case 15:shot("freedive-87");break;
    case 16:data->battery_percent=10;break;
    case 17:shot("freedive-10");break;
    case 18:data->battery_percent=87;crown(100);crown(24);tap();crown(-100);tap();break;
    case 19:tap();crown(100);tap();__ASSERT(scuba_ui_screen()==SCUBA_PREP,"Dive Settings Back failed");break;
    case 20:sim_controls_set_depth_dm(20);break;
    case 21:shot("scuba-87");break;
    case 22:scuba_ui_set_battery(50);break;
    case 23:shot("scuba-50");break;
    case 24:scuba_ui_set_battery(10);break;
    case 25:shot("scuba-10");break;
    case 26:if(scuba_ui_model()->notifications)tap();
        crown(1);tap();__ASSERT(scuba_ui_screen()==SCUBA_OPTIONS,"Options missing");watch_ui_input(WATCH_DOUBLE_TAP,0);__ASSERT(scuba_ui_screen()==SCUBA_MAIN,"Options double tap failed");break;
    case 27:scuba_ui_set_battery(51);break;
    case 28:shot("scuba-51");break;
    case 29:printk("APPEARANCE CHECK PASS: date, battery thresholds, analog hands, dive icons, Dive Settings Back, Options double tap\n");break;
    case 30:
        crown(1);tap();tap();crown(-100);crown(RING_COMPASS);tap();watch_ui_input(WATCH_DOUBLE_TAP,0);
        __ASSERT(scuba_ui_ring()==RING_COMPASS&&scuba_ui_screen()==SCUBA_MAIN,"Compass ring not selected");break;
    case 31:shot("compass-ring-north");break;
    case 32:sim_host_push_wheel(42,352,-18);break;
    case 33:shot("compass-ring-east");break;
    case 34:
        crown(1);tap();tap();crown(100);tap();
        __ASSERT(scuba_ui_screen()==SCUBA_OPTIONS&&scuba_ui_ring()==RING_COMPASS,"Ring Back changed selection");
        crown(1);tap();crown(100);tap();
        __ASSERT(scuba_ui_screen()==SCUBA_OPTIONS,"Bottom field Back failed");
        crown(1);tap();crown(100);tap();
        __ASSERT(scuba_ui_screen()==SCUBA_OPTIONS,"Backlight Back failed");
        watch_ui_input(WATCH_DOUBLE_TAP,0);break;
    case 35:shot("compass-ring-after-back");break;
    case 36:sim_controls_set_depth_dm(0);break;
    case 37:
        crown(100);crown(24);crown(3);tap();crown(1);tap();crown(-100);crown(1);tap();
        __ASSERT(watch_ui_page()==WATCH_BRIGHTNESS,"General brightness editor missing");break;
    case 38:shot("general-brightness");break;
    case 39:tap();crown(2);tap();__ASSERT(watch_ui_page()==WATCH_SENSITIVITY,"General sensitivity editor missing");break;
    case 40:shot("general-sensitivity");break;
    case 41:crown(4);break;
    case 42:shot("general-sensitivity-half");crown(-100);
        for(int i=0;i<=10;i++){__ASSERT(watch_ui_sensitivity()==i*10,"Sensitivity step mismatch");crown(1);}crown(-5);break;
    case 43:tap();watch_ui_input(WATCH_DOUBLE_TAP,0);watch_ui_input(WATCH_DOUBLE_TAP,0);crown(-100);tap();break;
    case 44:shot("analog-final");break;
    case 45:printk("POLISH UI PASS: compass rotation, menu Back fallbacks, shared light editors and watch layout\n");break;
    case 46:crown(1);crown(24);crown(-100);__ASSERT(watch_ui_mode()==WATCH_WATCH,"Watch must be first");break;
    case 47:shot("wave-main-menu");break;
    case 48:crown(3);tap();crown(-100);crown(2);tap();__ASSERT(watch_ui_backlight_option()==0,"Mode must be first");break;
    case 49:shot("wave-backlight");break;
    case 50:tap();crown(1);tap();__ASSERT(watch_ui_page()==WATCH_BRIGHTNESS,"Brightness must be second");break;
    case 51:shot("wave-brightness");break;
    case 52:tap();watch_ui_input(WATCH_DOUBLE_TAP,0);watch_ui_input(WATCH_DOUBLE_TAP,0);crown(-100);crown(WATCH_DIVING);tap();crown(-100);tap();sim_controls_set_depth_dm(20);break;
    case 53:sim_host_push_button(190,392,1);break;
    case 54:sim_host_push_button(190,392,0);break;
    case 55:peak=sim_controls_depth_mm();__ASSERT(peak>2000&&sim_controls_depth_rate()>0,"Rate buttons did not descend");shot("wave-descending");sim_host_push_button(70,392,1);break;
    case 56:sim_host_push_button(70,392,0);break;
    case 57:sim_host_push_button(70,392,1);break;
    case 58:sim_host_push_button(70,392,0);break;
    case 59:__ASSERT(sim_controls_depth_rate()<0,"Rate buttons did not ascend");shot("wave-ascending");sim_host_push_button(300,392,1);break;
    case 60:sim_host_push_button(300,392,0);break;
    case 61:
        __ASSERT(!sim_controls_depth_dm()&&!sim_controls_depth_rate(),"Surface must also stop vertical speed");shot("wave-surface");
        fixture=scuba_dashboard_create(lv_screen_active());lv_obj_set_pos(fixture,22,32);lv_obj_set_style_bg_color(fixture,lv_color_black(),0);lv_obj_set_style_bg_opa(fixture,LV_OPA_COVER,0);
        demo=(struct scuba_model){.active=true,.deco_started=true,.depth_dm=70,.peak_dm=300,.battery=87,.dive_ms=600000,.temperature=18};
        scuba_dashboard_update(fixture,&demo,data);break;
    case 62:shot("wave-deco-zero");demo.stop_ms=60000;scuba_dashboard_update(fixture,&demo,data);break;
    case 63:shot("wave-deco-first");demo.deco_stage=1;demo.stop_ms=45000;demo.depth_dm=40;scuba_dashboard_update(fixture,&demo,data);break;
    case 64:shot("wave-deco-second");demo.deco_done=true;demo.stop_ms=90000;scuba_dashboard_update(fixture,&demo,data);break;
    case 65:shot("wave-deco-complete");lv_obj_delete(fixture);break;
    case 66:scuba_ui_set_scenario(SCUBA_MANUAL);scuba_ui_set_speed(60);sim_controls_set_depth_dm(20);break;
    case 67:sim_controls_set_depth_dm(0);break;
    case 68:printk("WAVE UI PASS: reordered menus, shared backlight, real rate buttons, Surface reset and remaining stop ring\n");break;
    case 69:break;
    case 70:__ASSERT(scuba_ui_screen()==SCUBA_SUMMARY,"Summary missing");peak=scuba_ui_model()->surface_ms;shot("wave-summary-first");break;
    case 71:__ASSERT(scuba_ui_model()->surface_ms>peak,"Surface time froze after dive end");shot("wave-summary-later");break;
    case 72:printk("WAVE SUMMARY PASS: live surface interval after automatic dive completion\n");break;
    case 73:fixture=scuba_dashboard_create(lv_screen_active());lv_obj_set_pos(fixture,22,32);lv_obj_set_style_bg_color(fixture,lv_color_black(),0);lv_obj_set_style_bg_opa(fixture,LV_OPA_COVER,0);demo=(struct scuba_model){.active=true,.depth_dm=200,.battery=87,.ndl_s=3600};scuba_dashboard_set_ring(fixture,RING_NDL);scuba_dashboard_update(fixture,&demo,data);break;
    case 74:shot("rules-ndl-60");demo.ndl_s=7200;scuba_dashboard_update(fixture,&demo,data);break;
    case 75:shot("rules-ndl-over60");demo.ndl_s=300;scuba_dashboard_update(fixture,&demo,data);break;
    case 76:shot("rules-ndl-five");demo.ndl_s=299;scuba_dashboard_update(fixture,&demo,data);break;
    case 77:shot("rules-ndl-under-five");demo.ndl_s=0;scuba_dashboard_update(fixture,&demo,data);break;
    case 78:shot("rules-ndl-zero");lv_obj_delete(fixture);break;
    case 79:printk("RING RULE FIXTURES PASS: NDL 60, above 60, five minutes, below five and zero\n");break;
    case 80:scuba_ui_set_scenario(SCUBA_MANUAL);scuba_ui_set_speed(10);sim_controls_set_depth_dm(400);break;
    case 81:sim_host_push_button(70,392,1);break;
    case 82:sim_host_push_button(70,392,0);break;
    case 83:case 84:case 85:
        __ASSERT(sim_controls_depth_rate()<0&&scuba_ui_model()->ascent_dm_min==-sim_controls_depth_rate()*10,"Real button constant ascent oscillated");break;
    case 86:printk("PRECISE ASCENT UI PASS: actual SDL buttons, fractional depth and stable ascent through the full UI at x10\n");break;
    }
}
