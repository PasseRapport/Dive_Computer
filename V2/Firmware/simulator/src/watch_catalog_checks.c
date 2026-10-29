/* Native UI journeys: real menu input, animation, cancel/apply and process restart. */
#include "watch_catalog_checks.h"
#include "watch_ui.h"
#include "sim_host.h"
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static void tap(void){watch_ui_input(WATCH_SINGLE_TAP,0);}
static void crown(int steps){watch_ui_input(WATCH_CROWN,steps);}
static void back(void){watch_ui_input(WATCH_DOUBLE_TAP,0);}
static void shot(const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",sim_host_gallery_path(),name);
    lv_refr_now(NULL);__ASSERT(!sim_host_capture(path),"Watch capture failed");
}
static void open_watch(void)
{
    crown(1);crown(24);crown(-100);crown(WATCH_OPTION);tap();crown(-100);crown(1);tap();
    __ASSERT(watch_ui_view()==WATCH_FACE_SELECT,"Watch menu missing");
}
static void clock(void)
{
    back();back();crown(-100);tap();__ASSERT(watch_ui_view()==WATCH_CLOCK,"Clock return failed");
}
static void apply(enum watch_clock_style type,int dial)
{
    open_watch();
    if(watch_ui_clock_style()!=type)tap();
    __ASSERT(watch_ui_view()==WATCH_FACE_SELECT,"Type tap left the Watch menu");
    crown(1);tap();
    __ASSERT(watch_ui_view()==WATCH_DIAL_SELECT,"Preview missing");
    crown(dial-watch_ui_dial_option());tap();
    __ASSERT(watch_ui_view()==WATCH_FACE_SELECT,"Apply did not return to Watch");
    __ASSERT((type==CLOCK_ANALOG?(int)watch_ui_dial():watch_ui_digital_dial())==dial,"Wrong face committed");clock();
}
void watch_catalog_checks_poll(struct watch_data *data)
{
    static unsigned step;static bool once,mid;
    const char *mode=getenv("DIVE_SIM_WATCH_RESTART");
    if(k_uptime_get()<2400)return;
    if(mode) {
        if(once)return;
        once=true;
        __ASSERT(watch_ui_view()==WATCH_CLOCK,"Restart did not reach clock");
        if(!strcmp(mode,"read")) {
            __ASSERT(watch_ui_clock_style()==CLOCK_DIGITAL&&watch_ui_digital_dial()==DIGITAL_SEAL&&watch_ui_dial()==DIAL_SOLAR,"Preferences not restored after restart");
            apply(CLOCK_ANALOG,DIAL_SOLAR);shot("restart-solar");
            open_watch();tap();__ASSERT(watch_ui_clock_style()==CLOCK_DIGITAL&&watch_ui_digital_dial()==DIGITAL_SEAL,"Per-type choice lost");clock();shot("restart-seal");
        } else __ASSERT(watch_ui_clock_style()==CLOCK_DIGITAL&&watch_ui_dial()==DIAL_MINIMALIST&&watch_ui_digital_dial()==DIGITAL_EMBLEM,"Malformed preferences changed defaults");
        printk("WATCH RESTART PASS: %s\n",mode);return;
    }
    if(step==1&&!mid&&k_uptime_get()>=2600){shot("carousel-transition");mid=true;}
    if(step>42||k_uptime_get()<2500+step*400)return;
    if(step==0) {
        __ASSERT(watch_face_count(CLOCK_ANALOG)==7&&watch_face_count(CLOCK_DIGITAL)==5,"Catalog count");
        __ASSERT(watch_ui_clock_style()==CLOCK_DIGITAL,"Default type");
        open_watch();tap();__ASSERT(watch_ui_clock_style()==CLOCK_ANALOG,"Type did not toggle");
        shot("watch-menu-analog");crown(1);tap();crown(-1);
        __ASSERT(watch_ui_dial_option()==7,"Missing final analog Back");
        crown(-1);
        __ASSERT(watch_ui_dial_option()==6&&watch_ui_dial_animating(),"Negative wrap or animation failed");
    } else if(step==1) {
        __ASSERT(!watch_ui_dial_animating(),"Animation did not finish");shot("carousel-solar");
        crown(1);shot("analog-face-back");tap();
        __ASSERT(watch_ui_view()==WATCH_FACE_SELECT&&watch_ui_dial()==DIAL_MINIMALIST,"Analog Back changed face");
        tap();crown(-1);crown(1);__ASSERT(watch_ui_dial_option()==0,"Positive wrap failed");
        crown(100);crown(-100);__ASSERT(watch_ui_dial_option()==0,"Rapid multi-step crown failed");
        back();__ASSERT(watch_ui_dial()==DIAL_MINIMALIST&&watch_ui_view()==WATCH_FACE_SELECT&&!watch_ui_dial_animating(),"Cancel applied draft or retained animation");clock();
    } else if(step>=2&&step<26) {
        unsigned index=(step-2)/2;enum watch_clock_style type=index<7?CLOCK_ANALOG:CLOCK_DIGITAL;int dial=index<7?index:index-7;
        if(!(step%2))apply(type,dial);
        else {char name[64];snprintf(name,sizeof(name),"%s-%d-x1",type==CLOCK_ANALOG?"analog":"digital",dial);shot(name);}
    } else if(step==26) {
        open_watch();shot("watch-menu-digital");crown(1);tap();crown(1);__ASSERT(watch_ui_dial_option()==DIGITAL_COUNT,"Digital Back missing");shot("digital-face-back");tap();
        __ASSERT(watch_ui_view()==WATCH_FACE_SELECT&&watch_ui_digital_dial()==DIGITAL_SEAL,"Digital Back changed face");
        tap();crown(2);__ASSERT(watch_ui_dial_option()==0,"Digital wrap failed");
        back();__ASSERT(watch_ui_digital_dial()==DIGITAL_SEAL,"Cancel changed digital face");clock();
        __ASSERT(!sim_host_set_zoom(2),"Scale x2 failed");
    } else if(step>=27&&step<39) {
        unsigned index=step-27;enum watch_clock_style type=index<7?CLOCK_ANALOG:CLOCK_DIGITAL;int dial=index<7?index:index-7;
        apply(type,dial);char name[64];snprintf(name,sizeof(name),"%s-%d-x2",type==CLOCK_ANALOG?"analog":"digital",dial);shot(name);
    } else if(step==39) {
        __ASSERT(!sim_host_set_zoom(1),"Scale x1 failed");apply(CLOCK_ANALOG,DIAL_RADAR);
        data->battery_percent=100;watch_ui_update(data);shot("radar-full");
        data->battery_percent=50;watch_ui_update(data);shot("radar-half");
        data->battery_percent=0;watch_ui_update(data);shot("radar-empty");
    } else if(step==40) {
        apply(CLOCK_ANALOG,DIAL_SOLAR);data->battery_percent=87;watch_ui_update(data);shot("solar-roman-87");
        data->battery_percent=0;watch_ui_update(data);shot("solar-roman-zero");
        data->battery_percent=100;watch_ui_update(data);shot("solar-roman-100");
    } else if(step==41) {
        /* Dynamic values must render through the public data interface. */
        data->clock_hour=23;data->clock_minute=59;data->clock_day=31;data->clock_month=12;
        apply(CLOCK_DIGITAL,DIGITAL_EMBLEM);watch_ui_update(data);shot("emblem-2359");
        apply(CLOCK_DIGITAL,DIGITAL_TERMINAL);watch_ui_update(data);shot("terminal-2359");
        apply(CLOCK_DIGITAL,DIGITAL_SPORT);watch_ui_update(data);shot("sport-2359");
        apply(CLOCK_DIGITAL,DIGITAL_SEAL);
    } else printk("WATCH CATALOG PASS: 7 analog / 5 digital, wrap, rapid scrolling, animation, cancel, confirm, two remembered choices, x1/x2, live data and battery extremes\n");
    step++;
}
