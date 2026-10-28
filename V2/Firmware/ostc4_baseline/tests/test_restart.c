/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ostc4_exposure.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static ostc4_observation read_now(void)
{
    ostc4_observation r;
    CHECK(ostc4_exposure_read(&r)==OSTC4_OK);
    return r;
}
static void compare(const ostc4_observation *a,const ostc4_observation *b)
{
    CHECK(a->timestamp_seconds==b->timestamp_seconds && a->dive_seconds==b->dive_seconds);
    for(int i=0;i<OSTC4_TISSUES;++i) {
        CHECK(fabsf(a->nitrogen_bar[i]-b->nitrogen_bar[i])<=1e-6f);
        CHECK(fabsf(a->helium_bar[i]-b->helium_bar[i])<=1e-6f);
    }
    CHECK(a->gf_anchor_ambient_bar==b->gf_anchor_ambient_bar);
    CHECK(a->gf_low_depth_bar==b->gf_low_depth_bar);
    CHECK(a->absolute_pressure_bar==b->absolute_pressure_bar);
    CHECK(a->ndl_seconds==b->ndl_seconds && a->tts_seconds==b->tts_seconds);
    CHECK(fabsf(a->ceiling_meters-b->ceiling_meters)<=1e-6f);
    CHECK(memcmp(a->stop_seconds,b->stop_seconds,sizeof(a->stop_seconds))==0);
    CHECK(a->tissues==b->tissues && a->ndl==b->ndl && a->tts==b->tts && a->ceiling==b->ceiling && a->stops==b->stops);
}
static void invalid(void)
{
    ostc4_observation r=read_now(),prediction;
    CHECK(r.tissues==OSTC4_INVALID && r.ndl==OSTC4_INVALID && r.tts==OSTC4_INVALID &&
        r.ceiling==OSTC4_INVALID && r.stops==OSTC4_INVALID);
    CHECK(ostc4_exposure_advance(&(ostc4_sample){6000,1.01325f})!=OSTC4_OK);
    CHECK(ostc4_exposure_predict(&(ostc4_sample){6000,1.01325f},&prediction)!=OSTC4_OK);
    CHECK(prediction.tissues==OSTC4_INVALID);
    unsigned char bytes[OSTC4_CHECKPOINT_BYTES];memset(bytes,0xa5,sizeof(bytes));
    CHECK(ostc4_exposure_checkpoint(bytes,sizeof(bytes))!=OSTC4_OK);
    for(size_t i=0;i<sizeof(bytes);++i)CHECK(bytes[i]==0xa5);
}
/* Independent test encoder for deliberate valid-CRC malformed payloads. */
static void reseal(unsigned char *bytes)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<OSTC4_CHECKPOINT_BYTES-4;++i) {
        crc^=bytes[i];
        for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);
    }
    crc=~crc;
    for(int i=0;i<4;++i)bytes[OSTC4_CHECKPOINT_BYTES-4+i]=(unsigned char)(crc>>(8*i));
}
static void faults(const unsigned char *checkpoint)
{
    unsigned char bad[OSTC4_CHECKPOINT_BYTES];
    ostc4_recovery r={true,1920,86400,NULL,0};
    CHECK(ostc4_exposure_restore(NULL,0,&r)==OSTC4_CHECKPOINT_CORRUPT);invalid();
    CHECK(ostc4_exposure_restore(checkpoint,OSTC4_CHECKPOINT_BYTES-1,&r)==OSTC4_CHECKPOINT_CORRUPT);invalid();
    memcpy(bad,checkpoint,sizeof(bad));bad[50]^=1;
    CHECK(ostc4_exposure_restore(bad,sizeof(bad),&r)==OSTC4_CHECKPOINT_CORRUPT);invalid();
    for(int offset=0;offset<=12;offset+=4) {
        memcpy(bad,checkpoint,sizeof(bad));bad[offset]^=1;reseal(bad);
        CHECK(ostc4_exposure_restore(bad,sizeof(bad),&r)==OSTC4_CHECKPOINT_INCOMPATIBLE);invalid();
    }
    memcpy(bad,checkpoint,sizeof(bad));memset(bad+20,0xff,4);reseal(bad);
    CHECK(ostc4_exposure_restore(bad,sizeof(bad),&r)==OSTC4_CHECKPOINT_INVALID);invalid();
    /* v2 policy tuple/lock are validated, never restored by a default. */
    const unsigned offsets[]={289,290,291};
    const unsigned char values[]={0,101,2};
    for(size_t i=0;i<3;++i) {
        memcpy(bad,checkpoint,sizeof(bad));bad[offsets[i]]=values[i];reseal(bad);
        CHECK(ostc4_exposure_restore(bad,sizeof(bad),&r)==OSTC4_CHECKPOINT_INVALID);invalid();
    }
    r.time_known=false;
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),&r)==OSTC4_RESTART_TIME_UNKNOWN);invalid();
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),NULL)==OSTC4_RESTART_TIME_UNKNOWN);invalid();
    r.time_known=true;r.restart_seconds=1919;
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),&r)==OSTC4_RESTART_TIME_UNKNOWN);invalid();
    r.restart_seconds=1921;r.max_age_seconds=0;
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),&r)==OSTC4_CHECKPOINT_STALE);invalid();
    r.max_age_seconds=86400;
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),&r)==OSTC4_RESTART_EXPOSURE_UNKNOWN);invalid();
    const ostc4_sample short_history[]={{1921,4.01325f}};
    r.history=short_history;r.count=1;r.restart_seconds=1922;
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),&r)==OSTC4_RESTART_EXPOSURE_UNKNOWN);invalid();
    const ostc4_sample rejected[]={{1920,4.01325f},{5521,4.01325f},{1921,NAN},{1921,9}};
    for(size_t i=0;i<4;++i) {
        r.history=&rejected[i];r.restart_seconds=rejected[i].timestamp_seconds;
        CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),&r)==OSTC4_RESTART_EXPOSURE_REJECTED);invalid();
    }
    /* A checkpoint is valid again only when complete history is supplied. */
    r=(ostc4_recovery){true,1920,86400,NULL,0};
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(bad),&r)==OSTC4_RESTORED);
    CHECK(read_now().tissues==OSTC4_AVAILABLE);
}
static void emit(const ostc4_observation *r)
{
    printf("%u %u %.9g %.9g %.9g %.9g %d %d",r->timestamp_seconds,r->dive_seconds,
        (double)r->absolute_pressure_bar,(double)r->gf_anchor_ambient_bar,
        (double)r->gf_low_depth_bar,(double)r->ceiling_meters,r->ndl_seconds,r->tts_seconds);
    for(int i=0;i<16;++i)printf(" %.9g %.9g",(double)r->nitrogen_bar[i],(double)r->helium_bar[i]);
    for(int i=0;i<50;++i)printf(" %u",r->stop_seconds[i]);
    printf(" %d %d %d %d %d\n",r->tissues,r->ndl,r->tts,r->ceiling,r->stops);
}
int main(int argc,char **argv)
{
    const ostc4_sample profile[]={{120,4.01325f},{1920,4.01325f},{2100,1.01325f},
        {3600,1.01325f},{3720,3.01325f},{4920,3.01325f},{5040,1.01325f}};
    ostc4_observation control[7];
    unsigned char checkpoint[OSTC4_CHECKPOINT_BYTES];
    if(argc==3 && !strcmp(argv[1],"--resume")) {
        FILE *file=fopen(argv[2],"rb");CHECK(file);
        CHECK(fread(checkpoint,1,sizeof(checkpoint),file)==sizeof(checkpoint));CHECK(fclose(file)==0);
        /* This process has never initialized or executed the production engine. */
        const ostc4_recovery recovery={true,3600,86400,profile+2,2};
        CHECK(ostc4_exposure_restore(checkpoint,sizeof(checkpoint),&recovery)==OSTC4_RESTORED);
        ostc4_observation r=read_now();emit(&r);
        for(size_t i=4;i<7;++i) {
            CHECK(ostc4_exposure_advance(&profile[i])==OSTC4_OK);r=read_now();emit(&r);
        }
        return 0;
    }
    CHECK(ostc4_exposure_initialize(&(ostc4_initial){0,1.01325f})==OSTC4_OK);
    for(size_t i=0;i<7;++i) {
        CHECK(ostc4_exposure_advance(&profile[i])==OSTC4_OK);
        control[i]=read_now();
        if(i==1)CHECK(ostc4_exposure_checkpoint(checkpoint,sizeof(checkpoint))==OSTC4_OK);
    }
    if(argc==3 && !strcmp(argv[1],"--save")) {
        FILE *file=fopen(argv[2],"wb");CHECK(file);
        CHECK(fwrite(checkpoint,1,sizeof(checkpoint),file)==sizeof(checkpoint));CHECK(fclose(file)==0);
        for(size_t i=3;i<7;++i)emit(&control[i]);
        return 0;
    }
    /* Replace live state completely before recovery, including anchor history. */
    CHECK(ostc4_exposure_initialize(&(ostc4_initial){99,0.9f})==OSTC4_OK);
    const ostc4_recovery recovery={true,3600,86400,profile+2,2};
    CHECK(ostc4_exposure_restore(checkpoint,sizeof(checkpoint),&recovery)==OSTC4_RESTORED);
    ostc4_observation recovered=read_now();compare(&control[3],&recovered);
    for(size_t i=4;i<7;++i) {
        CHECK(ostc4_exposure_advance(&profile[i])==OSTC4_OK);
        recovered=read_now();compare(&control[i],&recovered);
    }
    faults(checkpoint);
    /* Deliberate fresh initialization is distinct from recovery. */
    CHECK(ostc4_exposure_initialize(&(ostc4_initial){0,1.01325f})==OSTC4_OK);
    CHECK(read_now().tissues==OSTC4_AVAILABLE);
    CHECK(read_now().nitrogen_bar[5]<control[3].nitrogen_bar[5]);
    puts("restart complete exposure agrees with uninterrupted replay");
    return 0;
}
