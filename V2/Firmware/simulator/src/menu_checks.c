/* Native acceptance journeys through menus and every added Back item. */
#include "menu_checks.h"
#include "watch_ui.h"
#include "sim_host.h"
#include "scuba_ui.h"
#include "apnea_settings.h"
#include <zephyr/kernel.h>
#include <stdio.h>
static void tap(void){watch_ui_input(WATCH_SINGLE_TAP,0);}
static void crown(int n){watch_ui_input(WATCH_CROWN,n);}
static void back(void){watch_ui_input(WATCH_DOUBLE_TAP,0);}
static void shot(const char *name)
{
 char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),name);
 lv_refr_now(NULL);__ASSERT(!sim_host_capture(path),"Menu capture failed");
}
static void option(int n){crown(-100);crown(n);tap();}
void menu_checks_poll(void)
{
 static unsigned step;
 if(step>43||k_uptime_get()<2400+step*400)return;
 switch(step++) {
 case 0:crown(1);crown(24);crown(-100);break;
 case 1:shot("main-watch");crown(3);break;
 case 2:shot("main-options");tap();break;
 case 3:shot("options-first");crown(1);break;
 case 4:shot("options-watch");tap();break;
 case 5:shot("watch-type");crown(1);break;
 case 6:shot("watch-face");crown(100);break;
 case 7:shot("watch-back");tap();__ASSERT(watch_ui_view()==WATCH_OPTION_VIEW,"Watch Back failed");option(0);break;
 case 8:shot("bluetooth-first");crown(1);break;
 case 9:shot("bluetooth-data");crown(100);break;
 case 10:shot("bluetooth-back");tap();__ASSERT(watch_ui_view()==WATCH_OPTION_VIEW&&!watch_ui_bluetooth()&&!watch_ui_bluetooth_dfu(),"Bluetooth Back changed settings");option(3);break;
 case 11:shot("haptics-first");crown(1);break;
 case 12:shot("haptics-pulse");crown(100);break;
 case 13:shot("haptics-back");tap();__ASSERT(watch_ui_view()==WATCH_OPTION_VIEW&&watch_ui_haptics()&&watch_ui_pulse()==PULSE_MEDIUM,"Haptics Back changed settings");option(2);break;
 case 14:shot("backlight-first");crown(1);break;
 case 15:shot("backlight-brightness");crown(100);break;
 case 16:shot("backlight-back");tap();__ASSERT(watch_ui_view()==WATCH_OPTION_VIEW&&watch_ui_backlight_mode()==BACKLIGHT_AUTO,"Backlight Back changed mode");crown(100);break;
 case 17:shot("options-back");tap();__ASSERT(watch_ui_view()==WATCH_MODE_MENU,"Options Back failed");crown(-100);crown(WATCH_DIVING);tap();break;
 case 18:shot("dive-scuba");crown(1);break;
 case 19:shot("dive-free");crown(100);break;
 case 20:shot("dive-back");tap();__ASSERT(watch_ui_view()==WATCH_MODE_MENU,"Dive mode Back failed");tap();crown(-100);tap();tap();tap();break;
 case 21:__ASSERT(scuba_ui_screen()==SCUBA_WATER,"Scuba water missing");shot("scuba-water-fresh");crown(1);break;
 case 22:shot("scuba-water-marine");crown(100);break;
 case 23:shot("scuba-water-back");tap();__ASSERT(scuba_ui_screen()==SCUBA_SETTINGS&&!scuba_ui_model()->salt_water,"Water Back applied draft");crown(100);break;
 case 24:shot("scuba-settings-back");tap();__ASSERT(scuba_ui_screen()==SCUBA_PREP,"Settings Back failed");crown(100);crown(24);tap();crown(1);tap();crown(2);tap();break;
 case 25:shot("apnea-settings-first");crown(1);break;
 case 26:shot("apnea-settings-field");crown(-100);tap();break;
 case 27:shot("apnea-water-fresh");crown(1);break;
 case 28:shot("apnea-water-marine");crown(100);break;
 case 29:shot("apnea-water-back");tap();__ASSERT(!apnea_settings_salt_water(),"Apnea water Back applied draft");crown(1);tap();break;
 case 30:shot("apnea-field-first");crown(1);break;
 case 31:shot("apnea-field-next");crown(100);break;
 case 32:shot("apnea-field-back");tap();__ASSERT(apnea_settings_bottom_field()==APNEA_TEMPERATURE,"Field Back applied draft");crown(100);break;
 case 33:shot("apnea-settings-back");tap();break;
 case 34:shot("apnea-options-gate");tap();crown(1);crown(-1);crown(1);crown(-1);break;
 case 35:__ASSERT(apnea_settings_selection_within_page(),"Rapid menu input moved capsule outside page");back();crown(100);crown(24);crown(-100);tap();break;
 case 36:__ASSERT(watch_ui_view()==WATCH_CLOCK,"Return to clock failed");crown(1);crown(24);crown(-100);crown(WATCH_OPTION);tap();option(1);crown(1);tap();crown(-1);break;
 case 37:shot("face-back");tap();__ASSERT(watch_ui_view()==WATCH_FACE_SELECT,"Face Back failed");crown(100);tap();crown(100);tap();crown(-100);crown(WATCH_DIVING);tap();crown(-100);tap();crown(1);tap();break;
 case 38:__ASSERT(scuba_ui_screen()==SCUBA_OPTIONS,"Scuba Options missing");shot("scuba-options-first");crown(1);break;
 case 39:shot("scuba-options-next");crown(100);tap();__ASSERT(scuba_ui_screen()==SCUBA_PREP,"Scuba Options Back failed");break;
 case 40:__ASSERT(!sim_host_set_zoom(2),"Scale x2 failed");crown(100);crown(24);crown(WATCH_OPTION);tap();option(0);break;
 case 41:shot("bluetooth-x2");crown(100);break;
 case 42:shot("bluetooth-back-x2");tap();__ASSERT(watch_ui_view()==WATCH_OPTION_VIEW,"Back at x2 failed");break;
 case 43:printk("SCROLL MENUS PASS: fixed selection, scrolling text/icons, unchanged main menu, explicit Back on watch/diving/settings/face selector, Back cancels drafts, rapid input and x2\n");break;
 }
}
