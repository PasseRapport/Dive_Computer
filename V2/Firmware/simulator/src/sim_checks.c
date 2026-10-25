/* Optional desktop acceptance scenario: actual SDL events, public UI results. */
#include "sim_checks.h"
#include "sim_host.h"
#include "sim_layout.h"
#include "watch_ui.h"
#include "v1_ui_layout.h"
#include "sim_controls.h"
#include "compass_ui.h"
#include <zephyr/kernel.h>
#include <stdio.h>

enum operation { WHEEL, OUTSIDE, SINGLE_DOWN, SINGLE_UP, DOUBLE_DOWN, DOUBLE_UP,
                 ZOOM_DOWN, ZOOM_UP, ZOOM, VIEW, MODE, DIVE_MODE, OPTION, PAGE, WATER, BRIGHTNESS, SLEEPING, PROGRESS, BLUETOOTH, DESCEND_DOWN, DESCEND_UP, ASCEND_DOWN, ASCEND_UP, SURFACE_DOWN, SURFACE_UP, DEPTH, LEAVE_PROGRESS, LIGHT_OPTION, LIGHT_MODE, SENSITIVITY, BT_OPTION, BT_DFU, FACE, CLOCK_STYLE, HEADING, ROTATE, HAPTIC_ON, HAPTIC_ROW, PULSE, CAPTURE, DONE };
struct step { int ms; enum operation op; int value; const char *name; };
static const struct step ui_steps[] = {
    {40, VIEW, WATCH_BOOT},
    {105, CAPTURE, 0, "00-logo-start"},
    {250, CAPTURE, 0, "00-logo-fade-in"},
    {750, CAPTURE, 0, "00-project-logo"},
    {1500, CAPTURE, 0, "00-logo-shrinking"},
    {1900, VIEW, WATCH_BOOT},
    {1965, CAPTURE, 0, "00-logo-docked"},
    {2100, VIEW, WATCH_CLOCK},
    {2165, CAPTURE, 0, "00-clock"},
    {2230, WHEEL, -1},
    {2540, VIEW, WATCH_TOOL_LEAVE},
    {2605, PAGE, WATCH_LEAVE},
    {2670, CAPTURE, 0, "60-watch-leave"},
    {2735, DOUBLE_DOWN, 0},
    {2825, DOUBLE_UP, 0},
    {3035, VIEW, WATCH_TOOL_LEAVE},
    {3100, WHEEL, -6},
    {3410, LEAVE_PROGRESS, 6},
    {3475, CAPTURE, 0, "61-watch-leave-quarter"},
    {3540, WHEEL, 3},
    {3850, LEAVE_PROGRESS, 3},
    {3915, WHEEL, 100},
    {4225, LEAVE_PROGRESS, 0},
    {4290, WHEEL, 1},
    {4600, VIEW, WATCH_CLOCK},
    {4665, WHEEL, -1},
    {4975, WHEEL, -24},
    {5285, VIEW, WATCH_MODE_MENU},
    {5350, WHEEL, 100},
    {5660, MODE, WATCH_DIVING},
    {5725, WHEEL, -100},
    {6035, MODE, WATCH_OPTION},
    {6100, WHEEL, -5},
    {6410, MODE, WATCH_OPTION},
    {6475, CAPTURE, 0, "03-menu-options"},
    {6540, WHEEL, 100},
    {6850, MODE, WATCH_DIVING},
    {6915, CAPTURE, 0, "01-menu-diving"},
    {6980, WHEEL, -1},
    {7290, MODE, WATCH_COMPASS},
    {7355, SINGLE_DOWN, 0},
    {7445, SINGLE_UP, 0},
    {7655, VIEW, WATCH_COMPASS_VIEW},
    {7720, CAPTURE, 0, "62-compass"},
    {7785, ROTATE, -18},
    {8095, HEADING, 90},
    {8160, VIEW, WATCH_COMPASS_VIEW},
    {8225, CAPTURE, 0, "65-compass-east"},
    {8290, ROTATE, -18},
    {8600, HEADING, 180},
    {8665, CAPTURE, 0, "66-compass-south"},
    {8730, ROTATE, -18},
    {9040, HEADING, 270},
    {9105, CAPTURE, 0, "67-compass-west"},
    {9170, ROTATE, -18},
    {9480, HEADING, 0},
    {9545, CAPTURE, 0, "68-compass-north"},
    {9610, ROTATE, 1},
    {9920, HEADING, 355},
    {9985, CAPTURE, 0, "69-compass-wrap"},
    {10050, ROTATE, -1},
    {10360, HEADING, 0},
    {10425, DOUBLE_DOWN, 0},
    {10515, DOUBLE_UP, 0},
    {10725, VIEW, WATCH_COMPASS_VIEW},
    {10790, WHEEL, -1},
    {11100, VIEW, WATCH_TOOL_LEAVE},
    {11165, CAPTURE, 0, "63-compass-leave"},
    {11230, WHEEL, -23},
    {11540, LEAVE_PROGRESS, 23},
    {11605, VIEW, WATCH_TOOL_LEAVE},
    {11670, CAPTURE, 0, "64-compass-leave-almost-full"},
    {11735, WHEEL, -1},
    {12045, VIEW, WATCH_MODE_MENU},
    {12110, MODE, WATCH_COMPASS},
    {12175, WHEEL, 1},
    {12485, SINGLE_DOWN, 0},
    {12575, SINGLE_UP, 0},
    {12785, VIEW, WATCH_DIVE_SELECT},
    {12850, WHEEL, 20},
    {13160, DIVE_MODE, DIVE_SCUBA},
    {13225, CAPTURE, 0, "16-mode-scubadiving"},
    {13290, SINGLE_DOWN, 0},
    {13380, SINGLE_UP, 0},
    {13590, PAGE, WATCH_SCUBA},
    {13655, CAPTURE, 0, "21-scuba-example"},
    {13720, WHEEL, -1},
    {14030, PAGE, WATCH_LEAVE},
    {14095, WHEEL, -6},
    {14405, CAPTURE, 0, "41-scuba-leave-quarter"},
    {14470, WHEEL, -18},
    {14780, VIEW, WATCH_MODE_MENU},
    {14845, SINGLE_DOWN, 0},
    {14935, SINGLE_UP, 0},
    {15145, WHEEL, -1},
    {15455, WHEEL, -20},
    {15765, DIVE_MODE, DIVE_FREE},
    {15830, CAPTURE, 0, "17-mode-freediving"},
    {15895, SINGLE_DOWN, 0},
    {15985, SINGLE_UP, 0},
    {16195, PAGE, WATCH_MAIN},
    {16260, CAPTURE, 0, "18-diving-main"},
    {16325, WHEEL, -1},
    {16635, PAGE, WATCH_STATS},
    {16700, WHEEL, -1},
    {16800, CAPTURE, 0, "08-transition-forward"},
    {16865, CAPTURE, 0, "09-transition-forward"},
    {17085, PAGE, WATCH_WATER},
    {17150, CAPTURE, 0, "19-water-fresh"},
    {17215, SINGLE_DOWN, 0},
    {17305, SINGLE_UP, 0},
    {17815, CAPTURE, 0, "20-water-marine"},
    {17880, WHEEL, -1},
    {18190, WHEEL, -23},
    {18500, VIEW, WATCH_DIVE_VIEW},
    {18565, CAPTURE, 0, "43-free-leave-almost-full"},
    {18630, WHEEL, -1},
    {18940, VIEW, WATCH_MODE_MENU},
    {19005, WHEEL, -3},
    {19315, MODE, WATCH_OPTION},
    {19380, SINGLE_DOWN, 0},
    {19470, SINGLE_UP, 0},
    {19680, VIEW, WATCH_OPTION_VIEW},
    {19745, WHEEL, 100},
    {20055, OPTION, 0},
    {20120, WHEEL, -100},
    {20430, OPTION, 4},
    {20495, WHEEL, -1},
    {20805, OPTION, 4},
    {20870, CAPTURE, 0, "58-options-watch"},
    {20935, WHEEL, 2},
    {21245, OPTION, 2},
    {21310, CAPTURE, 0, "04-option-brightness"},
    {21375, SINGLE_DOWN, 0},
    {21465, SINGLE_UP, 0},
    {21675, VIEW, WATCH_BACKLIGHT_VIEW},
    {21740, WHEEL, 100},
    {22050, LIGHT_OPTION, 0},
    {22115, WHEEL, -100},
    {22425, LIGHT_OPTION, 2},
    {22490, WHEEL, -1},
    {22800, LIGHT_OPTION, 2},
    {22865, WHEEL, 100},
    {23175, LIGHT_OPTION, 0},
    {23240, CAPTURE, 0, "44-backlight-menu"},
    {23305, SINGLE_DOWN, 0},
    {23395, SINGLE_UP, 0},
    {23605, WHEEL, 100},
    {23915, BRIGHTNESS, 0},
    {23980, CAPTURE, 0, "49-brightness-zero"},
    {24045, WHEEL, -100},
    {24355, BRIGHTNESS, 10},
    {24420, CAPTURE, 0, "50-brightness-ten"},
    {24485, WHEEL, 6},
    {24795, BRIGHTNESS, 4},
    {24860, CAPTURE, 0, "06-brightness-changed"},
    {24925, SINGLE_DOWN, 0},
    {25015, SINGLE_UP, 0},
    {25225, VIEW, WATCH_BACKLIGHT_VIEW},
    {25290, WHEEL, -1},
    {25600, CAPTURE, 0, "45-backlight-auto"},
    {25665, SINGLE_DOWN, 0},
    {25755, SINGLE_UP, 0},
    {25965, LIGHT_MODE, BACKLIGHT_ON},
    {26030, SINGLE_DOWN, 0},
    {26120, SINGLE_UP, 0},
    {26330, LIGHT_MODE, BACKLIGHT_OFF},
    {26395, SINGLE_DOWN, 0},
    {26485, SINGLE_UP, 0},
    {26695, LIGHT_MODE, BACKLIGHT_AUTO},
    {26760, WHEEL, -1},
    {27070, SINGLE_DOWN, 0},
    {27160, SINGLE_UP, 0},
    {27370, WHEEL, -100},
    {27680, SENSITIVITY, 100},
    {27745, WHEEL, 200},
    {28055, SENSITIVITY, 0},
    {28120, WHEEL, -20},
    {28430, SENSITIVITY, 20},
    {28495, CAPTURE, 0, "46-backlight-sensitivity"},
    {28560, SINGLE_DOWN, 0},
    {28650, SINGLE_UP, 0},
    {28860, DOUBLE_DOWN, 0},
    {28950, DOUBLE_UP, 0},
    {29160, WHEEL, -2},
    {29470, OPTION, 4},
    {29535, CAPTURE, 0, "07-option-sleep"},
    {29600, SINGLE_DOWN, 0},
    {29690, SINGLE_UP, 0},
    {29900, CAPTURE, 0, "14-sleep-confirm"},
    {29965, WHEEL, -6},
    {30275, CAPTURE, 0, "22-sleep-quarter"},
    {30340, DOUBLE_DOWN, 0},
    {30430, DOUBLE_UP, 0},
    {30640, SINGLE_DOWN, 0},
    {30730, SINGLE_UP, 0},
    {30940, WHEEL, -23},
    {31250, CAPTURE, 0, "23-sleep-almost-full"},
    {31315, WHEEL, -1},
    {31625, VIEW, WATCH_SLEEP_TRANSITION},
    {31690, SLEEPING, 0},
    {31755, CAPTURE, 0, "70-sleep-start"},
    {32115, CAPTURE, 0, "71-sleep-logo"},
    {32815, CAPTURE, 0, "72-sleep-fade"},
    {33165, SLEEPING, 0},
    {33565, SLEEPING, 1},
    {33630, CAPTURE, 0, "15-display-off"},
    {33695, SINGLE_DOWN, 0},
    {33785, SINGLE_UP, 0},
    {33995, SLEEPING, 0},
    {34060, WHEEL, 100},
    {34370, OPTION, 0},
    {34435, CAPTURE, 0, "24-option-bluetooth"},
    {34500, SINGLE_DOWN, 0},
    {34590, SINGLE_UP, 0},
    {34800, WHEEL, 100},
    {35110, BT_OPTION, 0},
    {35175, CAPTURE, 0, "25-bluetooth-off"},
    {35240, WHEEL, -100},
    {35550, BT_OPTION, 1},
    {35615, WHEEL, -1},
    {35925, BT_OPTION, 1},
    {35990, SINGLE_DOWN, 0},
    {36080, SINGLE_UP, 0},
    {36290, CAPTURE, 0, "48-bluetooth-dfu"},
    {36355, WHEEL, 100},
    {36665, BT_OPTION, 0},
    {36730, SINGLE_DOWN, 0},
    {36820, SINGLE_UP, 0},
    {37030, BLUETOOTH, 1},
    {37095, CAPTURE, 0, "26-bluetooth-on"},
    {37160, DOUBLE_DOWN, 0},
    {37250, DOUBLE_UP, 0},
    {37460, WHEEL, -3},
    {37770, OPTION, 3},
    {37835, CAPTURE, 0, "73-options-haptics"},
    {37900, SINGLE_DOWN, 0},
    {37990, SINGLE_UP, 0},
    {38200, PAGE, WATCH_HAPTICS},
    {38265, HAPTIC_ON, 1},
    {38330, PULSE, PULSE_MEDIUM},
    {38395, CAPTURE, 0, "74-haptics-on"},
    {38460, WHEEL, 100},
    {38770, HAPTIC_ROW, 0},
    {38835, SINGLE_DOWN, 0},
    {38925, SINGLE_UP, 0},
    {39135, HAPTIC_ON, 0},
    {39200, PULSE, PULSE_MEDIUM},
    {39265, CAPTURE, 0, "75-haptics-off"},
    {39330, WHEEL, -100},
    {39640, HAPTIC_ROW, 1},
    {39705, WHEEL, -1},
    {40015, HAPTIC_ROW, 1},
    {40080, SINGLE_DOWN, 0},
    {40170, SINGLE_UP, 0},
    {40380, PULSE, PULSE_LONG},
    {40445, CAPTURE, 0, "76-haptics-long"},
    {40510, SINGLE_DOWN, 0},
    {40600, SINGLE_UP, 0},
    {40810, PULSE, PULSE_SHORT},
    {40875, CAPTURE, 0, "77-haptics-short"},
    {40940, SINGLE_DOWN, 0},
    {41030, SINGLE_UP, 0},
    {41240, PULSE, PULSE_MEDIUM},
    {41305, HAPTIC_ON, 0},
    {41370, DOUBLE_DOWN, 0},
    {41460, DOUBLE_UP, 0},
    {41670, SINGLE_DOWN, 0},
    {41760, SINGLE_UP, 0},
    {41970, HAPTIC_ON, 0},
    {42035, PULSE, PULSE_MEDIUM},
    {42100, HAPTIC_ROW, 0},
    {42165, SINGLE_DOWN, 0},
    {42255, SINGLE_UP, 0},
    {42465, HAPTIC_ON, 1},
    {42530, DOUBLE_DOWN, 0},
    {42620, DOUBLE_UP, 0},
    {42830, WHEEL, 3},
    {43140, OPTION, 0},
    {43205, WHEEL, -1},
    {43515, OPTION, 1},
    {43580, SINGLE_DOWN, 0},
    {43670, SINGLE_UP, 0},
    {43880, VIEW, WATCH_FACE_SELECT},
    {43945, WHEEL, 100},
    {44255, FACE, 0},
    {44320, CAPTURE, 0, "53-watch-menu"},
    {44385, WHEEL, -100},
    {44695, FACE, 1},
    {44760, WHEEL, -1},
    {45070, FACE, 1},
    {45135, CAPTURE, 0, "54-watch-analog-selection"},
    {45200, SINGLE_DOWN, 0},
    {45290, SINGLE_UP, 0},
    {45500, VIEW, WATCH_OPTION_VIEW},
    {45565, CLOCK_STYLE, CLOCK_ANALOG},
    {45630, WHEEL, 0},
    {45940, OPTION, 1},
    {46005, DOUBLE_DOWN, 0},
    {46095, DOUBLE_UP, 0},
    {46305, WHEEL, 1},
    {46615, MODE, WATCH_WATCH},
    {46680, CAPTURE, 0, "59-menu-watch"},
    {46745, SINGLE_DOWN, 0},
    {46835, SINGLE_UP, 0},
    {47045, VIEW, WATCH_CLOCK},
    {47110, CAPTURE, 0, "55-watch-analog"},
    {47175, DOUBLE_DOWN, 0},
    {47265, DOUBLE_UP, 0},
    {47475, VIEW, WATCH_CLOCK},
    {47540, WHEEL, -1},
    {47850, VIEW, WATCH_TOOL_LEAVE},
    {47915, WHEEL, 1},
    {48225, VIEW, WATCH_CLOCK},
    {48290, CAPTURE, 0, "57-watch-analog-retained"},
    {48355, WHEEL, -1},
    {48665, WHEEL, -24},
    {48975, VIEW, WATCH_MODE_MENU},
    {49040, MODE, WATCH_WATCH},
    {49105, WHEEL, -1},
    {49415, SINGLE_DOWN, 0},
    {49505, SINGLE_UP, 0},
    {49715, OPTION, 1},
    {49780, SINGLE_DOWN, 0},
    {49870, SINGLE_UP, 0},
    {50080, WHEEL, 1},
    {50390, FACE, 0},
    {50455, SINGLE_DOWN, 0},
    {50545, SINGLE_UP, 0},
    {50755, CLOCK_STYLE, CLOCK_DIGITAL},
    {50820, DOUBLE_DOWN, 0},
    {50910, DOUBLE_UP, 0},
    {51120, WHEEL, 1},
    {51430, SINGLE_DOWN, 0},
    {51520, SINGLE_UP, 0},
    {51730, VIEW, WATCH_CLOCK},
    {51795, CAPTURE, 0, "56-watch-digital"},
    {51860, ZOOM_DOWN, 0},
    {51950, ZOOM_UP, 0},
    {52160, ZOOM, 1},
    {52225, ROTATE, -18},
    {52535, HEADING, 90},
    {52600, VIEW, WATCH_CLOCK},
    {52665, CAPTURE, 0, "12-zoom-x1"},
    {52730, ZOOM_DOWN, 0},
    {52820, ZOOM_UP, 0},
    {53030, ZOOM, 2},
    {53095, ROTATE, 18},
    {53405, HEADING, 0},
    {53470, VIEW, WATCH_CLOCK},
    {53535, CAPTURE, 0, "13-zoom-x2"},
    {53600, DONE, 0},
};

static const struct step dive_steps[] = {
    {100, WHEEL, 1}, {180, WHEEL, -24},
    {300, SINGLE_DOWN}, {400, SINGLE_UP},
    {650, WHEEL, -1},
    {950, SINGLE_DOWN}, {1050, SINGLE_UP},
    {1250, PAGE, WATCH_MAIN},
    {1300, DESCEND_DOWN}, {1400, DESCEND_UP}, {1550, DEPTH, 10},
    {1600, DESCEND_DOWN}, {1700, DESCEND_UP}, {1850, DEPTH, 20},
    {2000, DESCEND_DOWN}, {2100, DESCEND_UP}, {2250, DEPTH, 30},
    {2300, ASCEND_DOWN}, {2400, ASCEND_UP}, {2550, DEPTH, 20},
    {2800, CAPTURE, 0, "30-underwater"},
    {3900, SURFACE_DOWN}, {4000, SURFACE_UP}, {4150, DEPTH, 0},
    {5000, CAPTURE, 0, "31-recovery-red"},
    {6500, CAPTURE, 0, "32-recovery-red-later"},
    {9100, CAPTURE, 0, "33-recovery-green"},
    {18000, CAPTURE, 0, "34-recovery-green-hold"},
    {19300, CAPTURE, 0, "35-recovery-ring-hidden"},
    {19400, DONE},
};

void sim_checks_poll(void)
{
    if(sim_host_scuba_check_enabled()) return;
    const char *directory = sim_host_gallery_path();
    if (sim_host_water_check_enabled()) {
        static int stage;
        if (!stage) {
            watch_ui_input(WATCH_CROWN, 1);
            watch_ui_input(WATCH_CROWN, 24);
            watch_ui_input(WATCH_SINGLE_TAP, 0);
            watch_ui_input(WATCH_CROWN, 1);
            watch_ui_input(WATCH_SINGLE_TAP, 0);
            watch_ui_input(WATCH_CROWN, 2);
            stage = 1;
        }
        if (k_uptime_get() > 400 && stage == 1) {
            watch_ui_input(WATCH_SINGLE_TAP, 0); stage++;
        }
        if (k_uptime_get() > 800 && stage == 2) {
            watch_ui_input(WATCH_SINGLE_TAP, 0); stage++;
        }
        __ASSERT(ui_water_selection_within_page(), "Water capsule overflows during resize");
        if (k_uptime_get() > 1200 && stage == 3) {
            printk("WATER ANIMATION CHECK PASS: capsule stays within page throughout both transitions\n"); stage++;
        }
        return;
    }
    if (directory && watch_ui_view() == WATCH_DIVE_VIEW && watch_ui_page() == WATCH_WATER) {
        __ASSERT(ui_water_selection_within_page(), "Water capsule overflows old rectangular page during animation");
    }
    const bool dive_check = sim_host_dive_check_enabled();
    const struct step *steps = dive_check ? dive_steps : ui_steps;
    const unsigned step_count = dive_check ? ARRAY_SIZE(dive_steps) : ARRAY_SIZE(ui_steps);
    static unsigned index;
    if (!directory || index >= step_count || k_uptime_get() < steps[index].ms) {
        return;
    }
    const struct step *step = &steps[index++];
    const int x = SIM_PANEL_X + 30;
    switch (step->op) {
    case ROTATE: sim_host_push_wheel(SIM_ORIENTATION_X+20, SIM_ORIENTATION_Y+12, step->value); break;
    case HEADING:
        __ASSERT(sim_controls_heading_degrees() == step->value, "Orientation wheel failed");
        __ASSERT(compass_ui_heading() == step->value, "Compass heading did not update"); break;
    case WHEEL: sim_host_push_wheel(x, SIM_CROWN_Y + 30, step->value); break;
    case OUTSIDE: sim_host_push_wheel(100, 100, step->value); break;
    case SINGLE_DOWN: sim_host_push_button(x, SIM_TAP_Y + 20, 1); break;
    case SINGLE_UP: sim_host_push_button(x, SIM_TAP_Y + 20, 0); break;
    case DOUBLE_DOWN: sim_host_push_button(x, SIM_DOUBLE_Y + 20, 1); break;
    case DOUBLE_UP: sim_host_push_button(x, SIM_DOUBLE_Y + 20, 0); break;
    case ZOOM_DOWN: sim_host_push_button(x, SIM_ZOOM_Y + 20, 1); break;
    case ZOOM_UP: sim_host_push_button(x, SIM_ZOOM_Y + 20, 0); break;
    case DESCEND_DOWN: sim_host_push_button(SIM_DEPTH_DOWN_X + 30, SIM_DEPTH_Y + 12, 1); break;
    case DESCEND_UP: sim_host_push_button(SIM_DEPTH_DOWN_X + 30, SIM_DEPTH_Y + 12, 0); break;
    case ASCEND_DOWN: sim_host_push_button(SIM_DEPTH_UP_X + 30, SIM_DEPTH_Y + 12, 1); break;
    case ASCEND_UP: sim_host_push_button(SIM_DEPTH_UP_X + 30, SIM_DEPTH_Y + 12, 0); break;
    case SURFACE_DOWN: sim_host_push_button(SIM_SURFACE_X + 30, SIM_DEPTH_Y + 12, 1); break;
    case SURFACE_UP: sim_host_push_button(SIM_SURFACE_X + 30, SIM_DEPTH_Y + 12, 0); break;
    case DEPTH:
        __ASSERT(sim_controls_depth_dm() == step->value, "Depth control failed at %d ms", step->ms); break;
    case ZOOM:
        __ASSERT(sim_host_get_zoom() == step->value, "Window zoom did not change");
        printk("ZOOM CHECK: x%u\n", sim_host_get_zoom());
        break;
    case VIEW:
        __ASSERT(watch_ui_view() == step->value, "Wrong home/menu view at %d ms", step->ms);
        printk("VIEW CHECK: %s\n", watch_ui_page_name());
        break;
    case MODE:
        __ASSERT(watch_ui_mode() == step->value, "Wrong selected mode at %d ms", step->ms);
        break;
    case DIVE_MODE:
        __ASSERT(watch_ui_dive_mode() == step->value, "Dive mode not retained at %d ms", step->ms);
        printk("DIVE MODE CHECK: %s\n", watch_ui_dive_mode() == DIVE_FREE ? "FREEDIVING" : "SCUBADIVING");
        break;
    case OPTION:
        __ASSERT(watch_ui_option() == step->value, "Option wheel direction incorrect");
        break;
    case PAGE:
        __ASSERT((int)watch_ui_page() == step->value, "Wrong page at %d ms", step->ms);
        printk("PAGE CHECK: %s\n", watch_ui_page_name());
        break;
    case WATER:
        __ASSERT(watch_ui_salt_water() == step->value, "Water setting lost"); break;
    case BRIGHTNESS:
        __ASSERT(watch_ui_brightness() == step->value, "Brightness setting lost"); break;
    case SLEEPING:
        __ASSERT(watch_ui_is_sleeping() == step->value, "Display sleep/wake failed"); break;
    case PROGRESS:
        __ASSERT(watch_ui_sleep_progress() == step->value, "Sleep ring progress incorrect"); break;
    case BLUETOOTH:
        __ASSERT(watch_ui_bluetooth() == step->value, "Bluetooth toggle incorrect"); break;
    case LEAVE_PROGRESS:
        __ASSERT(watch_ui_leave_progress() == step->value, "Leave confirmation progress incorrect"); break;
    case LIGHT_OPTION:
        __ASSERT(watch_ui_backlight_option() == step->value, "Backlight navigation incorrect"); break;
    case LIGHT_MODE:
        __ASSERT(watch_ui_backlight_mode() == step->value, "Backlight mode incorrect"); break;
    case SENSITIVITY:
        __ASSERT(watch_ui_sensitivity() == step->value, "Sensitivity incorrect"); break;
    case HAPTIC_ON:
        __ASSERT(watch_ui_haptics() == step->value, "Haptics toggle incorrect"); break;
    case HAPTIC_ROW:
        __ASSERT(watch_ui_haptics_option() == step->value, "Haptics row bounds incorrect"); break;
    case PULSE:
        __ASSERT(watch_ui_pulse() == step->value, "Haptic pulse selection incorrect"); break;
    case BT_OPTION:
        __ASSERT(watch_ui_bluetooth_option() == step->value, "Bluetooth row navigation incorrect"); break;
    case BT_DFU:
        __ASSERT(watch_ui_bluetooth_dfu() == step->value, "Bluetooth data/DFU choice incorrect"); break;
    case FACE:
        __ASSERT(watch_ui_face_option() == step->value, "Clock face menu choice incorrect"); break;
    case CLOCK_STYLE:
        __ASSERT(watch_ui_clock_style() == step->value, "Clock style not applied or retained"); break;
    case CAPTURE: {
        char path[512];
        int size = snprintf(path, sizeof(path), "%s/%s.bmp", directory, step->name);
        __ASSERT(size > 0 && size < sizeof(path), "Screenshot path too long");
        __ASSERT(sim_host_capture(path) == 0, "Screenshot failed");
        break;
    }
    case DONE:
        if (dive_check) {
            printk("DIVE CONTROLS CHECK PASS: descend, ascend and surface via SDL buttons\n");
            break;
        }
        printk("NATIVE UI CHECK PASS: compass rotation/360 wrap at both zoom levels, centred time and lower logos, two-second boot/sleep, bounded menus and settings\n");
        break;
    }
}
