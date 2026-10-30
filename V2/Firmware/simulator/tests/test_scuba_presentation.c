#include "scuba_presentation.h"
#include <assert.h>
#include <math.h>
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
    /* Literal formatting expectations, independent of the raw endpoint oracle. */
    scuba_ceiling_pair_text(out,sizeof(out),13.362f,true,7.249f,true);
    assert(!strcmp(out,"13.4 / 7.2 m"));
    scuba_ceiling_pair_text(out,sizeof(out),-0.1f,true,0,true);
    assert(!strcmp(out,"0.0 / 0.0 m"));
    scuba_ceiling_pair_text(out,sizeof(out),123.44f,true,100.06f,true);
    assert(!strcmp(out,"123.4 / 100.1 m"));
    scuba_ceiling_pair_text(out,sizeof(out),7.25f,true,7.25f,true);
    assert(!strcmp(out,"7.3 / 7.3 m"));
    scuba_ceiling_pair_text(out,sizeof(out),0,false,0,false);
    assert(!strcmp(out,"-- / -- m"));
    scuba_ceiling_pair_text(out,sizeof(out),NAN,true,3.14f,true);
    assert(!strcmp(out,"-- / 3.1 m"));
    /* Exact approved wording and total hours, with persistent zero. */
    const struct {uint32_t minutes;const char *text;} estimates[]={
        {0,"Désaturation : 0 h 00 min"},{1,"Désaturation : 0 h 01 min"},
        {59,"Désaturation : 0 h 59 min"},{60,"Désaturation : 1 h 00 min"},
        {61,"Désaturation : 1 h 01 min"},{754,"Désaturation : 12 h 34 min"},
        {3517,"Désaturation : 58 h 37 min"}};
    char desaturation[64];
    for(unsigned i=0;i<sizeof(estimates)/sizeof(estimates[0]);++i) {
        scuba_desaturation_text(desaturation,sizeof(desaturation),estimates[i].minutes,true);
        assert(!strcmp(desaturation,estimates[i].text));
    }
    scuba_desaturation_text(desaturation,sizeof(desaturation),754,false);
    assert(!strcmp(desaturation,"Désaturation : -- h -- min"));
    puts("SCUBA PRESENTATION PASS: NDL >99, surface formats, ascent thresholds/duration and air ppO2 preview");
}
