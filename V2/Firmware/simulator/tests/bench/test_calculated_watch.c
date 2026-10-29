/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scuba_calculated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%d: %s\n",__LINE__,#x); exit(1); } } while(0)
int main(void)
{
    struct scuba_model m;
    scuba_model_init(&m);m.salt_water=true;scuba_model_arm(&m,0);
    ostc4_observation o={.tissues=OSTC4_AVAILABLE,.ndl=OSTC4_AVAILABLE,
        .tts=OSTC4_AVAILABLE,.ceiling=OSTC4_AVAILABLE,.stops=OSTC4_AVAILABLE,
        .ndl_seconds=240,.tts_seconds=600,.ceiling_meters=4.05f};
    o.stop_seconds[1]=120;
    scuba_calculated_apply(&m,&o,60,30.0f,18,87);
    CHECK(m.calculated && m.calculated_state==SCUBA_CALCULATED_AVAILABLE);
    CHECK(m.active && m.ndl_s==240 && scuba_model_tts(&m)==600);
    CHECK(scuba_model_ceiling(&m)==41 && scuba_model_stop_depth(&m)==60);
    scuba_calculated_apply(&m,&o,61,6.0f,18,87);
    CHECK(scuba_model_in_stop(&m) && scuba_model_stop_target(&m)==120000);
    o.stop_seconds[1]=80;o.tts_seconds=550;
    scuba_calculated_apply(&m,&o,62,6.0f,18,87);
    CHECK(m.stop_ms==40000 && scuba_model_stop_target(&m)==120000);
    CHECK(scuba_model_tts(&m)==550); /* TTS is not this stop's countdown. */
    /* The raw planner holds ten-second slots; the watch ticks between them. */
    for(unsigned second=63;second<=71;++second) {
        scuba_calculated_apply(&m,&o,second,6.0f,18,87);
        CHECK(m.calculated_stop_remaining_ms==(80-(second-62))*1000);
        CHECK(o.stop_seconds[1]==80 && scuba_model_tts(&m)==550);
    }
    scuba_calculated_apply(&m,&o,71,6.0f,18,87);
    CHECK(m.calculated_stop_remaining_ms==71000); /* paused/repeated sample */
    o.stop_seconds[1]=70;
    scuba_calculated_apply(&m,&o,72,6.0f,18,87);
    CHECK(m.calculated_stop_remaining_ms==70000); /* rebase to planner */
    scuba_calculated_apply(&m,&o,73,10.0f,18,87);
    CHECK(m.calculated_stop_remaining_ms==70000); /* outside stop zone */
    scuba_calculated_apply(&m,&o,74,6.0f,18,87);
    CHECK(m.calculated_stop_remaining_ms==70000); /* no credit for return */
    o.stop_seconds[1]=10;
    scuba_calculated_apply(&m,&o,75,6.0f,18,87);
    scuba_calculated_apply(&m,&o,100,6.0f,18,87);
    CHECK(m.calculated_stop_remaining_ms==1000 && !m.deco_done);
    scuba_model_acknowledge(&m);
    CHECK(m.notifications==0 && scuba_model_stop_target(&m)==120000);
    scuba_calculated_apply(&m,&o,101,4.02f,18,87);
    CHECK(m.alerts&SCUBA_CEILING); /* raw ceiling, before decimal rounding */
    scuba_model_acknowledge(&m);
    scuba_calculated_apply(&m,&o,102,4.02f,18,87);
    CHECK(!(m.notifications&SCUBA_CEILING) && (m.alerts&SCUBA_CEILING));
    o.tts=OSTC4_INVALID;
    scuba_calculated_apply(&m,&o,102,4.02f,18,87);
    CHECK(m.calculated_state==SCUBA_CALCULATED_INVALID);
    CHECK(scuba_model_stop_depth(&m)==60); /* last known obligation retained */
    o.tts=OSTC4_UNAVAILABLE;
    scuba_calculated_apply(&m,&o,102,4.02f,18,87);
    CHECK(m.calculated_state==SCUBA_CALCULATED_UNAVAILABLE);
    o.tts=OSTC4_LIMITED;
    scuba_calculated_apply(&m,&o,102,4.02f,18,87);
    CHECK(m.calculated_state==SCUBA_CALCULATED_LIMITED);
    o.tts=OSTC4_AVAILABLE;o.tts_seconds=0;o.ceiling_meters=0;
    memset(o.stop_seconds,0,sizeof(o.stop_seconds));o.ndl_seconds=3600;
    scuba_calculated_apply(&m,&o,103,6,18,87);
    CHECK(scuba_model_stop_depth(&m)==0 && m.deco_done && !m.safety_required);
    struct scuba_model safety;scuba_model_init(&safety);scuba_model_arm(&safety,0);
    scuba_calculated_apply(&safety,&o,1,12,18,87);
    scuba_calculated_apply(&safety,&o,2,5,18,87);
    CHECK(safety.safety_required && scuba_model_stop_target(&safety)==180000);
    CHECK(scuba_model_tts(&safety)==0); /* optional stop excluded */
    scuba_calculated_apply(&safety,&o,182,5,18,87);
    CHECK(safety.stop_ms==180000);
    return 0;
}
