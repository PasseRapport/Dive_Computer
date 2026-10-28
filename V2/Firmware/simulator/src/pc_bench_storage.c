/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "pc_bench_storage.h"
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <unistd.h>
static struct pc_bench_storage_state state;
static char directory[4096];
static unsigned long long *log_ids, next_log_id=1;
static void scan_logs(void);
static bool valid_pair(unsigned low,unsigned high) {return low>=1 && high<=100 && low<=high;}
const struct pc_bench_storage_state *pc_storage_read(void) {return &state;}
static int path_for(char *out,size_t count,const char *name)
{
    return snprintf(out,count,"%s/%s",directory,name)>=(int)count?-1:0;
}
static int make_directory(void)
{
    char path[sizeof(directory)];strcpy(path,directory);
    for(char *p=path+1;*p;++p)if(*p=='/') {
        *p=0;
        if(mkdir(path,0700) && errno!=EEXIST)return -1;
        *p='/';
    }
    return mkdir(path,0700) && errno!=EEXIST?-1:0;
}
/* Flush bytes before atomic replacement. Failed preparation never touches the
 * last committed file. Temporary files are unique to this process/operation. */
static int atomic_write(const char *name,const char *content,bool immutable)
{
    char target[4096],temporary[4096];
    if(path_for(target,sizeof(target),name)||path_for(temporary,sizeof(temporary),".pending-XXXXXX"))return -1;
    int directory_fd=open(directory,O_RDONLY|O_DIRECTORY);
    if(directory_fd<0)return -1;
    if(fsync(directory_fd)){close(directory_fd);return -1;}
    int fd=mkstemp(temporary);if(fd<0){close(directory_fd);return -1;}
    size_t size=strlen(content),offset=0;int result=0;
    while(offset<size) {
        ssize_t written=write(fd,content+offset,size-offset);
        if(written<0 && errno==EINTR)continue;
        if(written<=0){result=-1;break;}
        offset+=(size_t)written;
    }
    if(!result && fsync(fd))result=-1;
    if(close(fd))result=-1;
    char backup[4128];bool backed_up=false,committed=false;
    if(!result && !immutable) {
        snprintf(backup,sizeof(backup),"%s.previous",temporary);
        if(link(target,backup)==0)backed_up=true;
        else if(errno!=ENOENT)result=-1;
    }
    if(!result) {
        if(immutable) {if(link(temporary,target))result=-1;else unlink(temporary);}
        else if(rename(temporary,target))result=-1;
    }
    if(!result)committed=true;
    bool keep_backup=false;
    if(committed && fsync(directory_fd)) {
        result=-1;
        if(backed_up) {if(rename(backup,target)==0)backed_up=false;else keep_backup=true;}
        else unlink(target);
        (void)fsync(directory_fd);
    }
    if(backed_up && !keep_backup)unlink(backup);
    if(result)unlink(temporary);
    close(directory_fd);
    return result;
}
int pc_storage_configure(const char *path)
{
    free(log_ids);log_ids=NULL;next_log_id=1;
    memset(&state,0,sizeof(state));directory[0]=0;
    if(!path)return 0;
    state.enabled=true;
    if(!*path || strlen(path)>=sizeof(directory)) {
        state.preference_error="State path invalid; preference not loaded";return -1;
    }
    strcpy(directory,path);
    if(make_directory()) {
        state.preference_error="State directory unavailable; preference not loaded";return -1;
    }
    scan_logs();
    char file_path[4096];
    if(path_for(file_path,sizeof(file_path),"preference.txt"))return -1;
    FILE *file=fopen(file_path,"r");
    if(!file) {
        if(errno!=ENOENT)state.preference_error="Preference read failed; using default 30/70";
        return 0;
    }
    unsigned version=0,low=0,high=0;char content[128]={0},canonical[128];
    size_t bytes=fread(content,1,sizeof(content)-1,file);
    bool failed=ferror(file) || fgetc(file)!=EOF;if(fclose(file))failed=true;
    int fields=sscanf(content,"DIVE_WATCH_GF %u %u %u",&version,&low,&high);
    int expected=snprintf(canonical,sizeof(canonical),"DIVE_WATCH_GF 1 %u %u\n",low,high);
    if(failed || fields!=3 || version!=1 || !valid_pair(low,high) ||
        expected<0 || bytes!=(size_t)expected || memcmp(content,canonical,bytes)) {
        state.preference_error="Invalid preference schema or GF pair; using default 30/70";return 0;
    }
    state.saved_preference=(ostc4_gf_pair){(uint8_t)low,(uint8_t)high};
    state.preference_loaded=state.preference_saved=true;
    return 0;
}
void pc_storage_save_preference(ostc4_gf_pair pair)
{
    if(!state.enabled)return;
    char content[96];snprintf(content,sizeof(content),"DIVE_WATCH_GF 1 %u %u\n",pair.low,pair.high);
    state.preference_saved=false;
    if(!valid_pair(pair.low,pair.high)||atomic_write("preference.txt",content,false)) {
        state.preference_error="Preference write failed; committed GF is NOT saved";return;
    }
    state.saved_preference=pair;state.preference_saved=true;state.preference_error=NULL;
}

uint64_t pc_storage_new_session_id(void)
{
    uint64_t id=0;unsigned char *p=(unsigned char *)&id;size_t left=sizeof(id);
    while(left) {
        ssize_t n=getrandom(p,left,0);
        if(n<0 && errno==EINTR)continue;
        if(n<=0) {state.log_error="Recording identity unavailable; logs cannot be saved";return 0;}
        p+=n;left-=(size_t)n;
    }
    if(!id)return pc_storage_new_session_id();
    return id;
}
static int read_log_id(unsigned long long id,struct pc_bench_log *record)
{
    char name[64],path[4096],extra;unsigned version,low,high,automatic;
    struct pc_bench_log candidate={0};
    snprintf(name,sizeof(name),"dive-%020llu.log",id);
    if(path_for(path,sizeof(path),name))return -1;
    FILE *file=fopen(path,"r");if(!file)return -1;
    char row[512]={0};bool failed=!fgets(row,sizeof(row),file) || ferror(file);
    if(!failed && fgetc(file)!=EOF)failed=true;
    if(fclose(file))failed=true;
    unsigned long long recording_id=0;
    int fields=sscanf(row,"DIVE_WATCH_LOG %u",&version);
    if(fields!=1)return -1;
    if(version==1)fields=sscanf(row,"DIVE_WATCH_LOG %u %llu %u %u %u %u %u %u %191s %c",
        &version,&candidate.id,&candidate.started_seconds,&candidate.completed_seconds,
        &candidate.dive_seconds,&low,&high,&automatic,candidate.policy,&extra);
    else if(version==2)fields=sscanf(row,"DIVE_WATCH_LOG %u %llu %llu %u %u %u %u %u %u %191s %c",
        &version,&candidate.id,&recording_id,&candidate.started_seconds,&candidate.completed_seconds,
        &candidate.dive_seconds,&low,&high,&automatic,candidate.policy,&extra);
    else return -1;
    if(failed || fields!=(version==1?9:10) || candidate.id!=id || (version==2 && !recording_id) ||
        !valid_pair(low,high) || automatic>1 ||
        candidate.completed_seconds<candidate.started_seconds ||
        candidate.dive_seconds>candidate.completed_seconds-candidate.started_seconds)return -1;
    candidate.recording_id=(uint64_t)recording_id;
    candidate.active_gf=(ostc4_gf_pair){(uint8_t)low,(uint8_t)high};
    candidate.automatic_completion=automatic!=0;
    if(record)*record=candidate;
    return 0;
}
static int compare_ids(const void *a,const void *b)
{
    unsigned long long x=*(const unsigned long long *)a,y=*(const unsigned long long *)b;
    return x<y?-1:x>y?1:0;
}
static void scan_logs(void)
{
    free(log_ids);log_ids=NULL;state.log_count=0;next_log_id=1;
    DIR *dir=opendir(directory);
    if(!dir){state.log_error="Log directory cannot be read";return;}
    struct dirent *entry;
    while((entry=readdir(dir))) {
        unsigned long long id;int end=0;
        if(sscanf(entry->d_name,"dive-%20llu.log%n",&id,&end)!=1 || !end || entry->d_name[end])continue;
        char canonical_name[64];snprintf(canonical_name,sizeof(canonical_name),"dive-%020llu.log",id);
        if(strcmp(canonical_name,entry->d_name))continue;
        if(id>=next_log_id)next_log_id=id==ULLONG_MAX?0:id+1;
        if(read_log_id(id,NULL)) {state.log_error="Invalid historical log preserved without inferred metadata";continue;}
        void *grown=realloc(log_ids,(state.log_count+1)*sizeof(*log_ids));
        if(!grown){state.log_error="Log index memory unavailable";break;}
        log_ids=grown;log_ids[state.log_count++]=id;
    }
    if(closedir(dir))state.log_error="Log directory read failed";
    if(state.log_count>1)qsort(log_ids,state.log_count,sizeof(*log_ids),compare_ids);
}
void pc_storage_append_log(const struct pc_bench_log *record)
{
    if(!state.enabled)return;
    char content[512],name[64];
    unsigned long long id=next_log_id;
    int length=snprintf(content,sizeof(content),"DIVE_WATCH_LOG 2 %llu %llu %u %u %u %u %u %u %s\n",
        id,(unsigned long long)record->recording_id,record->started_seconds,record->completed_seconds,record->dive_seconds,
        record->active_gf.low,record->active_gf.high,record->automatic_completion?1:0,record->policy);
    snprintf(name,sizeof(name),"dive-%020llu.log",id);
    if(!id || !record->recording_id || length<0 || length>=(int)sizeof(content) || atomic_write(name,content,true)) {
        ++state.unsaved_logs;state.log_error="Completed dive log write failed; log is NOT saved";return;
    }
    scan_logs();
}
int pc_storage_log_read(size_t index,struct pc_bench_log *record)
{
    if(!record || index>=state.log_count)return -1;
    return read_log_id(log_ids[index],record);
}

void pc_storage_reconcile_log(const struct pc_bench_log *record)
{
    if(!state.enabled)return;
    for(size_t i=0;i<state.log_count;++i) {
        struct pc_bench_log existing;
        if(pc_storage_log_read(i,&existing)==0 && existing.recording_id==record->recording_id &&
           existing.started_seconds==record->started_seconds && existing.completed_seconds==record->completed_seconds &&
           existing.dive_seconds==record->dive_seconds && existing.active_gf.low==record->active_gf.low &&
           existing.active_gf.high==record->active_gf.high && existing.automatic_completion==record->automatic_completion &&
           !strcmp(existing.policy,record->policy))return;
    }
    pc_storage_append_log(record);
}
