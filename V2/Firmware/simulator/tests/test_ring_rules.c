#include "scuba_model.h"
#include "scuba_presentation.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(scuba_ring_ndl(0)==0&&scuba_ring_ndl(300)==83&&scuba_ring_ndl(1800)==500&&scuba_ring_ndl(3600)==1000&&scuba_ring_ndl(7200)==1000);
 assert(scuba_ring_tts(0)==0&&scuba_ring_tts(1500)==500&&scuba_ring_tts(3000)==1000&&scuba_ring_tts(6000)==1000);
 assert(scuba_ring_stop(0,120000)==1000&&scuba_ring_stop(60000,120000)==500&&scuba_ring_stop(120000,120000)==0&&scuba_ring_stop(1,0)==0);
 for(int speed=1;speed<=60;speed+=(speed==1?9:50))for(int rate=-60;rate<=60;rate++){
  struct scuba_model m;scuba_model_init(&m);scuba_model_arm(&m,0);m.speed=speed;scuba_model_update_mm(&m,0,50000,18,87);
  for(unsigned t=50;t*speed<=30000;t+=50){
   int travel=-rate*(int)t*speed;int dm=500+travel/6000,rem=travel%6000;
   scuba_model_update_mm(&m,t,dm*100+rem/60,18,87);
   if(t*speed>=1000&&m.ascent_dm_min!=rate*10){printf("rate=%d speed=%d t=%u actual=%d\n",rate,speed,t,m.ascent_dm_min);return 1;}
  }
 }
 puts("RING/RATE PASS: fixed NDL 0-60, TTS 0-50, remaining stop timer; all 121 constant rates stable at x1/x10/x60");
}
