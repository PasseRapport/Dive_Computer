/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Text commands enter only through the public PC configuration/exposure seam. */
#include "pc_bench.h"
#include <stdio.h>
#include <string.h>
static void emit(void)
{
    const struct bench_view *v=pc_bench_read();
    const ostc4_observation *r=&v->result;
    printf("{\"gf_pair\":[%u,%u],\"gf_locked\":%s,\"timestamp_seconds\":%u,\"dive_seconds\":%u,\"absolute_pressure_bar\":%.9g,\"gf_anchor_ambient_bar\":%.9g,\"gf_low_depth_bar\":%.9g,\"ceiling_meters\":%.9g,\"ndl_seconds\":%d,\"tts_seconds\":%d,",
        r->gf_pair.low,r->gf_pair.high,r->gf_locked?"true":"false",r->timestamp_seconds,
        r->dive_seconds,(double)r->absolute_pressure_bar,(double)r->gf_anchor_ambient_bar,
        (double)r->gf_low_depth_bar,(double)r->ceiling_meters,r->ndl_seconds,r->tts_seconds);
    printf("\"gf_low_ceiling_meters\":%.9g,\"gf_high_ceiling_meters\":%.9g,", (double)r->gf_low_ceiling_meters,(double)r->gf_high_ceiling_meters);
    printf("\"endpoint_availability\":[\"%s\",\"%s\"],", ostc4_availability_name(r->gf_low_ceiling),ostc4_availability_name(r->gf_high_ceiling));
    printf("\"desaturation_minutes\":%d,\"desaturation_availability\":\"%s\",",r->desaturation_minutes,ostc4_availability_name(r->desaturation));
    printf("\"nitrogen_bar\":[");
    for(int i=0;i<OSTC4_TISSUES;++i)printf("%s%.9g",i?",":"",(double)r->nitrogen_bar[i]);
    printf("],\"stop_seconds\":[");
    for(int i=0;i<OSTC4_STOPS;++i)printf("%s%u",i?",":"",r->stop_seconds[i]);
    printf("],\"availability\":{\"tissues\":\"%s\",\"ndl\":\"%s\",\"tts\":\"%s\",\"ceiling\":\"%s\",\"stops\":\"%s\"}}\n",
        ostc4_availability_name(r->tissues),ostc4_availability_name(r->ndl),
        ostc4_availability_name(r->tts),ostc4_availability_name(r->ceiling),ostc4_availability_name(r->stops));
}
int main(void)
{
    char line[128],command[32];
    while(fgets(line,sizeof(line),stdin)) {
        if(sscanf(line,"%31s",command)!=1)continue;
        int low,high;unsigned seconds;float pressure;
        if(!strcmp(command,"fresh")) {
            if(sscanf(line,"%*s%d%d",&low,&high)!=2 || pc_bench_fresh(1.01325f,1025,NULL,0) ||
                pc_bench_set_gf_pair(low,high))return 1;
        } else if(!strcmp(command,"advance")) {
            if(sscanf(line,"%*s%u%f",&seconds,&pressure)!=2 ||
                pc_bench_accept_sample(&(ostc4_sample){seconds,pressure}))return 2;
        } else if(!strcmp(command,"pair")) {
            if(sscanf(line,"%*s%d%d",&low,&high)!=2 || pc_bench_set_gf_pair(low,high))return 3;
        } else if(!strcmp(command,"finish")) {
            if(pc_bench_recording_finish())return 4;
            continue;
        } else if(!strcmp(command,"arm")) {pc_bench_recording_arm();continue;}
        else return 5;
        emit();
    }
    return 0;
}
