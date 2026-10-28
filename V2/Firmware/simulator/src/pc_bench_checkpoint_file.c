/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "pc_bench_checkpoint_file.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Fixed 64-byte PC lifecycle envelope + portable v2 + CRC. The numerical
 * checkpoint remains the only representation of tissues and planner state. */
#define HEADER 64
#define FILE_BYTES (HEADER+OSTC4_CHECKPOINT_BYTES+4)
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put(unsigned char *p,uint32_t v)
{for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static uint32_t crc(const unsigned char *p,size_t n)
{
    uint32_t c=UINT32_MAX;
    while(n--) {c^=*p++;for(unsigned i=0;i<8;++i)c=c>>1^((c&1)?UINT32_C(0xedb88320):0);}
    return ~c;
}
static uint32_t fingerprint(bool legacy)
{
    static const unsigned char old[]=OSTC4_REVISION "\n" OSTC4_POLICY "\n" OSTC4_CORRECTION_SET "\n"
        OSTC4_EXPOSURE_POLICY "\nieee-binary32-binary64-no-fast-math-no-contract-checkpoint-v1";
    static const unsigned char current[]=OSTC4_REVISION "\n" OSTC4_POLICY "\n" OSTC4_CORRECTION_SET "\n"
        OSTC4_PLANNER_CORRECTION_SET "\n" OSTC4_CONFIGURABLE_GF_POLICY "\nieee-binary32-binary64-no-fast-math-no-contract-checkpoint-v2";
    return legacy?crc(old,sizeof(old)-1):crc(current,sizeof(current)-1);
}
const char *pc_bench_checkpoint_file_default_path(void)
{
    static char path[4096];const char *dir=getenv("DIVE_WATCH_STATE_DIR");
    if(!dir || !*dir)return NULL;
    int n=snprintf(path,sizeof(path),"%s/recovery.checkpoint",dir);
    return n<0 || (size_t)n>=sizeof(path)?NULL:path;
}
int pc_bench_checkpoint_file_save(const char *path)
{
    struct pc_bench_checkpoint_state state;
    if(!path || !*path || pc_bench_checkpoint_export(&state)) {
        pc_bench_checkpoint_notice("Disk checkpoint save failed: path or valid exposure unavailable");return -1;
    }
    unsigned char b[FILE_BYTES]={0};memcpy(b,"DWCP",4);put(b+4,1);put(b+8,sizeof(b));
    uint32_t bits;memcpy(&bits,&state.density,4);put(b+12,bits);
    b[16]=state.preferred.low;b[17]=state.preferred.high;
    b[18]=state.recording.armed;b[19]=state.recording.active;
    put(b+20,state.recording.start_depth_dm);put(b+24,state.recording.started_seconds);
    put(b+28,state.recording.completed_seconds);put(b+32,state.recording.dive_seconds);
    put(b+36,state.recording.surface_seconds);put(b+40,state.recording.dives_started);
    put(b+44,state.recording.dives_completed);
    memcpy(b+HEADER,state.core,sizeof(state.core));put(b+sizeof(b)-4,crc(b,sizeof(b)-4));
    char temporary[4096];int n=snprintf(temporary,sizeof(temporary),"%s.tmp.XXXXXX",path);
    if(n<0 || (size_t)n>=sizeof(temporary))goto failed;
    int fd=mkstemp(temporary);if(fd<0)goto failed;
    FILE *f=fdopen(fd,"wb");if(!f){close(fd);unlink(temporary);goto failed;}
    bool okay=fwrite(b,1,sizeof(b),f)==sizeof(b) && fflush(f)==0 && fsync(fd)==0;
    if(fclose(f))okay=false;
    if(!okay || rename(temporary,path)) {unlink(temporary);goto failed;}
    pc_bench_checkpoint_notice("Disk checkpoint saved: exposure, GF pair and recording lock");return 0;
failed:
    pc_bench_checkpoint_notice("Disk checkpoint write failed; live state and prior file retained");return -1;
}
static bool lifecycle_valid(const struct pc_bench_checkpoint_state *s)
{
    const struct recorded_dive *r=&s->recording;uint32_t t=u32(s->core+16);
    return isfinite(s->density) && s->density>=990 && s->density<=1040 &&
        s->preferred.low>=1 && s->preferred.high<=100 && s->preferred.low<=s->preferred.high &&
        r->start_depth_dm>=5 && r->start_depth_dm<=100 && r->start_depth_dm%5==0 &&
        r->started_seconds<=t && r->completed_seconds<=t && r->dive_seconds<=t &&
        r->surface_seconds<=t && r->dives_completed<=r->dives_started &&
        r->dives_started-r->dives_completed==(r->active?1u:0u) &&
        (!r->active || (r->armed && r->dives_started>r->dives_completed && r->surface_seconds<300)) &&
        r->active==(s->core[291]!=0);
}
static ostc4_restart_status load(const char *path,const ostc4_recovery *evidence,bool saved_instant,bool configuration_unchanged)
{
    unsigned char b[FILE_BYTES+1];size_t n=0;
    FILE *f=path?fopen(path,"rb"):NULL;
    if(!f)return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_CORRUPT);
    n=fread(b,1,sizeof(b),f);bool bad=ferror(f)!=0;if(fclose(f))bad=true;
    if(bad)return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_CORRUPT);
    struct pc_bench_checkpoint_state s={0};bool legacy=false;
    if(n==293) {
        if(crc(b,289)!=u32(b+289))return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_CORRUPT);
        if(u32(b)!=UINT32_C(0x31435845) || u32(b+4)!=1 || u32(b+8)!=fingerprint(true) || u32(b+12)!=293)
            return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_INCOMPATIBLE);
        /* Recognized immutable fixed-policy source is retained byte-for-byte on
         * disk. Migration exists only in memory and is never saved as healthy. */
        memcpy(s.core,b,289);put(s.core+4,2);put(s.core+8,fingerprint(false));put(s.core+12,OSTC4_CHECKPOINT_BYTES);
        s.core[289]=30;s.core[290]=70;s.core[291]=1;put(s.core+292,crc(s.core,292));
        s.preferred=(ostc4_gf_pair){30,70};s.density=1025;legacy=true;
    } else {
        if(n!=FILE_BYTES || crc(b,n-4)!=u32(b+n-4))return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_CORRUPT);
        if(memcmp(b,"DWCP",4) || u32(b+4)!=1 || u32(b+8)!=FILE_BYTES)
            return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_INCOMPATIBLE);
        for(size_t i=48;i<HEADER;++i)if(b[i])return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_INCOMPATIBLE);
        uint32_t bits=u32(b+12);memcpy(&s.density,&bits,4);
        s.preferred=(ostc4_gf_pair){b[16],b[17]};
        if(b[18]>1 || b[19]>1 || u32(b+20)>UINT16_MAX)return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_INVALID);
        s.recording=(struct recorded_dive){b[18]!=0,b[19]!=0,(uint16_t)u32(b+20),
            u32(b+24),u32(b+28),u32(b+32),u32(b+36),u32(b+40),u32(b+44)};
        memcpy(s.core,b+HEADER,sizeof(s.core));
        if(!lifecycle_valid(&s))return pc_bench_checkpoint_reject(OSTC4_CHECKPOINT_INVALID);
    }
    ostc4_recovery instant={true,u32(s.core+16),0,NULL,0};
    if(saved_instant){evidence=&instant;configuration_unchanged=true;}
    if(evidence && evidence->time_known && !configuration_unchanged)
        return pc_bench_checkpoint_reject(OSTC4_RESTART_CONFIGURATION_UNKNOWN);
    if(evidence && evidence->count>=PC_BENCH_HISTORY)return pc_bench_checkpoint_reject(OSTC4_RESTART_EXPOSURE_REJECTED);
    return pc_bench_checkpoint_import(&s,evidence,legacy);
}
ostc4_restart_status pc_bench_checkpoint_file_load(const char *path,const ostc4_recovery *evidence,bool configuration_unchanged)
{return load(path,evidence,false,configuration_unchanged);}
ostc4_restart_status pc_bench_checkpoint_file_load_saved_instant(const char *path)
{return load(path,NULL,true,true);}
