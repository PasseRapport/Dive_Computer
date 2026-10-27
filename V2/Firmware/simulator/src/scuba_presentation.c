#include "scuba_presentation.h"
#include <stdio.h>
struct scuba_ascent_display scuba_ascent_display(int32_t speed, uint32_t fast_ms)
{
    unsigned levels=speed<10?0:speed<20?1:(unsigned)speed/20;
    if(levels>5)levels=5;
    return (struct scuba_ascent_display){levels,speed>100?ASCENT_RED:speed>80?ASCENT_YELLOW:speed>=20?ASCENT_GREEN:ASCENT_GREY,speed>100&&fast_ms>=5000};
}
void scuba_surface_text(char *out,size_t size,uint32_t seconds)
{
    if(seconds<3600)snprintf(out,size,"%02u:%02u",seconds/60,seconds%60);
    else if(seconds<86400)snprintf(out,size,"%uh%02u",seconds/3600,(seconds%3600)/60);
    else if(seconds<604800)snprintf(out,size,"%uh",seconds/3600);
    else snprintf(out,size,"%ud",seconds/86400);
}
void scuba_ndl_text(char *out,size_t size,uint32_t seconds)
{
    unsigned minutes=seconds/60;
    if(minutes>99)snprintf(out,size,">99");
    else snprintf(out,size,"%u",minutes);
}
uint16_t scuba_demo_ppo2(uint16_t depth_dm) {return ((100u+depth_dm)*21u+50)/100;}

unsigned scuba_ring_ndl(uint32_t seconds) {return seconds>=3600?1000:(uint64_t)seconds*1000/3600;}
unsigned scuba_ring_tts(uint32_t seconds) {return seconds>=3000?1000:(uint64_t)seconds*1000/3000;}
unsigned scuba_ring_stop(uint32_t elapsed_ms,uint32_t target_ms)
{return !target_ms||elapsed_ms>=target_ms?0:(uint64_t)(target_ms-elapsed_ms)*1000/target_ms;}
