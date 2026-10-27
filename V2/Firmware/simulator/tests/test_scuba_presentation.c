#include "scuba_presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct scuba_ascent_display a=scuba_ascent_display(-100,0);assert(!a.levels&&a.color==ASCENT_GREY);
    a=scuba_ascent_display(0,0);assert(!a.levels&&a.color==ASCENT_GREY);
    a=scuba_ascent_display(9,0);assert(!a.levels&&a.color==ASCENT_GREY);
    a=scuba_ascent_display(10,0);assert(a.levels==1&&a.color==ASCENT_GREY);
    a=scuba_ascent_display(19,0);assert(a.levels==1&&a.color==ASCENT_GREY);
    a=scuba_ascent_display(20,0);assert(a.levels==1&&a.color==ASCENT_GREEN);
    a=scuba_ascent_display(40,0);assert(a.levels==2&&a.color==ASCENT_GREEN);
    a=scuba_ascent_display(60,0);assert(a.levels==3&&a.color==ASCENT_GREEN);
    a=scuba_ascent_display(80,0);assert(a.levels==4&&a.color==ASCENT_GREEN);
    a=scuba_ascent_display(81,0);assert(a.color==ASCENT_YELLOW);
    a=scuba_ascent_display(100,5000);assert(a.levels==5&&a.color==ASCENT_YELLOW&&!a.highlighted);
    a=scuba_ascent_display(101,4999);assert(a.color==ASCENT_RED&&!a.highlighted);
    a=scuba_ascent_display(101,5000);assert(a.highlighted);
    char out[32];scuba_ndl_text(out,sizeof(out),6000);assert(!strcmp(out,">99"));
    scuba_ndl_text(out,sizeof(out),5999);assert(!strcmp(out,"99"));
    scuba_surface_text(out,sizeof(out),65);assert(!strcmp(out,"01:05"));
    scuba_surface_text(out,sizeof(out),3660);assert(!strcmp(out,"1h01"));
    scuba_surface_text(out,sizeof(out),86400);assert(!strcmp(out,"24h"));
    scuba_surface_text(out,sizeof(out),604800);assert(!strcmp(out,"7d"));
    assert(scuba_demo_ppo2(0)==21&&scuba_demo_ppo2(200)==63);
    puts("SCUBA PRESENTATION PASS: NDL >99, surface formats, ascent thresholds/duration and air ppO2 preview");
}
