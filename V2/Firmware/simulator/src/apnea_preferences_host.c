/* Native host adapter only. No exposure, GF, log or session state is serialized. */
#define _POSIX_C_SOURCE 200809L
#include "sim_host.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int state_directory(char *out,size_t size)
{
 const char *explicit_path=getenv("DIVE_WATCH_STATE_DIR");int n;
 if(!explicit_path) {
  static const char *const checks[]={"DIVE_SIM_GALLERY","DIVE_SIM_CHECK_CONTROLS","DIVE_SIM_SCALE_CHECK",
   "DIVE_SIM_WATER_CHECK","DIVE_SIM_DIVE_CHECK","DIVE_SIM_SCUBA_CHECK","DIVE_SIM_APNEA_CHECK",
   "DIVE_SIM_APNEA_SETTINGS_CHECK","DIVE_SIM_APNEA_PERSISTENCE","DIVE_SIM_APPEARANCE_CHECK","DIVE_SIM_BENCH_CHECK"};
  for(size_t i=0;i<sizeof(checks)/sizeof(checks[0]);i++)if(getenv(checks[i]))return 1;
 }
 if(explicit_path)n=snprintf(out,size,"%s",explicit_path);
 else {
  const char *xdg=getenv("XDG_STATE_HOME"),*home=getenv("HOME");
  if(xdg&&*xdg)n=snprintf(out,size,"%s/dive-watch-simulator",xdg);
  else if(home&&*home)n=snprintf(out,size,"%s/.local/state/dive-watch-simulator",home);
  else return -1;
 }
 return n<=0||(size_t)n>=size?-1:0;
}
static int make_directory(const char *directory)
{
 char path[4096];snprintf(path,sizeof(path),"%s",directory);
 for(char *p=path+1;*p;p++)if(*p=='/'){
  *p=0;if(mkdir(path,0700)&&errno!=EEXIST)return -1;*p='/';
 }
 return mkdir(path,0700)&&errno!=EEXIST?-1:0;
}
int sim_host_apnea_preferences_load(int *salt,int *field)
{
 char directory[4096],path[4096],content[128]={0},canonical[128];
 int status=state_directory(directory,sizeof(directory));if(status)return status;
 if(snprintf(path,sizeof(path),"%s/apnea-preferences.txt",directory)>=(int)sizeof(path))return -1;
 FILE *file=fopen(path,"r");if(!file)return errno==ENOENT?1:-1;
 size_t bytes=fread(content,1,sizeof(content)-1,file);
 bool failed=ferror(file)||fgetc(file)!=EOF;if(fclose(file))failed=true;
 unsigned version=0,water=0,bottom=0;
 int fields=sscanf(content,"DIVE_WATCH_APNEA %u %u %u",&version,&water,&bottom);
 int expected=snprintf(canonical,sizeof(canonical),"DIVE_WATCH_APNEA 1 %u %u\n",water,bottom);
 /* Version 1 uses stable water 0/1 and bottom-field 0..4 identifiers (2 now selects cumulative underwater time). */
 if(failed||fields!=3||version!=1||water>1||bottom>4||expected<0||bytes!=(size_t)expected||memcmp(content,canonical,bytes))return -1;
 *salt=water;*field=bottom;return 0;
}
static int save_preference(const char *name,const char *tag,const char *content)
{
 char directory[4096],target[4096],temporary[4096],backup[4128];
 int status=state_directory(directory,sizeof(directory));if(status==1)return 0;
 if(status||make_directory(directory))goto failed;
 if(snprintf(target,sizeof(target),"%s/%s",directory,name)>=(int)sizeof(target)||
    snprintf(temporary,sizeof(temporary),"%s/.%s-pending-XXXXXX",directory,tag)>=(int)sizeof(temporary))goto failed;
 int dir=open(directory,O_RDONLY|O_DIRECTORY);if(dir<0)goto failed;
 if(fsync(dir)){close(dir);goto failed;}
 int fd=mkstemp(temporary);if(fd<0){close(dir);goto failed;}
 size_t size=strlen(content),offset=0;
 int result=0;
 while(offset<size){ssize_t written=write(fd,content+offset,size-offset);if(written<0&&errno==EINTR)continue;if(written<=0){result=-1;break;}offset+=(size_t)written;}
 if(!result&&fsync(fd))result=-1;
 if(close(fd))result=-1;
 bool backed_up=false,committed=false,keep_backup=false;
 snprintf(backup,sizeof(backup),"%s.previous",temporary);
 if(!result){if(!link(target,backup))backed_up=true;else if(errno!=ENOENT)result=-1;}
 if(!result){if(rename(temporary,target))result=-1;else committed=true;}
 if(committed&&fsync(dir)){
  result=-1;
  if(backed_up){if(!rename(backup,target))backed_up=false;else keep_backup=true;}
  else unlink(target);
  (void)fsync(dir);
 }
 if(backed_up&&!keep_backup)unlink(backup);
 if(result)unlink(temporary);
 close(dir);
 if(!result)return 0;
failed:
 fprintf(stderr,"%s preferences were not saved; retaining the prior file\n",tag);return -1;
}

int sim_host_apnea_preferences_save(int salt,int field)
{
 if(salt<0||salt>1||field<0||field>4)return -1;
 char content[64];snprintf(content,sizeof(content),"DIVE_WATCH_APNEA 1 %d %d\n",salt,field);
 return save_preference("apnea-preferences.txt","apnea",content);
}
int sim_host_water_preferences_load(int *salt)
{
 char directory[4096],path[4096],content[64]={0},canonical[64];
 int status=state_directory(directory,sizeof(directory));if(status)return status;
 if(snprintf(path,sizeof(path),"%s/water-preferences.txt",directory)>=(int)sizeof(path))return -1;
 FILE *file=fopen(path,"r");if(!file)return errno==ENOENT?1:-1;
 size_t bytes=fread(content,1,sizeof(content)-1,file);
 bool failed=ferror(file)||fgetc(file)!=EOF;if(fclose(file))failed=true;
 unsigned version=0,water=0;
 int fields=sscanf(content,"DIVE_WATCH_WATER %u %u",&version,&water);
 int expected=snprintf(canonical,sizeof(canonical),"DIVE_WATCH_WATER 1 %u\n",water);
 if(failed||fields!=2||version!=1||water>1||expected<0||bytes!=(size_t)expected||memcmp(content,canonical,bytes))return -1;
 *salt=water;return 0;
}
int sim_host_water_preferences_save(int salt)
{
 if(salt<0||salt>1)return -1;
 char content[64];snprintf(content,sizeof(content),"DIVE_WATCH_WATER 1 %d\n",salt);
 return save_preference("water-preferences.txt","water",content);
}
