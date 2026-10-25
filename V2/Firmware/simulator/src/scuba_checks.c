/* Native acceptance test: accelerated scenarios plus real SDL test buttons. */
#include "scuba_checks.h"
#include "scuba_ui.h"
#include "sim_host.h"
#include "sim_controls.h"
#include "sim_layout.h"
#include <zephyr/kernel.h>
#include <stdio.h>
enum op { OPEN, ENTER, SCREEN, SHOT, TAP, DOUBLE, CROWN, SCENE_DOWN, SPEED_DOWN, BATTERY_DOWN, BUTTON_UP, SPEED, ACTIVE, ROTATE, SCENARIO, EXITED, DEPTH, CEILING_ALERT, ACKED, LOW_BATTERY, DONE };
struct step {int ms;enum op op;int value;const char *name;};
static const struct step steps[]={
    {100, OPEN, 0},
    {450, SCREEN, SCUBA_PREP},
    {500, SHOT, 0, "scuba-01-ready"},
    {650, TAP, 0},
    {950, SCREEN, SCUBA_SETTINGS},
    {1000, SHOT, 0, "scuba-02-settings"},
    {1150, TAP, 0},
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
    {3900, CROWN, 1},
    {4200, SCREEN, SCUBA_COMPASS},
    {4250, SHOT, 0, "scuba-05-compass"},
    {4300, ROTATE, 0},
    {4400, CROWN, 1},
    {4750, SHOT, 0, "scuba-06-stats"},
    {4800, CROWN, 100},
    {4850, SCREEN, SCUBA_STATS},
    {5000, DOUBLE, 0},
    {5300, SCREEN, SCUBA_BRIGHTNESS},
    {5320, SHOT, 0, "scuba-07-brightness"},
    {5370, CROWN, -100},
    {5400, TAP, 0},
    {5450, SCREEN, SCUBA_STATS},
    {6300, CROWN, -100},
    {6650, SHOT, 0, "scuba-08-surface"},
    {6700, CROWN, 100},
    {7000, CROWN, 6},
    {7300, SHOT, 0, "scuba-09-leave-quarter"},
    {7400, CROWN, -6},
    {7500, CROWN, -1},
    {7800, SCENARIO, SCUBA_SAFETY},
    {13000, SHOT, 0, "scuba-10-safety-stop"},
    {15600, SHOT, 0, "scuba-11-stop-complete"},
    {16400, CROWN, 100},
    {16700, CROWN, 6},
    {17000, SHOT, 0, "scuba-12-surface-exit"},
    {17300, CROWN, 18},
    {17400, EXITED, 0},
    {17700, ENTER, 0},
    {18000, SCENARIO, SCUBA_DECO},
    {20800, SHOT, 0, "scuba-13-deco"},
    {21100, CROWN, 100},
    {21200, SCREEN, SCUBA_STATS},
    {21300, DOUBLE, 0},
    {21400, SCREEN, SCUBA_BRIGHTNESS},
    {21500, TAP, 0},
    {21700, CROWN, -100},
    {22000, DEPTH, 50},
    {22300, CEILING_ALERT, 0},
    {22350, SHOT, 0, "scuba-14-ceiling-alert"},
    {22400, TAP, 0},
    {22700, ACKED, 0},
    {22750, SHOT, 0, "scuba-15-alert-acknowledged"},
    {22800, CROWN, 100},
    {22900, SCREEN, SCUBA_STATS},
    {23100, DEPTH, 60},
    {23500, BATTERY_DOWN, 0},
    {23600, BUTTON_UP, 0},
    {23900, BATTERY_DOWN, 0},
    {24000, BUTTON_UP, 0},
    {24400, CROWN, -100},
    {24500, LOW_BATTERY, 0},
    {24750, SHOT, 0, "scuba-16-low-battery"},
    {24800, DEPTH, 0},
    {25200, CROWN, 100},
    {25300, SCREEN, SCUBA_LEAVE},
    {25500, DEPTH, 20},
    {25800, SCREEN, SCUBA_MAIN},
    {26000, DEPTH, 0},
    {31300, SCREEN, SCUBA_SUMMARY},
    {31400, SHOT, 0, "scuba-17-summary"},
    {31600, CROWN, 1},
    {31900, CROWN, 24},
    {32000, EXITED, 0},
    {32100, DONE, 0},
};
void scuba_checks_poll(void)
{
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
        watch_ui_input(WATCH_SINGLE_TAP,0);watch_ui_input(WATCH_CROWN,-100);watch_ui_input(WATCH_SINGLE_TAP,0);break;
    case SCREEN:
        __ASSERT(watch_ui_view()==WATCH_SCUBA_VIEW && scuba_ui_screen()==step->value,"Wrong scuba screen at %d ms: %d expected %d",step->ms,scuba_ui_screen(),step->value);break;
    case SHOT: {
        char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),step->name);
        __ASSERT(sim_host_capture(path)==0,"Scuba screenshot failed");break;
    }
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
    case DONE:__ASSERT(scuba_ui_haptic_count()>0,"Alerts did not trigger simulated haptics");printk("SCUBA UI PASS: preparation, settings, actual scenario/speed/battery buttons, main/compass/stats, brightness return, bounded underwater navigation, safety/deco, alert acknowledgement, resubmersion guard, surface exit and summary\n");break;
    }
}
