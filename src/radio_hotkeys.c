/* SPDX-License-Identifier: GPL-3.0-only */
/* MustardTube optional in-game radio hotkeys for muOS Andromeda.
 * Opens evdev read-only, never uses EVIOCGRAB/uinput, and never edits OS hotkeys.
 * Runs only while a background radio process is alive; closes on deletion.
 * GPL-friendly standalone Linux input adapter; no SDL/PortMaster dependency.
 */
#define _GNU_SOURCE
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "game_mixer.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define MAX_KEYCODE (KEY_MAX + 1)
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define NBITS(x) (((x) + BITS_PER_LONG - 1) / BITS_PER_LONG)
static volatile sig_atomic_t quit = 0;
static void on_signal(int sig){ (void)sig; quit=1; }
static int interrupted(void){return quit!=0;}
static int bit_test(const unsigned long *p, int bit){ return (int)((p[bit/BITS_PER_LONG] >> (bit%BITS_PER_LONG)) & 1UL); }
static bool ability(int fd,unsigned event,int code){
    unsigned long mask[NBITS(KEY_MAX+1)];
    memset(mask,0,sizeof(mask));
    if(ioctl(fd,EVIOCGBIT(event,sizeof(mask)),mask)<0)return false;
    return bit_test(mask,code)!=0;
}
static bool suitable(int fd){
    bool shoulders = ability(fd,EV_KEY,BTN_TL2) && ability(fd,EV_KEY,BTN_TR2);
    bool dpad = ability(fd,EV_ABS,ABS_HAT0X) ||
       (ability(fd,EV_KEY,KEY_LEFT) && ability(fd,EV_KEY,KEY_RIGHT)) ||
       (ability(fd,EV_KEY,BTN_DPAD_LEFT) && ability(fd,EV_KEY,BTN_DPAD_RIGHT));
    return shoulders && dpad;
}
static int scan_gamepad(char*display,size_t length){
    DIR *dir=opendir("/dev/input");
    if(!dir)return -1;
    int chosen=-1,priority=-1;
    struct dirent *entry;
    while((entry=readdir(dir))){
        if(strncmp(entry->d_name,"event",5)!=0)continue;
        char file[PATH_MAX],name[256]={0};
        if(snprintf(file,sizeof(file),"/dev/input/%s",entry->d_name)>=(int)sizeof(file))continue;
        int fd=open(file,O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOCTTY);
        if(fd<0)continue;
        (void)ioctl(fd,EVIOCGNAME(sizeof(name)),name);
        int p=strstr(name,"muOS-Keys")?10:strstr(name,"muOS")?9:1;
        if(suitable(fd) && p>priority){
            if(chosen>=0)close(chosen);
            chosen=fd;priority=p;
            if(display)snprintf(display,length,"%s (%s)",file,name);
        }else close(fd);
    }
    closedir(dir);
    return chosen;
}
static int do_list(void){
    DIR *d=opendir("/dev/input");
    if(!d){perror("/dev/input");return 1;}
    struct dirent *e;
    while((e=readdir(d))){
        if(strncmp(e->d_name,"event",5))continue;
        char p[PATH_MAX],name[256]={0};
        snprintf(p,sizeof(p),"/dev/input/%s",e->d_name);
        int fd=open(p,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(fd<0)continue;
        (void)ioctl(fd,EVIOCGNAME(sizeof(name)),name);
        printf("%s %-32.32s shoulders=%d/%d dpad-hat=%d dpad-keys=%d/%d compatible=%s\n",
               p,name,ability(fd,EV_KEY,BTN_TL2),ability(fd,EV_KEY,BTN_TR2),
               ability(fd,EV_ABS,ABS_HAT0X),ability(fd,EV_KEY,KEY_LEFT),
               ability(fd,EV_KEY,KEY_RIGHT),suitable(fd)?"yes":"no");
        close(fd);
    }
    closedir(d);
    return 0;
}
static int do_probe(const char *device){
    if(strncmp(device,"/dev/input/event",16)!=0 || strchr(device+16,'/') || !device[16])return 2;
    int fd=open(device,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0){perror("probe open");return 2;}
    puts("Read-only button probe (15 seconds). Press L2, R2, LEFT and RIGHT separately. No events are stored.");
    fflush(stdout);
    struct timespec start,now;clock_gettime(CLOCK_MONOTONIC,&start);
    while(!quit){
        struct pollfd pfd={.fd=fd,.events=POLLIN};
        int rc=poll(&pfd,1,500);
        if(rc>0 && (pfd.revents&POLLIN)){
            struct input_event ev;
            while(read(fd,&ev,sizeof(ev))==(ssize_t)sizeof(ev)){
                if(ev.type==EV_KEY && (ev.value==0||ev.value==1))
                    printf("KEY code=%u %s\n",ev.code,ev.value?"down":"up");
                else if(ev.type==EV_ABS && (ev.code==ABS_HAT0X||ev.code==ABS_HAT0Y))
                    printf("HAT code=%u value=%d\n",ev.code,ev.value);
                fflush(stdout);
            }
        }
        clock_gettime(CLOCK_MONOTONIC,&now);
        if(now.tv_sec-start.tv_sec>=15)break;
    }
    close(fd);return 0;
}
/* Proc stat field 22 (start ticks), plus field 5 (process group). */
static bool proc_valid(pid_t pid,unsigned long long expect){
    if(pid<=1)return false;
    char path[80],raw[4096];
    snprintf(path,sizeof(path),"/proc/%ld/stat",(long)pid);
    FILE*f=fopen(path,"r");if(!f)return false;
    bool ok=fgets(raw,sizeof(raw),f)!=NULL;fclose(f);if(!ok)return false;
    char*r=strrchr(raw,')');if(!r||!r[1])return false;
    int index=0;pid_t grp=-1;unsigned long long started=0;
    for(char*token=strtok(r+2," \n");token;token=strtok(NULL," \n"),index++){
        if(index==0&&(token[0]=='Z'||token[0]=='X'))return false;
        if(index==2)grp=(pid_t)strtol(token,NULL,10);
        if(index==19){started=strtoull(token,NULL,10);break;}
    }
    return grp==pid && started==expect && started!=0;
}
static int preferences(const char*app){
    char file[PATH_MAX],buf[8]={0};
    if(snprintf(file,sizeof(file),"%s/data/radio-hotkeys-enabled.txt",app)>=(int)sizeof(file))return 0;
    int fd=open(file,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return 0;
    ssize_t n=read(fd,buf,sizeof(buf)-1);
    close(fd);
    return n>0 && (buf[0]=='1'||buf[0]=='2') && (buf[1]=='\n'||buf[1]=='\0') ? (buf[0]-'0') : 0;
}
static void do_volume(const char*app,int delta){
    char node[PATH_MAX],ctl[PATH_MAX];
    if(snprintf(node,sizeof(node),"%s/bin/node",app)>=(int)sizeof(node)||
       snprintf(ctl,sizeof(ctl),"%s/scripts/background-radio.js",app)>=(int)sizeof(ctl))return;
    pid_t child=fork();
    if(child==0){
        char*args[]={node,ctl,"adjust-volume",delta>0?"5":"-5",NULL};
        /* The radio control knows its exact ffplay PID and never touches game audio. */
        setenv("MUSTARDTUBE_APP_DIR",app,1);
        char data[PATH_MAX];snprintf(data,sizeof(data),"%s/data",app);
        setenv("MUSTARDTUBE_DATA_DIR",data,1);
        int nullfd=open("/dev/null",O_RDWR);
        if(nullfd>=0){dup2(nullfd,STDOUT_FILENO);dup2(nullfd,STDERR_FILENO);if(nullfd>2)close(nullfd);}
        execv(node,args);_exit(127);
    }
    if(child>0){int status;while(waitpid(child,&status,0)<0 && errno==EINTR){} }
}
/* Avoid repeatedly spawning Node at the ends of the 0-100% range.
 * This file is the app's own saved radio mix, NOT the hardware sink. */
static bool radio_at_limit(const char *app,int delta){
    char path[PATH_MAX],buf[16]={0};
    if(snprintf(path,sizeof(path),"%s/data/radio-mix-volume.txt",app)>=(int)sizeof(path))return false;
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return false;
    ssize_t n=read(fd,buf,sizeof(buf)-1);close(fd);
    if(n<=0)return false;
    char *end=NULL;errno=0;long level=strtol(buf,&end,10);
    if(errno || end==buf || (end[0]!='\0' && (end[0]!='\n'||end[1]!='\0')) ||
       level<0 || level>100)return false;
    return (delta>0 && level==100)||(delta<0 && level==0);
}
/* Single-process press-and-hold autorepeat. No timer thread or extra daemon.
 * 300 ms initial delay, then at most one 5% change every 150 ms.
 * The existing evdev reader remains read-only and never grabs buttons.
 */
#define HOLD_INITIAL_MS 300LL
#define HOLD_REPEAT_MS  150LL
struct keys { bool l2,r2,left,right,up,down;int hat,hat_y; };
struct repeat_key { int step; long long due_ms; };
static long long mono_ms(void){
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return 0;
    return (long long)t.tv_sec*1000LL+t.tv_nsec/1000000LL;
}
static bool held_direction(const struct keys *k,int step){
    if(!k->l2||!k->r2)return false;
    switch(step){
    case -1:return k->left||k->hat<0;
    case 1:return k->right||k->hat>0;
    case 2:return k->up||k->hat_y<0;
    case -2:return k->down||k->hat_y>0;
    default:return false;
    }
}
static void repeat_cancel(struct repeat_key *r){r->step=0;r->due_ms=0;}
static void repeat_event(struct repeat_key *r,const struct keys *k,int edge,long long now,int mode){
    /* Release any shoulder or the active direction: cancel without a write. */
    if(r->step && (!held_direction(k,r->step) || (abs(r->step)==2 && mode!=2)))
        repeat_cancel(r);
    /* Only a genuine press edge may start repeat; ignore kernel EV_KEY repeat. */
    if(edge && held_direction(k,edge) && (abs(edge)==1||mode==2)){
        r->step=edge;
        r->due_ms=now+HOLD_INITIAL_MS;
    }
}
static int repeat_due(const struct repeat_key *r,const struct keys *k,long long now,int mode){
    if(!r->step || now<r->due_ms || !held_direction(k,r->step) ||
       (abs(r->step)==2&&mode!=2))return 0;
    return r->step;
}
static int repeat_poll_ms(const struct repeat_key *r,long long now){
    if(!r->step)return 1000;
    long long wait=r->due_ms-now;
    if(wait<=0)return 0;
    return wait>1000?1000:(int)wait;
}

static int direction(struct keys*k,struct input_event ev){
    if(ev.type==EV_KEY){
        if(ev.code==BTN_TL2){k->l2=ev.value!=0;return 0;}
        if(ev.code==BTN_TR2){k->r2=ev.value!=0;return 0;}
        if(ev.code==KEY_LEFT||ev.code==BTN_DPAD_LEFT){
            bool old=k->left;k->left=ev.value!=0;
            return ev.value==1&&!old&&k->l2&&k->r2?-1:0;
        }
        if(ev.code==KEY_RIGHT||ev.code==BTN_DPAD_RIGHT){
            bool old=k->right;k->right=ev.value!=0;
            return ev.value==1&&!old&&k->l2&&k->r2?1:0;
        }
        if(ev.code==KEY_UP||ev.code==BTN_DPAD_UP){
            bool old=k->up;k->up=ev.value!=0;
            return ev.value==1&&!old&&k->l2&&k->r2?2:0;
        }
        if(ev.code==KEY_DOWN||ev.code==BTN_DPAD_DOWN){
            bool old=k->down;k->down=ev.value!=0;
            return ev.value==1&&!old&&k->l2&&k->r2?-2:0;
        }
    }else if(ev.type==EV_ABS&&(ev.code==ABS_HAT0X||ev.code==ABS_HAT0Y)){
        int *axis=ev.code==ABS_HAT0X?&k->hat:&k->hat_y;
        int old=*axis;*axis=ev.value;
        if(k->l2&&k->r2&&old!=ev.value){
            if(ev.code==ABS_HAT0X){
                if(ev.value<0)return -1;
                if(ev.value>0)return 1;
            }else{
                if(ev.value<0)return 2;  /* UP = GAME louder */
                if(ev.value>0)return -2; /* DOWN = GAME quieter */
            }
        }
    }
    return 0;
}
static int selftest(void){
    struct keys k={0};struct input_event e={.type=EV_KEY,.code=BTN_TL2,.value=1};
    if(direction(&k,e))return 1;
    e.code=BTN_TR2;if(direction(&k,e))return 2;
    e.type=EV_ABS;e.code=ABS_HAT0X;e.value=-1;
    if(direction(&k,e)!=-1||direction(&k,e))return 3;
    e.value=0;if(direction(&k,e))return 4;
    e.value=1;if(direction(&k,e)!=1)return 5;
    e.type=EV_KEY;e.code=BTN_TR2;e.value=0;if(direction(&k,e))return 6;
    e.type=EV_ABS;e.code=ABS_HAT0X;e.value=0;
    if(direction(&k,e))return 7;
    e.value=-1;if(direction(&k,e))return 8;
    e.type=EV_KEY;e.code=BTN_TR2;e.value=1;
    if(direction(&k,e))return 9;
    e.type=EV_ABS;e.code=ABS_HAT0Y;e.value=-1;
    if(direction(&k,e)!=2 || direction(&k,e))return 10;
    e.value=0;if(direction(&k,e))return 11;
    e.value=1;if(direction(&k,e)!=-2)return 12;
    e.type=EV_KEY;e.code=BTN_TL2;e.value=0;
    if(direction(&k,e))return 13;
    e.type=EV_ABS;e.code=ABS_HAT0Y;e.value=0;
    if(direction(&k,e))return 14;
    e.value=-1;if(direction(&k,e))return 15;
    /* Deterministic monotonic-time repeat state tests; no sleeping/evdev needed. */
    struct repeat_key rep={0};
    struct keys h={.l2=true,.r2=true};
    struct input_event down={.type=EV_ABS,.code=ABS_HAT0Y,.value=1};
    if(direction(&h,down)!=-2)return 16;
    repeat_event(&rep,&h,-2,1000,2);
    if(repeat_poll_ms(&rep,1000)!=HOLD_INITIAL_MS ||
       repeat_due(&rep,&h,1000+HOLD_INITIAL_MS-1,2))return 17;
    if(repeat_due(&rep,&h,1000+HOLD_INITIAL_MS,2)!=-2)return 18;
    rep.due_ms=1000+HOLD_INITIAL_MS+HOLD_REPEAT_MS;
    if(repeat_due(&rep,&h,rep.due_ms-1,2) ||
       repeat_due(&rep,&h,rep.due_ms,2)!=-2)return 19;
    /* Key-repeat EV_KEY value 2 never starts a second adjustment. */
    struct input_event shift={.type=EV_KEY,.code=KEY_DOWN,.value=2};
    struct keys keyheld={.l2=true,.r2=true,.down=true};
    if(direction(&keyheld,shift))return 20;
    /* Release direction, or either shoulder, must cancel immediately. */
    down.value=0;
    if(direction(&h,down))return 21;
    repeat_event(&rep,&h,0,1710,2);
    if(rep.step || repeat_due(&rep,&h,2000,2))return 22;
    down.value=1;
    if(direction(&h,down)!=-2)return 23;
    repeat_event(&rep,&h,-2,2000,2);
    h.r2=false;repeat_event(&rep,&h,0,2001,2);
    if(rep.step)return 24;
    h.r2=true;
    /* Disabling game mode cancels UP/DOWN, while radio LEFT/RIGHT remains. */
    repeat_event(&rep,&h,-2,3000,2);
    repeat_event(&rep,&h,0,3001,1);
    if(rep.step)return 25;
    h.hat_y=0;h.hat=-1;
    repeat_event(&rep,&h,-1,3100,1);
    if(repeat_due(&rep,&h,3600,1)!=-1)return 26;
    repeat_cancel(&rep);
    if(repeat_due(&rep,&h,9000,1))return 27;
    puts("PASS: L2+R2 radio LEFT/RIGHT, game UP/DOWN, single-press and hold-to-repeat; release/mode cancel");return 0;
}
/* Read-only diagnostic: whether an existing monitor holds the app-owned lock.
 * Does not open/grab controller inputs or change volume. */
static int do_status(const char*app){
    if(app[0]!='/'||strlen(app)>PATH_MAX-100)return 2;
    char lockpath[PATH_MAX];
    if(snprintf(lockpath,sizeof(lockpath),"%s/data/radio-hotkeys.lock",app)>=(int)sizeof(lockpath))return 2;
    int fd=open(lockpath,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0){puts("monitor=inactive");return 0;}
    if(flock(fd,LOCK_EX|LOCK_NB)==0){
        puts("monitor=inactive");
        flock(fd,LOCK_UN);
    }else if(errno==EWOULDBLOCK || errno==EAGAIN){
        puts("monitor=active");
    }else{
        puts("monitor=unknown");
    }
    close(fd);
    return 0;
}
static int do_monitor(const char*app,pid_t pid,unsigned long long started){
    if(app[0]!='/'||strlen(app)>PATH_MAX-100||!proc_valid(pid,started))return 2;
    char lockpath[PATH_MAX];snprintf(lockpath,sizeof(lockpath),"%s/data/radio-hotkeys.lock",app);
    int lockfd=open(lockpath,O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if(lockfd<0)return 0;
    /* A station change can start the next helper before the previous radio's
     * helper releases the lock. Retry for up to 2 seconds, not forever. */
    bool locked=false;
    for(int attempt=0;attempt<20 && !quit;attempt++){
        if(flock(lockfd,LOCK_EX|LOCK_NB)==0){locked=true;break;}
        if(errno!=EWOULDBLOCK && errno!=EAGAIN)break;
        if(!proc_valid(pid,started)||access(app,F_OK)!=0)break;
        usleep(100000);
    }
    if(!locked){close(lockfd);return 0;}
    struct keys keys={0};struct repeat_key repeating={0};int fd=-1;unsigned loops=0;
    struct timespec last_radio={0},last_game={0};
    bool game_mode=false;
    long long next_game_scan=0;
    /* Recover a previous session only for the SAME validated game stream. */
    (void)mt_game_restore(app);
    while(!quit){
        if(!proc_valid(pid,started))break;
        if(access(app,F_OK)!=0){
            /* App has been deleted while radio was active: terminate ONLY its
             * validated audio process group, never a game or global service. */
            if(proc_valid(pid,started))kill(-pid,SIGTERM);
            break;
        }
        /* Disabled means no resident monitor. A future opt-in starts a new
         * helper through set-hotkeys 1 without changing global muOS files. */
        int mode=preferences(app);
        if(mode==0)break;
        if(mode!=2 && game_mode){(void)mt_game_restore(app);game_mode=false;}
        if(repeating.step && (!held_direction(&keys,repeating.step) ||
            (abs(repeating.step)==2 && mode!=2)))repeat_cancel(&repeating);
        if(mode==2){
            game_mode=true;
            long long scan_now=mono_ms();
            if(scan_now>=next_game_scan){
                next_game_scan=scan_now+1250;
                /* /proc-only fast path once a game is identified. */
                (void)mt_game_mix_sync(app,pid);
            }
        }
        if(fd<0 && loops%3==0){
            fd=scan_gamepad(NULL,0);memset(&keys,0,sizeof(keys));repeat_cancel(&repeating);
        }
        struct pollfd pfd={.fd=fd,.events=POLLIN};
        int count=fd>=0?1:0;
        int rc=poll(&pfd,count,repeat_poll_ms(&repeating,mono_ms()));
        if(rc>0 && fd>=0 && (pfd.revents & (POLLERR|POLLHUP|POLLNVAL))){
            close(fd);fd=-1;memset(&keys,0,sizeof(keys));repeat_cancel(&repeating);
        }else if(rc>0 && fd>=0 && (pfd.revents&POLLIN)){
            struct input_event ev;
            while(read(fd,&ev,sizeof(ev))==(ssize_t)sizeof(ev)){
                if(ev.type==EV_SYN && ev.code==SYN_DROPPED){
                    /* Never keep repeating when evdev loses key releases. */
                    memset(&keys,0,sizeof(keys));repeat_cancel(&repeating);
                    continue;
                }
                int step=direction(&keys,ev);
                repeat_event(&repeating,&keys,step,mono_ms(),mode);
                if(step){
                    if(!proc_valid(pid,started))break;
                    if((step==2||step==-2) && mode!=2)continue;
                    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
                    struct timespec *last=(step==1||step==-1)?&last_radio:&last_game;
                    long ms=(now.tv_sec-last->tv_sec)*1000L+(now.tv_nsec-last->tv_nsec)/1000000L;
                    if(ms>=180){
                        *last=now;
                        if(step==1||step==-1)do_volume(app,step);
                        else (void)mt_game_adjust(app,pid,step>0?5:-5);
                    }
                }
            }
        }
        /* Poll wakes at the repeat deadline rather than busy-looping.
         * Read all pending releases above before changing audio again.
         * Never queue missed repeats after a slow wpctl/Node command. */
        int held=repeat_due(&repeating,&keys,mono_ms(),mode);
        if(held){
            if(!proc_valid(pid,started))break;
            if(abs(held)==1){
                if(radio_at_limit(app,held))repeat_cancel(&repeating);
                else do_volume(app,held);
            }else if(mode==2){
                /* Failure (e.g. ambiguous stream) or saturation: do not
                 * repeatedly launch expensive wpctl processes. */
                if(mt_game_adjust(app,pid,held>0?5:-5)!=0)
                    repeat_cancel(&repeating);
            }
            if(repeating.step)repeating.due_ms=mono_ms()+HOLD_REPEAT_MS;
        }
        loops++;
    }
    /* Restore even after SIGTERM, radio stop, disable, or station switch.
     * Never restores a recycled PID or an unrelated PipeWire node. */
    (void)mt_game_restore(app);
    if(fd>=0)close(fd);
    close(lockfd);return 0;
}
int main(int argc,char**argv){
    signal(SIGTERM,on_signal);signal(SIGINT,on_signal);
    if(argc==2&&!strcmp(argv[1],"--self-test"))return selftest();
    if(argc==2&&!strcmp(argv[1],"--list-input"))return do_list();
    if(argc==3&&!strcmp(argv[1],"--probe"))return do_probe(argv[2]);
    if(argc==3&&!strcmp(argv[1],"--status"))return do_status(argv[2]);
    if(argc==3&&!strcmp(argv[1],"--game-session"))return mt_game_session_status(argv[2]);
    if(argc==4&&!strcmp(argv[1],"--set-game-mix")){
        char *end=NULL;errno=0;long value=strtol(argv[3],&end,10);
        if(errno||end==argv[3]||*end||value<0||value>100)return 2;
        int rc=mt_game_mix_set(argv[2],(int)value);
        return rc==0?0:(rc==2?4:1);
    }
    if(argc==3&&!strcmp(argv[1],"--restore-game")){
        int result=mt_game_restore(argv[2]);
        return result==0?0:(result==2?3:1);
    }
    if(argc==3&&!strcmp(argv[1],"--recover-game")){
        int result=mt_game_recover_bounded(argv[2],interrupted);
        return result==0?0:(result==2?3:1);
    }
    if(argc==3&&!strcmp(argv[1],"--recover-once")){
        int result=mt_game_recover_once(argv[2]);
        return result==0?0:(result==2?3:1);
    }
    if(argc==4&&!strcmp(argv[1],"--inspect-game")){
        char *end=NULL;long radio=strtol(argv[3],&end,10);
        if(!end||*end||radio<=1||radio>INT_MAX)return 2;
        return mt_game_diagnostic(argv[2],(pid_t)radio);
    }
    if(argc==5&&!strcmp(argv[1],"--monitor")){
        char*end=NULL;long id=strtol(argv[3],&end,10);
        if(!end||*end||id<=1||id>INT_MAX)return 2;
        end=NULL;unsigned long long started=strtoull(argv[4],&end,10);
        if(!end||*end||started==0)return 2;
        return do_monitor(argv[2],(pid_t)id,started);
    }
    fputs("usage: radio-hotkeys --list-input | --probe /dev/input/eventN | --status APP_DIR | --game-session APP_DIR | --restore-game APP_DIR | --recover-game APP_DIR | --recover-once APP_DIR | --inspect-game APP_DIR RADIO_PID | --monitor APP_DIR PID START_TICKS\n",stderr);
    return 2;
}
