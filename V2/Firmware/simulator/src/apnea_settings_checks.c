#include "watch_ui.h"
#include "sim_controls.h"
#include "scuba_ui.h"
#include "sim_host.h"
#include "sim_layout.h"
#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
static unsigned step;
static uint64_t clock_ms=100000;
static bool scuba_salt;
static enum scuba_field scuba_bottom;
static enum scuba_ring scuba_ring;
static struct scuba_summary scuba_saved_summary;
static bool visible(lv_obj_t *o,const char *s){if(!lv_obj_is_visible(o))return false;if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),s))return true;for(unsigned i=0;i<lv_obj_get_child_count(o);i++)if(visible(lv_obj_get_child(o,i),s))return true;return false;}
static void field_bounds(lv_obj_t *o)
{
 if(!lv_obj_is_visible(o))return;
 if(lv_obj_check_type(o,&lv_label_class)){
  const char *s=lv_label_get_text(o);
  if(!strcmp(s,"TEMPERATURE")||!strcmp(s,"SESSION DEPTH")||!strcmp(s,"SESSION APNEA")){
   lv_area_t a;lv_obj_get_coords(o,&a);__ASSERT(a.x1>=0&&a.x2<320,"Bottom field label %s clipped horizontally %d..%d at step%u",s,a.x1,a.x2,step);
  }
 }
 for(unsigned i=0;i<lv_obj_get_child_count(o);i++)field_bounds(lv_obj_get_child(o,i));
}
static void expect(const char *s){__ASSERT(visible(lv_screen_active(),s),"Settings missing %s at %u",s,step);}
static void click(void){watch_ui_input(WATCH_SINGLE_TAP,0);}
static void turn(int n){watch_ui_input(WATCH_CROWN,n);}
static void back(void){watch_ui_input(WATCH_DOUBLE_TAP,0);}
static void open_bottom(void){turn(2);click();turn(100);click();}
static void main_page(void){back();back();turn(-100);}
bool apnea_settings_checks_inputs(uint64_t *sample_ms)
{
 if(!sim_host_apnea_settings_check_enabled())return false;
 *sample_ms=clock_ms;
 if(step>=50||k_uptime_get()<500+(uint64_t)step*400)return false;
 switch(step){
 case 0:scuba_salt=scuba_ui_model()->salt_water;scuba_bottom=scuba_ui_field();scuba_ring=scuba_ui_ring();scuba_saved_summary=scuba_ui_model()->summary;turn(1);turn(24);turn(-100);turn(WATCH_DIVING);click();turn(100);click();break;
 case 2:open_bottom();turn(100);click();main_page();break;
 case 4:open_bottom();turn(-100);turn(1);click();main_page();break;
 case 6:clock_ms=101000;sim_controls_set_depth_dm(123);break;
 case 8:clock_ms=161000;break;
 case 10:clock_ms=162000;sim_controls_set_depth_dm(0);break;
 case 12:open_bottom();turn(100);click();main_page();break;
 case 14:open_bottom();turn(-100);back();main_page();break;
 case 16:turn(2);click();turn(-100);click();turn(100);click();main_page();break;
 case 18:turn(2);click();turn(-100);click();turn(-100);break;
 case 19:clock_ms=163000;sim_controls_set_depth_dm(150);break;
 case 21:click();main_page();break;
 case 23:turn(2);click();turn(100);click();turn(-100);click();main_page();break;
 case 24:clock_ms=233000;break;
 case 25:clock_ms=234000;sim_controls_set_depth_dm(0);break;
 case 27:open_bottom();turn(-100);break;
 case 28:clock_ms=235000;sim_controls_set_depth_dm(160);break;
 case 30:click();main_page();break;
 case 32:turn(3);turn(24);break;
 case 33:clock_ms=300000;sim_controls_set_depth_dm(0);break;
 case 34:click();click();break;
 case 36:sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,1);break;
 case 37:sim_host_push_button(SIM_PANEL_X+30,SIM_ZOOM_Y+20,0);break;
 case 39:open_bottom();break;
 case 41:turn(-100);click();main_page();break;
 case 42:clock_ms=301000;sim_controls_set_depth_dm(20);break;
 case 44:clock_ms=311000;sim_controls_set_depth_dm(0);break;
 case 46:turn(2);click();turn(-100);click();turn(-100);click();main_page();break;
 case 48:turn(2);click();turn(-100);click();turn(100);back();main_page();break;
 }
 *sample_ms=clock_ms;return true;
}
void apnea_settings_checks_observe(const struct watch_data *data)
{
 switch(step){
 case 1:expect("TEMPERATURE");break;
 case 3:expect("SESSION APNEA");expect("--");break;
 case 5:expect("SESSION DEPTH");expect("--");break;
 case 7:expect("SESSION DEPTH");expect("12.3 m");break;
 case 9:expect("12.3");__ASSERT(data->max_dive_s==60,"Live record missing");break;
 case 11:expect("SESSION DEPTH");expect("12.3 m");break;
 case 13:expect("SESSION APNEA");expect("1:01");break;
 case 15:expect("SESSION APNEA");break;
 case 17:__ASSERT(watch_ui_salt_water(),"Marine confirmation failed");break;
 case 22:__ASSERT(watch_ui_salt_water(),"Editor applied after descent");expect("SESSION APNEA");break;
 case 24:expect("SESSION APNEA");expect("1:10");__ASSERT(watch_ui_salt_water(),"Underwater settings modified water");break;
 case 26:expect("1:11");break;
 case 31:expect("SESSION APNEA");__ASSERT(data->total_dives==3,"Settings reset count");break;
 case 35:expect("SESSION APNEA");expect("--");__ASSERT(watch_ui_salt_water()&&data->total_dives==0,"Reentry lost preferences or retained session");break;
 case 38:{expect("SESSION APNEA");char path[512];snprintf(path,sizeof(path),"%s/settings-reentry-x2.bmp",sim_host_gallery_path());lv_refr_now(NULL);__ASSERT(sim_host_capture(path)==0,"Capture");break;}
 }
 lv_obj_update_layout(lv_screen_active());if(step%2)field_bounds(lv_screen_active());
 if(step==40){expect("BOTTOM FIELD");char path[512];snprintf(path,sizeof(path),"%s/settings-bottom-x2.bmp",sim_host_gallery_path());lv_refr_now(NULL);__ASSERT(sim_host_capture(path)==0,"Capture");}
 if(step==43||step==45){expect("TEMPERATURE");expect("18°C");}
 if(step==47||step==49)__ASSERT(!watch_ui_salt_water(),"Fresh apply/cancel failed");
 if(step==49){__ASSERT(scuba_ui_model()->salt_water==scuba_salt&&scuba_ui_field()==scuba_bottom&&scuba_ui_ring()==scuba_ring&&!memcmp(&scuba_saved_summary,&scuba_ui_model()->summary,sizeof(scuba_saved_summary)),"Apnea changed scuba settings or stored summary");}
 if(step==49)printk("APNEA SETTINGS PASS: public apply/cancel, all fields, live records, water, phase lock, submenu continuity and reentry x1/x2\n");
 if(step==11||step==17||step==20||step==29){char path[512];snprintf(path,sizeof(path),"%s/settings-%02u.bmp",sim_host_gallery_path(),step);lv_refr_now(NULL);__ASSERT(sim_host_capture(path)==0,"Capture");}
 step++;
}
