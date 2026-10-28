#include "apnea_persistence_checks.h"
#include "apnea_settings.h"
#include "watch_ui.h"
#include "sim_controls.h"
#include "sim_host.h"
#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
static unsigned step;
static uint64_t clock_ms=100000;
static void click(void){watch_ui_input(WATCH_SINGLE_TAP,0);}
static void turn(int n){watch_ui_input(WATCH_CROWN,n);}
static void back(void){watch_ui_input(WATCH_DOUBLE_TAP,0);}
static void enter(void){turn(1);turn(24);turn(-100);turn(WATCH_DIVING);click();turn(100);click();}
static void editor(bool bottom){turn(2);click();turn(bottom?100:-100);click();}
static void main_page(void){back();back();turn(-100);}
static bool visible(lv_obj_t *o,const char *text)
{
 if(!lv_obj_is_visible(o))return false;
 if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return true;
 for(unsigned i=0;i<lv_obj_get_child_count(o);i++)if(visible(lv_obj_get_child(o,i),text))return true;
 return false;
}
static void expect(const char *text){__ASSERT(visible(lv_screen_active(),text),"Persistence step%u missing %s",step,text);}
static void capture(const char *name)
{
 char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),name);
 lv_refr_now(NULL);__ASSERT(!sim_host_capture(path),"Persistence capture failed");
}
static const char *caption(int field){return field==1?"SESSION DEPTH":field==2?"SESSION APNEA":"TEMPERATURE";}
bool apnea_persistence_checks_inputs(uint64_t *sample_ms)
{
 int mode=sim_host_apnea_persistence_mode();if(!mode)return false;
 int choice=sim_host_apnea_persistence_choice(),salt=choice/3,field=choice%3;
 *sample_ms=clock_ms;
 if(step>=20||k_uptime_get()<500+(uint64_t)step*300)return false;
 if(mode==1)switch(step){
 case 0:enter();break;
 case 1:editor(false);turn(-100);turn(!salt);click();main_page();editor(false);turn(-100);turn(salt);click();main_page();break;
 case 2:editor(true);turn(-100);turn((field+1)%3);click();main_page();editor(true);turn(-100);turn(field);click();main_page();break;
 case 3:clock_ms=101000;sim_controls_set_depth_dm(123);break;
 case 4:clock_ms=161000;sim_controls_set_depth_dm(200);break;
 case 5:clock_ms=162000;sim_controls_set_depth_dm(0);break;
 case 6:editor(false);turn(-100);turn(!salt);back();main_page();break;
 case 7:editor(true);turn(-100);turn((field+1)%3);back();main_page();break;
 case 8:editor(false);turn(-100);turn(!salt);break;
 case 9:clock_ms=163000;sim_controls_set_depth_dm(150);break;
 case 10:click();main_page();break;
 case 11:clock_ms=233000;sim_controls_set_depth_dm(0);break;
 case 12:editor(true);turn(-100);turn((field+1)%3);break;
 case 13:clock_ms=234000;sim_controls_set_depth_dm(160);break;
 case 14:click();main_page();break;
 case 15:clock_ms=235000;sim_controls_set_depth_dm(0);break;
 }
 else switch(step){
 case 0:enter();break;
 case 2:turn(1);break;
 case 4:turn(-1);clock_ms=101000;sim_controls_set_depth_dm(123);break;
 case 5:clock_ms=161000;break;
 case 6:turn(1);break;
 case 8:turn(-1);break;
 case 10:clock_ms=162000;sim_controls_set_depth_dm(0);break;
 case 11:editor(false);break;
 case 13:main_page();break;
 case 14:turn(3);turn(24);break;
 case 15:clock_ms=300000;break;
 case 16:click();break;
 case 17:clock_ms=400000;click();break;
 }
 *sample_ms=clock_ms;return true;
}
void apnea_persistence_checks_observe(const struct watch_data *data)
{
 int mode=sim_host_apnea_persistence_mode(),choice=sim_host_apnea_persistence_choice();
 int salt=choice/3,field=choice%3;
 if(mode==1){
  if(step==16){expect(caption(field));__ASSERT(watch_ui_salt_water()==salt&&apnea_settings_bottom_field()==field,"Cancelled or underwater edit changed applied preferences");capture("saved-main");}
  if(step==19)printk("APNEA PERSISTENCE SAVE PASS: choices%d/%d, cancellation and editors crossing immersion\n",salt,field);
 }else switch(step){
 case 1:expect(caption(field));expect(field?"--":"18°C");__ASSERT(watch_ui_salt_water()==salt,"Water preference not restored");__ASSERT(data->total_dives==0&&!data->show_recovery&&data->runtime_s==0,"Restart restored session history");capture("restored-main");break;
 case 3:expect("SESSION TIME");expect("--");expect("0");capture("restored-statistics");break;
 case 5:expect(caption(field));expect(field==1?"12.3 m":field==2?"1:00":"18°C");capture("live-main");break;
 case 7:expect("12.3");expect("1:00");expect("1");expect("0:01:01");capture("live-statistics");break;
 case 9:expect(caption(field));expect(field==1?"12.3 m":field==2?"1:00":"18°C");capture("returned-main");break;
 case 12:expect("WATER TYPE");__ASSERT(watch_ui_salt_water()==salt,"Loaded water selection wrong");capture("restored-water");break;
 case 18:expect(caption(field));expect(field?"--":"18°C");__ASSERT(watch_ui_salt_water()==salt&&data->total_dives==0&&!data->show_recovery&&data->runtime_s==0,"Session reset lost preferences or retained history");capture("reentry-main");break;
 case 19:printk("APNEA PERSISTENCE LOAD PASS: choices%d/%d, fresh session, Main/Statistics/Main live record agreement and reset\n",salt,field);break;
 }
 step++;
}
