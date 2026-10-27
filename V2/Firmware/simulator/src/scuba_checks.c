/* Native acceptance test: accelerated scenarios plus real SDL test buttons. */
#include "scuba_checks.h"
#include "scuba_ui.h"
#include "sim_host.h"
#include "sim_controls.h"
#include "sim_layout.h"
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
enum op { OPEN, ENTER, SCREEN, SHOT, TAP, DOUBLE, CROWN, SCENE_DOWN, SPEED_DOWN, BATTERY_DOWN, BUTTON_UP, SPEED, ACTIVE, ROTATE, SCENARIO, EXITED, DEPTH, CEILING_ALERT, ACKED, LOW_BATTERY, FIELDS, FIELD, DASHBOARD_TEST, NDL_SCENE, NAV, PICK_FIELD, MENU_CHECK, BACKLIGHT_CHECK, OPEN_FIELD, DONE };
struct step {int ms;enum op op;int value;const char *name;};
static const struct step steps[]={
    {100, OPEN, 0},
    {450, SCREEN, SCUBA_PREP},
    {500, SHOT, 0, "scuba-01-ready"},
    {650, TAP, 0},
    {950, SCREEN, SCUBA_SETTINGS},
    {1000, SHOT, 0, "scuba-02-settings"},
    {1010, CROWN, 100},
    {1020, TAP, 0},
    {1040, SCREEN, SCUBA_PREP},
    {1060, TAP, 0},
    {1070, CROWN, -100},
    {1150, TAP, 0},
    {1190, CROWN, 1},
    {1210, TAP, 0},
    {1250, CROWN, 1},
    {1300, TAP, 0},
    {1550, CROWN, 30},
    {1600, SHOT, 0, "scuba-03-depth-alarm"},
    {1700, TAP, 0},
    {1800, CROWN, 1},
    {1850, TAP, 0},
    {2000, CROWN, 45},
    {2050, TAP, 0},
    {2150, DOUBLE, 0},
    {2300, SCENE_DOWN, 0},
    {2390, BUTTON_UP, 0},
    {2600, SPEED_DOWN, 0},
    {2690, BUTTON_UP, 0},
    {2900, SPEED_DOWN, 0},
    {2990, BUTTON_UP, 0},
    {3200, SPEED, 60},
    {3600, ACTIVE, 1},
    {3700, SHOT, 0, "scuba-04-main"},
    {3760, FIELDS, 0},
    {3800, PICK_FIELD, FIELD_MAX_DEPTH},
    {3850, SHOT, 0, "scuba-18-max-depth-field"},
    {3900, CROWN, 1},
    {4200, SCREEN, SCUBA_OPTIONS_GATE},
    {4250, SHOT, 0, "scuba-05-options-gate"},
    {4300, MENU_CHECK, 0},
    {4750, SHOT, 0, "scuba-06-ring-options"},
    {4800, TAP, 0},
    {4850, SCREEN, SCUBA_OPTIONS},
    {5000, BACKLIGHT_CHECK, 0},
    {5300, SCREEN, SCUBA_BRIGHTNESS},
    {5320, SHOT, 0, "scuba-07-brightness"},
    {5370, CROWN, -100},
    {5400, TAP, 0},
    {5450, SCREEN, SCUBA_BACKLIGHT},
    {5600, OPEN_FIELD, 0},
    {5650, SHOT, 0, "scuba-field-before-scroll"},
    {5700, CROWN, 1},
    {5750, SHOT, 0, "scuba-field-moving"},
    {6000, SHOT, 0, "scuba-22-bottom-options"},
    {6100, DOUBLE, 0},
    {6110, CROWN, -100},
    {6300, SHOT, 0, "scuba-options-first"},
    {6310, CROWN, 1},
    {6360, SHOT, 0, "scuba-options-moving"},
    {6610, SHOT, 0, "scuba-options-settled"},
    {6640, DOUBLE, 0},
    {6680, SCREEN, SCUBA_MAIN},
    {6800, NAV, SCUBA_MAIN},
    {7150, SHOT, 0, "scuba-08-surface"},
    {7200, NAV, SCUBA_LEAVE},
    {7500, CROWN, 6},
    {7800, SHOT, 0, "scuba-09-leave-quarter"},
    {7900, CROWN, -6},
    {8000, CROWN, -1},
    {8300, SCENARIO, SCUBA_SAFETY},
    {13450, FIELD, FIELD_SAFETY_STOP},
    {13500, SHOT, 0, "scuba-10-safety-stop"},
    {16100, SHOT, 0, "scuba-11-stop-complete"},
    {16900, NAV, SCUBA_LEAVE},
    {17200, CROWN, 6},
    {17500, SHOT, 0, "scuba-12-surface-exit"},
    {17800, CROWN, 18},
    {17900, EXITED, 0},
    {18200, ENTER, 0},
    {18500, SCENARIO, SCUBA_DECO},
    {21250, FIELD, FIELD_CEILING},
    {21300, SHOT, 0, "scuba-13-deco"},
    {21600, CROWN, 100},
    {21700, SCREEN, SCUBA_OPTIONS_GATE},
    {21800, DOUBLE, 0},
    {21900, SCREEN, SCUBA_BRIGHTNESS},
    {22000, TAP, 0},
    {22050, TAP, 0},
    {22200, NAV, SCUBA_MAIN},
    {22500, DEPTH, 50},
    {22800, CEILING_ALERT, 0},
    {22850, SHOT, 0, "scuba-14-ceiling-alert"},
    {22900, TAP, 0},
    {23200, ACKED, 0},
    {23250, SHOT, 0, "scuba-15-alert-acknowledged"},
    {23300, CROWN, 100},
    {23400, SCREEN, SCUBA_OPTIONS_GATE},
    {23600, DEPTH, 60},
    {24000, BATTERY_DOWN, 0},
    {24100, BUTTON_UP, 0},
    {24400, BATTERY_DOWN, 0},
    {24500, BUTTON_UP, 0},
    {24900, NAV, SCUBA_MAIN},
    {25000, LOW_BATTERY, 0},
    {25250, SHOT, 0, "scuba-16-low-battery"},
    {25300, DEPTH, 0},
    {25700, NAV, SCUBA_LEAVE},
    {25800, SCREEN, SCUBA_LEAVE},
    {26000, DEPTH, 20},
    {26300, SCREEN, SCUBA_MAIN},
    {26500, DEPTH, 0},
    {31800, SCREEN, SCUBA_SUMMARY},
    {31900, SHOT, 0, "scuba-17-summary"},
    {32100, CROWN, 1},
    {32300, CROWN, 1},
    {32400, CROWN, 24},
    {32500, EXITED, 0},
    {32600, DASHBOARD_TEST, 0},
    {32800, DEPTH, 400},
    {33800, DEPTH, 380},
    {34300, SHOT, 0, "scuba-19-fast-ascent"},
    {34800, DEPTH, 360},
    {35800, DEPTH, 340},
    {36800, DEPTH, 320},
    {37800, DEPTH, 300},
    {38300, SHOT, 0, "scuba-20-sustained-ascent"},
    {38500, TAP, 0},
    {39500, PICK_FIELD, FIELD_MAX_DEPTH},
    {39770, SHOT, 0, "scuba-field-max-depth"},
    {39800, PICK_FIELD, FIELD_CLOCK},
    {40070, SHOT, 0, "scuba-field-clock"},
    {40100, PICK_FIELD, FIELD_BATTERY},
    {40370, SHOT, 0, "scuba-field-battery"},
    {40400, PICK_FIELD, FIELD_SAFETY_STOP},
    {40670, SHOT, 0, "scuba-field-safety"},
    {40700, PICK_FIELD, FIELD_TTS},
    {40970, SHOT, 0, "scuba-field-tts"},
    {41000, PICK_FIELD, FIELD_PPO2},
    {41270, SHOT, 0, "scuba-field-ppo2"},
    {41300, PICK_FIELD, FIELD_HEADING},
    {41570, SHOT, 0, "scuba-field-heading"},
    {41600, PICK_FIELD, FIELD_TEMPERATURE},
    {41870, SHOT, 0, "scuba-field-temperature"},
    {42300, NDL_SCENE, 0},
    {43200, SHOT, 0, "scuba-21-low-ndl"},
    {43600, DONE, 0},
};
static void main_view(void)
{
    if(scuba_ui_model()->notifications)watch_ui_input(WATCH_SINGLE_TAP,0);
    if(scuba_ui_screen()==SCUBA_BRIGHTNESS||scuba_ui_screen()==SCUBA_SENSITIVITY)watch_ui_input(WATCH_SINGLE_TAP,0);
    if(scuba_ui_screen()==SCUBA_RING_SELECT||scuba_ui_screen()==SCUBA_FIELD_SELECT||scuba_ui_screen()==SCUBA_BACKLIGHT)watch_ui_input(WATCH_DOUBLE_TAP,0);
    if(scuba_ui_screen()==SCUBA_OPTIONS) {
        watch_ui_input(WATCH_CROWN,100);watch_ui_input(WATCH_SINGLE_TAP,0);
    }
    if(scuba_ui_screen()==SCUBA_OPTIONS_GATE)watch_ui_input(WATCH_CROWN,-100);
}
static void open_options(void)
{
    main_view();watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);
    __ASSERT(scuba_ui_screen()==SCUBA_OPTIONS,"Options menu did not open directly");
}
static void pick_field(enum scuba_field field)
{
    open_options();watch_ui_input(WATCH_CROWN,1);
    watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);
    watch_ui_input(WATCH_CROWN,field);watch_ui_input(WATCH_SINGLE_TAP,0);
    main_view();
    __ASSERT(scuba_ui_field()==field,"Display selection was not applied");
}
static void prep_shot(const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),name);
    __ASSERT(sim_host_capture(path)==0,"Preparation capture failed");
}
static void prep_check(void)
{
    static unsigned step;
    if(step>17||k_uptime_get()<100+step*300)return;
    const struct scuba_model *s=scuba_ui_model();
    switch(step++) {
    case 0:
        watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_CROWN,24);
        watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);
        watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 1:prep_shot("prep-default");watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 2:__ASSERT(scuba_ui_screen()==SCUBA_WATER,"Water submenu missing");prep_shot("prep-water-fresh");watch_ui_input(WATCH_CROWN,1);break;
    case 3:prep_shot("prep-water-marine");watch_ui_input(WATCH_SINGLE_TAP,0);__ASSERT(s->salt_water,"Marine not confirmed");watch_ui_input(WATCH_CROWN,3);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 4:watch_ui_input(WATCH_CROWN,3);__ASSERT(s->start_depth_dm==35,"Start depth not adjusted");break;
    case 5:prep_shot("prep-start-depth");watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-2);watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,30);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 6:watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,45);watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_DOUBLE_TAP,0);break;
    case 7:__ASSERT(scuba_ui_screen()==SCUBA_PREP&&s->depth_alarm_dm==300&&s->time_alarm_s==2700,"Preparation settings lost");prep_shot("prep-configured");watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 8:watch_ui_input(WATCH_CROWN,-100);__ASSERT(s->start_depth_dm==5,"Start lower bound failed");watch_ui_input(WATCH_CROWN,100);__ASSERT(s->start_depth_dm==100,"Start upper bound failed");watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 9:watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);__ASSERT(s->salt_water,"Back changed water type");watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);break;
    case 10:prep_shot("prep-water-fresh-again");watch_ui_input(WATCH_SINGLE_TAP,0);__ASSERT(!s->salt_water,"Fresh not confirmed");watch_ui_input(WATCH_CROWN,100);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case 11:__ASSERT(scuba_ui_screen()==SCUBA_PREP,"Settings Back failed");printk("PREP SETTINGS PASS: water icons/confirmation/Back, alarm recap, adjustable start and bounds\n");step=18;break;
    }
}
void scuba_checks_poll(void)
{
    if(getenv("DIVE_SIM_PREP_CHECK")){prep_check();return;}
    static unsigned index;
    static int button_y;
    if(!sim_host_scuba_check_enabled()||index>=sizeof(steps)/sizeof(steps[0])) return;
    const struct step *step=&steps[index];
    if(k_uptime_get()<step->ms) return;
    index++;
    const struct scuba_model *s=scuba_ui_model();
    switch(step->op) {
    case OPEN:
        watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_CROWN,24);
        /* fall through */
    case ENTER:
        watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case SCREEN:
        __ASSERT(watch_ui_view()==WATCH_SCUBA_VIEW && scuba_ui_screen()==step->value,"Wrong scuba screen at %d ms: %d expected %d",step->ms,scuba_ui_screen(),step->value);break;
    case SHOT: {
        char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),step->name);
        __ASSERT(sim_host_capture(path)==0,"Scuba screenshot failed");break;
    }
    case NDL_SCENE:scuba_ui_set_scenario(SCUBA_ALERTS);scuba_ui_set_speed(60);break;
    case DASHBOARD_TEST:
        watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_SINGLE_TAP,0);
        scuba_ui_set_scenario(SCUBA_MANUAL);scuba_ui_set_speed(1);scuba_ui_set_battery(87);break;
    case FIELDS:
        for(unsigned i=0;i<SCUBA_FIELD_COUNT;i++)pick_field(i);
        pick_field(FIELD_TEMPERATURE);break;
    case PICK_FIELD:pick_field(step->value);break;
    case NAV:
        main_view();watch_ui_input(WATCH_CROWN,step->value==SCUBA_LEAVE?100:step->value==SCUBA_OPTIONS?1:0);break;
    case OPEN_FIELD:
        open_options();watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case BACKLIGHT_CHECK: {
        main_view();watch_ui_input(WATCH_CROWN,1);watch_ui_input(WATCH_SINGLE_TAP,0);
        watch_ui_input(WATCH_CROWN,2);watch_ui_input(WATCH_SINGLE_TAP,0);
        __ASSERT(scuba_ui_screen()==SCUBA_BACKLIGHT,"Backlight submenu did not open");
        enum watch_backlight_mode mode=watch_ui_backlight_mode();
        watch_ui_input(WATCH_SINGLE_TAP,0);
        __ASSERT(watch_ui_backlight_mode()==(mode+1)%3,"Backlight mode was not shared");
        watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_SINGLE_TAP,0);
        watch_ui_input(WATCH_CROWN,2);watch_ui_input(WATCH_SINGLE_TAP,0);
        __ASSERT(scuba_ui_screen()==SCUBA_SENSITIVITY,"Sensitivity editor missing");
        watch_ui_input(WATCH_CROWN,100);watch_ui_input(WATCH_SINGLE_TAP,0);
        __ASSERT(watch_ui_sensitivity()==100,"Sensitivity was not shared");
        watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,WATCH_DIVING);watch_ui_input(WATCH_SINGLE_TAP,0);
        break;
    }
    case MENU_CHECK:
        watch_ui_input(WATCH_CROWN,100);
        __ASSERT(scuba_ui_screen()==SCUBA_OPTIONS_GATE,"Underwater exit should be locked");
        watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_SINGLE_TAP,0);
        __ASSERT(scuba_ui_screen()==SCUBA_RING_SELECT,"Ring selector did not open");
        for(int i=SCUBA_RING_COUNT-1;i>=0;i--) {
            watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_CROWN,i);
            watch_ui_input(WATCH_SINGLE_TAP,0);
            __ASSERT(scuba_ui_ring()==i,"Ring selection was not applied");
            watch_ui_input(WATCH_SINGLE_TAP,0);
        }
        watch_ui_input(WATCH_CROWN,100);watch_ui_input(WATCH_CROWN,100);
        watch_ui_input(WATCH_CROWN,-100);break;
    case FIELD:__ASSERT(scuba_ui_field()==step->value,"Automatic scuba field incorrect");break;
    case TAP:watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case DOUBLE:watch_ui_input(WATCH_DOUBLE_TAP,0);break;
    case CROWN:watch_ui_input(WATCH_CROWN,step->value);break;
    case SCENE_DOWN:button_y=286;sim_host_push_button(SIM_PANEL_X+30,button_y,1);break;
    case SPEED_DOWN:button_y=320;sim_host_push_button(SIM_PANEL_X+30,button_y,1);break;
    case BATTERY_DOWN:button_y=354;sim_host_push_button(SIM_PANEL_X+30,button_y,1);break;
    case BUTTON_UP:sim_host_push_button(SIM_PANEL_X+30,button_y,0);break;
    case SPEED:__ASSERT(s->speed==step->value&&s->scenario==SCUBA_NO_DECO,"Scenario/speed buttons failed");break;
    case ACTIVE:__ASSERT(s->active==step->value,"Scuba did not start automatically");break;
    case ROTATE:sim_host_push_wheel(SIM_ORIENTATION_X+20,SIM_ORIENTATION_Y+12,-18);break;
    case SCENARIO:scuba_ui_set_scenario(step->value);break;
    case EXITED:__ASSERT(watch_ui_view()==WATCH_MODE_MENU&&s->summary.valid,"Surface exit failed");break;
    case DEPTH:scuba_ui_manual();sim_controls_set_depth_dm(step->value);break;
    case CEILING_ALERT:__ASSERT(s->alerts&SCUBA_CEILING,"Missing ceiling violation");break;
    case ACKED:__ASSERT(!s->notifications&&(s->alerts&SCUBA_CEILING),"Acknowledgement cleared the persistent alert");break;
    case LOW_BATTERY:__ASSERT(s->battery==5&&(s->alerts&SCUBA_BATTERY),"Battery test buttons failed");break;
    case DONE:__ASSERT(scuba_ui_haptic_count()>0,"Alerts did not trigger simulated haptics");printk("SCUBA UI PASS: preparation, settings, actual scenario/speed/battery buttons, options entry, shared backlight, animated ring and footer selectors, brightness return, bounded underwater navigation, safety/deco, alert acknowledgement, resubmersion guard, surface exit and summary\n");break;
    }
}
