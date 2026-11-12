/* SPDX-License-Identifier: GPL-3.0-only */
/* MustardTube Beta 2.2.2 -- optional independent GAME mix.
 * No PipeWire libraries, no resident service, no emulator-name allowlist.
 * Only one unambiguous non-radio AUDIO OUTPUT stream may be adjusted.
 * Stream and process lifetime are checked before EVERY volume write.
 */
#define _GNU_SOURCE
#include "game_mixer.h"
#include <errno.h>
#include <dirent.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define OUT_MAX 32768
#define WPCTL_TIMEOUT_MS 1600

typedef struct {
    int node;
    pid_t pid;
    unsigned long long started;
    double original;
    /* Identity of the executable captured BEFORE the original game exits.
     * This is never a pathname or process name supplied by a user. */
    unsigned long long exe_dev,exe_ino;
} GameTarget;
/* A small bounded set of deferred resets prevents a previous emulator from
 * blocking the next game. Every slot retains the exact process fingerprint. */
#define MT_MAX_DEFERRED 8
static GameTarget last_target;
static bool last_valid=false;

/* Never use a shell; never leak a stream URL into diagnostics. */
static int wp_run(const char *a,const char *b,const char *c,char *out,size_t cap){
    int pipes[2];if(pipe(pipes)<0)return -1;
    pid_t child=fork();
    if(child<0){close(pipes[0]);close(pipes[1]);return -1;}
    if(child==0){
        close(pipes[0]);dup2(pipes[1],STDOUT_FILENO);
        int nul=open("/dev/null",O_WRONLY);
        if(nul>=0){dup2(nul,STDERR_FILENO);close(nul);}
        close(pipes[1]);
        setenv("XDG_RUNTIME_DIR","/run",1);
        setenv("PIPEWIRE_RUNTIME_DIR","/run",1);
        char *argv[]={"wpctl",(char*)a,(char*)b,(char*)c,NULL};
        if(!b)argv[2]=NULL;
        else if(!c)argv[3]=NULL;
        execvp("wpctl",argv);_exit(127);
    }
    close(pipes[1]);
    int flags=fcntl(pipes[0],F_GETFL);if(flags>=0)fcntl(pipes[0],F_SETFL,flags|O_NONBLOCK);
    size_t used=0;if(out&&cap)out[0]=0;
    struct timespec t0,t;clock_gettime(CLOCK_MONOTONIC,&t0);
    bool ended=false;int rc=-1,status=0;
    while(!ended){
        clock_gettime(CLOCK_MONOTONIC,&t);
        long elapsed=(t.tv_sec-t0.tv_sec)*1000L+(t.tv_nsec-t0.tv_nsec)/1000000L;
        if(elapsed>=WPCTL_TIMEOUT_MS)break;
        struct pollfd pfd={.fd=pipes[0],.events=POLLIN|POLLHUP};
        int p=poll(&pfd,1,WPCTL_TIMEOUT_MS-(int)elapsed);
        if(p<0&&errno==EINTR)continue;
        if(p<0)break;
        if(p==0)break;
        if(pfd.revents&(POLLIN|POLLHUP)){
            char buf[2048];ssize_t n;
            while((n=read(pipes[0],buf,sizeof(buf)))>0){
                if(out&&cap>1&&used<cap-1){
                    size_t copy=(size_t)n;if(copy>cap-1-used)copy=cap-1-used;
                    memcpy(out+used,buf,copy);used+=copy;out[used]=0;
                }
            }
            if(n==0){ended=true;break;}
            if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK)break;
        }
    }
    close(pipes[0]);
    if(!ended)kill(child,SIGKILL);
    while(waitpid(child,&status,0)<0&&errno==EINTR){}
    if(ended&&WIFEXITED(status)&&WEXITSTATUS(status)==0)rc=0;
    return rc;
}
static bool start_ticks(pid_t pid,unsigned long long *ticks){
    if(pid<=1)return false;
    char name[80],line[4096];snprintf(name,sizeof(name),"/proc/%ld/stat",(long)pid);
    FILE*f=fopen(name,"r");if(!f)return false;
    bool valid=fgets(line,sizeof(line),f)!=NULL;fclose(f);if(!valid)return false;
    char*r=strrchr(line,')');if(!r||r[1]!=' ')return false;
    int i=0;for(char *s=strtok(r+2," \n");s;s=strtok(NULL," \n"),i++){
        if(i==0&&(s[0]=='Z'||s[0]=='X'))return false;
        if(i==19){char*end;unsigned long long val=strtoull(s,&end,10);
            if(end==s||*end||!val)return false;
            *ticks=val;return true;
        }
    }
    return false;
}
static bool digits(const char*s);
static bool exe_identity(pid_t pid,unsigned long long *dev,unsigned long long *ino){
    char path[80];struct stat st;
    if(pid<=1)return false;
    snprintf(path,sizeof(path),"/proc/%ld/exe",(long)pid);
    if(stat(path,&st)<0 || st.st_ino==0)return false;
    *dev=(unsigned long long)st.st_dev;
    *ino=(unsigned long long)st.st_ino;
    return true;
}
static bool exe_matches(pid_t pid,const GameTarget *g){
    unsigned long long dev=0,ino=0;
    return g->exe_ino && exe_identity(pid,&dev,&ino) &&
           dev==g->exe_dev && ino==g->exe_ino;
}
/* /proc-only existence check. No PipeWire query is spawned until
 * at least one process with the same executable exists. Multiple processes
 * are allowed; a UNIQUE compatible audio stream is required below. */
static bool reopened_executable_exists(const GameTarget *g){
    DIR *d=opendir("/proc");if(!d)return false;
    struct dirent *entry;bool found=false;
    while((entry=readdir(d))){
        if(!digits(entry->d_name))continue;
        long n=strtol(entry->d_name,NULL,10);
        if(n<=1||n>INT_MAX)continue;
        pid_t pid=(pid_t)n;
        unsigned long long ticks=0;
        if(!start_ticks(pid,&ticks) || (pid==g->pid&&ticks==g->started))continue;
        if(exe_matches(pid,g)){found=true;break;}
    }
    closedir(d);
    return found;
}
static bool digits(const char*s){
    if(!s||!*s)return false;
    for(;*s;s++)if(*s<'0'||*s>'9')return false;
    return true;
}
static int prop_int(const char*inspect,const char*key){
    char pat[96];snprintf(pat,sizeof(pat),"%s =",key);
    const char*p=inspect;
    while((p=strstr(p,pat))){
        if(p!=inspect && p[-1]!=' ' && p[-1]!='\t' && p[-1]!='\n'){p++;continue;}
        p+=strlen(pat);while(*p==' '||*p=='\t'||*p=='\"')p++;
        char num[24];size_t i=0;
        while(i<sizeof(num)-1 && *p>='0'&&*p<='9')num[i++]=*p++;
        num[i]=0;
        if(digits(num)){
            long val=strtol(num,NULL,10);
            if(val>1&&val<INT_MAX)return (int)val;
        }
    }
    return -1;
}
/* Inspect node ID, then (if needed) its owning PipeWire Client ID. */
static pid_t node_pid(int node){
    char id[32],info[OUT_MAX];snprintf(id,sizeof(id),"%d",node);
    if(wp_run("inspect",id,NULL,info,sizeof(info))!=0)return -1;
    /* If class is reported, require audio output; never modify capture. */
    if(strstr(info,"media.class") &&
       !strstr(info,"Stream/Output/Audio"))return -1;
    int p=prop_int(info,"application.process.id");
    if(p>1)return (pid_t)p;
    int client=prop_int(info,"client.id");
    if(client<2)return -1;
    snprintf(id,sizeof(id),"%d",client);
    if(wp_run("inspect",id,NULL,info,sizeof(info))!=0)return -1;
    p=prop_int(info,"application.process.id");
    return p>1?(pid_t)p:-1;
}
static bool valid_target(GameTarget *g){
    unsigned long long current=0;
    return g->node>1 && g->pid>1 && g->started>0 &&
           start_ticks(g->pid,&current) && current==g->started &&
           node_pid(g->node)==g->pid;
}
static bool get_volume(int node,double *v){
    char id[32],out[512];snprintf(id,sizeof(id),"%d",node);
    if(wp_run("get-volume",id,NULL,out,sizeof(out))!=0)return false;
    char *s=strstr(out,"Volume:");if(!s)return false;
    char *end;double n=strtod(s+7,&end);
    if(end==s+7||n<0.0||n>4.0)return false;
    *v=n;return true;
}
static bool set_volume(int node,double volume){
    if(node<2||volume<0.0||volume>4.0)return false;
    char id[32],v[32];snprintf(id,sizeof(id),"%d",node);
    snprintf(v,sizeof(v),"%.4f",volume);
    return wp_run("set-volume",id,v,NULL,0)==0;
}
static bool session_path(const char*app,char*out,size_t sz,const char*leaf){
    if(!app||app[0]!='/'||strlen(app)>PATH_MAX-100)return false;
    return snprintf(out,sz,"%s/data/%s",app,leaf)<(int)sz;
}
/* A simple 0-100 game-mix setting, distinct from the radio mix and
 * from muOS/PipeWire master volume. It is persisted WITHOUT stream IDs. */
int mt_game_mix_level(const char *app){
    char path[PATH_MAX],buf[20]={0};
    if(!session_path(app,path,sizeof(path),"game-mix-volume.txt"))return 100;
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return 100;
    ssize_t n=read(fd,buf,sizeof(buf)-1);close(fd);
    if(n<=0)return 100;
    char *end=NULL;errno=0;long val=strtol(buf,&end,10);
    if(errno||end==buf||(end[0]!='\n'&&end[0]!='\0')||
       (end[0]=='\n'&&end[1]!='\0')||val<0||val>100)return 100;
    return (int)val;
}
static bool write_game_mix(const char*app,int level){
    if(level<0||level>100)return false;
    char path[PATH_MAX],tmp[PATH_MAX],text[8];
    if(!session_path(app,path,sizeof(path),"game-mix-volume.txt") ||
       snprintf(tmp,sizeof(tmp),"%s.tmp-%ld",path,(long)getpid())>=(int)sizeof(tmp))return false;
    int fd=open(tmp,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)return false;
    int n=snprintf(text,sizeof(text),"%d\n",level);
    bool ok=n>0&&n<(int)sizeof(text)&&write(fd,text,(size_t)n)==n;
    if(ok)ok=fsync(fd)==0;
    close(fd);
    if(ok)ok=rename(tmp,path)==0;
    if(!ok)unlink(tmp);
    return ok;
}
static int lock_session(const char*app){
    char path[PATH_MAX];if(!session_path(app,path,sizeof(path),"game-volume.lock"))return -1;
    int fd=open(path,O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)return -1;
    if(flock(fd,LOCK_EX)<0){close(fd);return -1;}
    return fd;
}
static bool read_record(const char*app,const char*leaf,GameTarget*g){
    char path[PATH_MAX];if(!session_path(app,path,sizeof(path),leaf))return false;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return false;
    char buf[192]={0};ssize_t n=read(fd,buf,sizeof(buf)-1);close(fd);
    if(n<=0)return false;
    long p=0;unsigned long long st=0,dev=0,ino=0;
    double original=0;int node=0;char tail=0;
    int fields=sscanf(buf,"%d %ld %llu %lf %llu %llu %c",
                      &node,&p,&st,&original,&dev,&ino,&tail);
    /* Legacy sessions are readable for same-process restoration, but never
     * eligible for deferred cross-process restoration without exe identity. */
    if((fields!=4 && fields!=6) || node<=1||p<=1||p>INT_MAX||!st||
       original<0||original>4 || (fields==6&&(!dev||!ino)))return false;
    *g=(GameTarget){.node=node,.pid=(pid_t)p,.started=st,
                    .original=original,.exe_dev=dev,.exe_ino=ino};
    return true;
}
static bool read_session(const char*app,GameTarget*g){return read_record(app,"game-volume.session",g);}
static void clear_record(const char*app,const char*leaf){
    char path[PATH_MAX];if(session_path(app,path,sizeof(path),leaf))unlink(path);
}
static void clear_session(const char*app){clear_record(app,"game-volume.session");}
static void deferred_leaf(int i,char *buf,size_t cap){
    snprintf(buf,cap,"game-volume.deferred-%d",i);
}
/* Called under the game-volume lock. Old record is retained by atomic rename
 * and never overwritten. No extra service or global audio setting. */
static bool archive_session(const char*app){
    char from[PATH_MAX],to[PATH_MAX],leaf[64];
    if(!session_path(app,from,sizeof(from),"game-volume.session"))return false;
    for(int i=0;i<MT_MAX_DEFERRED;i++){
        deferred_leaf(i,leaf,sizeof(leaf));
        if(!session_path(app,to,sizeof(to),leaf))return false;
        struct stat st;
        if(lstat(to,&st)==0)continue; /* existing file or symlink */
        if(errno!=ENOENT)return false;
        return rename(from,to)==0;
    }
    return false; /* Full: fail closed rather than lose a restoration record. */
}
static bool write_session(const char*app,GameTarget*g){
    char path[PATH_MAX],tmp[PATH_MAX];
    if(!session_path(app,path,sizeof(path),"game-volume.session"))return false;
    if(snprintf(tmp,sizeof(tmp),"%s.tmp-%ld",path,(long)getpid())>=(int)sizeof(tmp))return false;
    int fd=open(tmp,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)return false;
    char body[180];int n=snprintf(body,sizeof(body),
         "%d %ld %llu %.4f %llu %llu\n",g->node,(long)g->pid,
         g->started,g->original,g->exe_dev,g->exe_ino);
    bool ok=n>0 && n<(int)sizeof(body) && write(fd,body,(size_t)n)==n;
    if(ok)ok=fsync(fd)==0;
    close(fd);
    if(ok)ok=rename(tmp,path)==0;
    if(!ok)unlink(tmp);
    return ok;
}
/* A failing PipeWire query is NOT proof the game has gone away.  Only the
 * /proc start-time check can safely establish that the old process ended.
 * Keep the recovery record for a transient lookup/mixer failure. */
static bool same_game_process(const GameTarget*g){
    unsigned long long current=0;
    return g->pid>1 && start_ticks(g->pid,&current) && current==g->started;
}
static bool restore_target(GameTarget*g){
    if(!same_game_process(g))return false; /* Gone: defer, never target other node. */
    for(int attempt=0;attempt<2;attempt++){
        if(node_pid(g->node)==g->pid && set_volume(g->node,1.0)){
            double verified=0;
            if(node_pid(g->node)==g->pid && get_volume(g->node,&verified) &&
               verified>=0.985 && verified<=1.015)return true;
        }
        if(attempt==0)usleep(100000);
    }
    return false; /* Target still alive: keep original level for retry. */
}
static void forget_memory(void){last_valid=false;memset(&last_target,0,sizeof(last_target));}
static int restore_reopened_locked(const char *app,const GameTarget *old,const char*leaf);
/* Restore managed game output to 100%, independent of prior level.
 * Return 0 if restored/empty, 2 if ORIGINAL process exited and the
 * fingerprinted recovery record was retained, -1 for temporary failures. */
int mt_game_restore(const char*app){
    int lock=lock_session(app);
    if(lock<0){
        bool ok=!last_valid||restore_target(&last_target);
        forget_memory();return ok?0:-1;
    }
    GameTarget old={0};
    bool found=read_session(app,&old);
    if(!found&&last_valid){old=last_target;found=true;}
    int result=0;
    if(found){
        if(same_game_process(&old)){
            if(restore_target(&old))clear_session(app);
            else if(old.exe_dev && old.exe_ino){
                /* The process can remain alive while recreating its PipeWire
                 * stream. Try exactly that executable, else retain the record
                 * for the bounded post-stop recovery watcher. */
                result=restore_reopened_locked(app,&old,"game-volume.session");
            }else result=-1;
        }else if(old.exe_dev && old.exe_ino){
            /* No stream exists yet: retain the exact original level and
             * executable fingerprint. Never claim a restoration occurred. */
            result=2;
        }else{
            result=-1; /* Legacy record: not safe to identify a new process. */
        }
    }
    /* Also reset earlier managed games that exited while radio was active. */
    for(int i=0;i<MT_MAX_DEFERRED;i++){
        char leaf[64];GameTarget pending={0};
        deferred_leaf(i,leaf,sizeof(leaf));
        if(!read_record(app,leaf,&pending))continue;
        int r=2;
        if(same_game_process(&pending) && restore_target(&pending)){
            clear_record(app,leaf);r=0;
        }else if(pending.exe_dev && pending.exe_ino){
            r=restore_reopened_locked(app,&pending,leaf);
        }
        if(r && result==0)result=r;
    }
    flock(lock,LOCK_UN);close(lock);
    forget_memory();return result;
}
/* Strictly re-identify ONE live stream for ONE reopened executable.
 * Never touch an audio sink, radio, a PID-recycled process, or a different
 * emulator.  Missing/ambiguous streams leave the record for another try. */
static int restore_reopened_locked(const char *app,const GameTarget *old,const char*leaf){
    if(!old->exe_dev || !old->exe_ino)return 2;
    bool original_alive=same_game_process(old);
    if(original_alive){
        /* Only recover a new node if the old node is genuinely gone; do not
         * override an active original stream after a transient wpctl error. */
        if(node_pid(old->node)==old->pid)return 2;
    }else if(!reopened_executable_exists(old))return 2;
    char text[OUT_MAX];
    if(wp_run("status",NULL,NULL,text,sizeof(text))!=0)return 2;
    char *copy=strdup(text);if(!copy)return 2;
    bool audio=false,streams=false;int found=0,node=-1;pid_t reopened=-1;
    unsigned long long started=0;
    for(char *save=NULL,*line=strtok_r(copy,"\n",&save);line;
        line=strtok_r(NULL,"\n",&save)){
        char *head=line;while(*head==' '||*head=='\t')head++;
        if(strcmp(head,"Audio")==0){audio=true;streams=false;continue;}
        if(strcmp(head,"Video")==0){audio=false;streams=false;continue;}
        if(audio && strstr(line,"Streams:")){streams=true;continue;}
        if(!streams)continue;
        const char *p=line;
        while(*p && !(*p>='0'&&*p<='9'))p++;
        if(!*p)continue;
        const char *start=p;while(*p>='0'&&*p<='9')p++;
        if(*p!='.'||p[1]!=' ')continue;
        if((int)(start-line)>12&&strstr(line,"output_"))continue;
        long id=strtol(start,NULL,10);
        if(id<=1||id>1000000)continue;
        pid_t pid=node_pid((int)id);
        unsigned long long ticks=0;
        if(pid<=1 || !start_ticks(pid,&ticks) || !exe_matches(pid,old))continue;
        bool original_process=(pid==old->pid && ticks==old->started);
        if(original_alive ? (original_process && (int)id!=old->node)
                          : !original_process){
            reopened=pid;started=ticks;node=(int)id;
            if(++found>1)break;
        }
    }
    free(copy);
    if(found!=1)return 2;
    unsigned long long check=0;
    if(!start_ticks(reopened,&check)||check!=started||!exe_matches(reopened,old)||
       node_pid(node)!=reopened)return 2;
    if(!set_volume(node,1.0))return 2;
    /* A successful command alone is not proof that the stream survived long
     * enough to receive the volume. Keep recovery data if verification fails. */
    double verified=0;
    if(!start_ticks(reopened,&check)||check!=started||
       node_pid(node)!=reopened||!exe_matches(reopened,old)||
       !get_volume(node,&verified))return 2;
    double diff=verified-1.0;
    if(diff<0)diff=-diff;
    if(diff>0.015)return 2;
    clear_record(app,leaf);
    return 0;
}
int mt_game_recover_once(const char *app){
    int lock=lock_session(app);if(lock<0)return -1;
    GameTarget old={0};int result=0;
    if(read_session(app,&old)){
        if(same_game_process(&old)){
            if(restore_target(&old)){clear_session(app);result=0;}
            else result=restore_reopened_locked(app,&old,"game-volume.session");
        }else result=restore_reopened_locked(app,&old,"game-volume.session");
    }
    for(int i=0;i<MT_MAX_DEFERRED;i++){
        char leaf[64];GameTarget pending={0};
        deferred_leaf(i,leaf,sizeof(leaf));
        if(!read_record(app,leaf,&pending))continue;
        int r=2;
        if(same_game_process(&pending) && restore_target(&pending)){
            clear_record(app,leaf);r=0;
        }else if(pending.exe_dev && pending.exe_ino){
            r=restore_reopened_locked(app,&pending,leaf);
        }
        if(r && result==0)result=r;
    }
    flock(lock,LOCK_UN);close(lock);
    return result;
}
/* 180-second recovery window, process sleeps two seconds between checks.
 * This mode is launched ONLY if a game exited before radio was stopped;
 * it terminates on success, uninstall, signal, or timeout. */
int mt_game_recover_bounded(const char *app,int (*cancelled)(void)){
    char path[PATH_MAX];
    if(!session_path(app,path,sizeof(path),"game-volume-recover.lock"))return -1;
    int fd=open(path,O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)return -1;
    if(flock(fd,LOCK_EX|LOCK_NB)<0){close(fd);return 0;}
    struct timespec start,now;
    clock_gettime(CLOCK_MONOTONIC,&start);
    int result=2;
    for(;;){
        if((cancelled&&cancelled())||access(app,F_OK)!=0)break;
        result=mt_game_recover_once(app);
        if(result!=2)break;
        clock_gettime(CLOCK_MONOTONIC,&now);
        long elapsed=(now.tv_sec-start.tv_sec)*1000L+
                     (now.tv_nsec-start.tv_nsec)/1000000L;
        if(elapsed>=180000)break;
        struct timespec nap={.tv_sec=2,.tv_nsec=0};
        while(nanosleep(&nap,&nap)<0&&errno==EINTR){
            if(cancelled&&cancelled())break;
        }
    }
    flock(fd,LOCK_UN);close(fd);
    return result;
}
/* Parse ONLY the Audio > Streams section, rejecting nested port lines.
 * This is intentionally conservative: >1 plausible nonradio stream => no-op. */
static int discover_target(pid_t radio,GameTarget*out){
    char text[OUT_MAX];if(wp_run("status",NULL,NULL,text,sizeof(text))!=0)return -1;
    char *copy=strdup(text);if(!copy)return -1;
    bool audio=false,streams=false;int found=0;GameTarget candidate={0};
    for(char*save=NULL,*line=strtok_r(copy,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
        /* Headings are standalone; do not mistake an emulator named
         * "Audio" for the start of another section. */
        char *head=line;while(*head==' '||*head=='\t')head++;
        if(strcmp(head,"Audio")==0){audio=true;streams=false;continue;}
        if(strcmp(head,"Video")==0){audio=false;streams=false;continue;}
        if(audio && strstr(line,"Streams:")){streams=true;continue;}
        if(!streams)continue;
        const char*p=line;while(*p && !(*p>='0'&&*p<='9'))p++;
        if(!*p)continue;
        const char*start=p;while(*p>='0'&&*p<='9')p++;
        if(*p!='.'||p[1]!=' ')continue;
        /* Ignore port links indented beneath stream node records. */
        if((int)(start-line)>12 && strstr(line,"output_"))continue;
        long node=strtol(start,NULL,10);if(node<2||node>1000000)continue;
        pid_t pid=node_pid((int)node);
        if(pid<=1 || pid==radio || pid==getpid())continue;
        unsigned long long started=0;
        if(!start_ticks(pid,&started))continue;
        double orig=0;
        if(!get_volume((int)node,&orig))continue;
        unsigned long long dev=0,ino=0;
        if(!exe_identity(pid,&dev,&ino))continue;
        candidate=(GameTarget){.node=(int)node,.pid=pid,.started=started,
                               .original=orig,.exe_dev=dev,.exe_ino=ino};
        if(++found>1)break;
    }
    free(copy);
    if(found!=1)return found==0?-2:-3;
    *out=candidate;return 0;
}
/* Read-only developer diagnostic. No URLs, device sink IDs or changes. */
int mt_game_session_status(const char*app){
    for(int i=0;i<MT_MAX_DEFERRED;i++){
        char leaf[64];GameTarget pending={0};
        deferred_leaf(i,leaf,sizeof(leaf));
        if(read_record(app,leaf,&pending))
            printf("game_deferred_reset=slot%d pid=%ld target=1.00\n",i,(long)pending.pid);
    }
    GameTarget g={0};
    if(!read_session(app,&g)){puts("game_restore_state=none");return 0;}
    if(!same_game_process(&g)){
        if(g.exe_dev && g.exe_ino)
            printf("game_restore_state=awaiting_reopen pid=%ld original=%.2f\n",
                   (long)g.pid,g.original);
        else puts("game_restore_state=legacy_exited_no_identity");
        return 0;
    }
    if(node_pid(g.node)!=g.pid){
        printf("game_restore_state=pending_stream_unverified node=%d pid=%ld original=%.2f\n",
               g.node,(long)g.pid,g.original);
        return 0;
    }
    double current=0;
    if(!get_volume(g.node,&current)){
        printf("game_restore_state=pending_volume_unavailable node=%d pid=%ld original=%.2f\n",
               g.node,(long)g.pid,g.original);
        return 0;
    }
    printf("game_restore_state=active node=%d pid=%ld current=%.2f original=%.2f\n",
           g.node,(long)g.pid,current,g.original);
    return 0;
}
int mt_game_diagnostic(const char*app,pid_t radio_pid){
    (void)app;
    GameTarget g={0};int r=discover_target(radio_pid,&g);
    if(r==0)printf("game_stream=unique node=%d pid=%ld\n",g.node,(long)g.pid);
    else printf("game_stream=%s\n",r==-3?"ambiguous":"unavailable");
    return 0;
}
/* Set the selected mix level. When the user is on Now Playing, no game
 * may be running: save the preference for the next uniquely identified game.
 * If a verified session is active, update that stream immediately. */
int mt_game_mix_set(const char*app,int percentage){
    if(percentage<0||percentage>100)return -1;
    int lock=lock_session(app);if(lock<0)return -1;
    if(!write_game_mix(app,percentage)){
        flock(lock,LOCK_UN);close(lock);return -1;
    }
    GameTarget state={0};int result=0;
    if(read_session(app,&state) && valid_target(&state)){
        if(!set_volume(state.node,(double)percentage/100.0))result=2;
    }
    flock(lock,LOCK_UN);close(lock);
    return result;
}
/* Called under lock when the previous game has exited, but the newly
 * discovered stream belongs to a different executable. Preserve the old
 * record in a deferred slot and admit the verified new game. Do NOT do this
 * for a live original game or the same executable when recovery fails. */
static bool make_room_for_new_game(const char*app,const GameTarget*old,const GameTarget*fresh){
    if(same_game_process(old))return false;
    if(old->exe_dev==fresh->exe_dev && old->exe_ino==fresh->exe_ino)return false;
    return archive_session(app);
}
/* If the deferred emulator has reappeared, reset that exact stream to 100%
 * before applying the current radio mix. Never adjust an unrelated stream. */
static bool recover_matching_deferred(const char*app,const GameTarget*fresh){
    for(int i=0;i<MT_MAX_DEFERRED;i++){
        char leaf[64];GameTarget pending={0};
        deferred_leaf(i,leaf,sizeof(leaf));
        if(!read_record(app,leaf,&pending))continue;
        if(pending.exe_dev!=fresh->exe_dev || pending.exe_ino!=fresh->exe_ino)continue;
        if(restore_reopened_locked(app,&pending,leaf)!=0)return false;
    }
    return true;
}
/* Apply the saved game mix ONLY once to a new, unambiguous active game
 * stream. No scan when an existing session still belongs to a live game.
 * Never replace a pending restoration record with an unrelated emulator. */
int mt_game_mix_sync(const char*app,pid_t radio_pid){
    int lock=lock_session(app);if(lock<0)return -1;
    GameTarget state={0};bool found=read_session(app,&state);
    /* Cheap /proc-only fast path for the same running game. */
    if(found && same_game_process(&state)){
        flock(lock,LOCK_UN);close(lock);return 0;
    }
    GameTarget fresh={0};
    int discovery=discover_target(radio_pid,&fresh);
    if(found){
        if(restore_reopened_locked(app,&state,"game-volume.session")==0){
            found=false;
        }else if(discovery==0 && make_room_for_new_game(app,&state,&fresh)){
            found=false;
        }else{
            flock(lock,LOCK_UN);close(lock);return 2;
        }
    }
    if(discovery!=0){flock(lock,LOCK_UN);close(lock);return 1;}
    if(!recover_matching_deferred(app,&fresh)){
        flock(lock,LOCK_UN);close(lock);return 2;
    }
    if(!write_session(app,&fresh)){
        flock(lock,LOCK_UN);close(lock);return -1;
    }
    int desired=mt_game_mix_level(app);
    double current=0,target=(double)desired/100.0;
    bool ok=valid_target(&fresh)&&get_volume(fresh.node,&current);
    if(ok && (current-target>0.005 || target-current>0.005))
        ok=valid_target(&fresh)&&set_volume(fresh.node,target);
    flock(lock,LOCK_UN);close(lock);
    return ok?0:-1;
}
int mt_game_adjust(const char*app,pid_t radio_pid,int delta){
    if(delta!=5&&delta!=-5)return -1;
    int lock=lock_session(app);if(lock<0)return -1;
    GameTarget state={0};bool found=read_session(app,&state);
    if(found && !valid_target(&state)){
        if(same_game_process(&state)){
            flock(lock,LOCK_UN);close(lock);return -1;
        }
        GameTarget fresh={0};
        int discovery=discover_target(radio_pid,&fresh);
        if(!state.exe_dev || !state.exe_ino){
            /* Legacy session: cannot identify it after exit; do not block. */
            clear_session(app);found=false;forget_memory();
        }else if(restore_reopened_locked(app,&state,"game-volume.session")==0){
            found=false;forget_memory();
        }else if(discovery==0 && make_room_for_new_game(app,&state,&fresh)){
            found=false;forget_memory();
        }else{
            flock(lock,LOCK_UN);close(lock);return -1;
        }
    }
    if(!found){
        int r=discover_target(radio_pid,&state);
        if(r!=0){flock(lock,LOCK_UN);close(lock);return r;}
        if(!recover_matching_deferred(app,&state)){
            flock(lock,LOCK_UN);close(lock);return -1;
        }
        if(!write_session(app,&state)){
            flock(lock,LOCK_UN);close(lock);return -1;
        }
        double target=(double)mt_game_mix_level(app)/100.0,existing=0;
        if(!valid_target(&state) || !get_volume(state.node,&existing) ||
           ((existing-target>0.005 || target-existing>0.005) &&
            !set_volume(state.node,target))){
            flock(lock,LOCK_UN);close(lock);return -1;
        }
    }
    last_target=state;last_valid=true;
    double current=0;
    if(!valid_target(&state)||!get_volume(state.node,&current)){
        flock(lock,LOCK_UN);close(lock);return -1;
    }
    double next=current+(double)delta/100.;
    if(next<0)next=0;
    if(next>1.0)next=1.0;
    if(next==current){
        /* Saturated at 0% or 100%. No write; tell the hold-repeat helper
         * to stop trying until the user presses again. */
        flock(lock,LOCK_UN);close(lock);return 1;
    }
    bool ok=valid_target(&state)&&set_volume(state.node,next);
    if(ok)ok=write_game_mix(app,(int)(next*100.0+0.5));
    flock(lock,LOCK_UN);close(lock);
    return ok?0:-1;
}
