/* Host regression harness, never installed on the handheld. */
#include "../src/game_mixer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char**argv){
    if(argc!=4){fputs("usage: harness MODE APP RADIO_PID\n",stderr);return 2;}
    int pid=atoi(argv[3]);
    if(!strcmp(argv[1],"mix-get")){printf("mix=%d\n",mt_game_mix_level(argv[2]));return 0;}
    if(!strcmp(argv[1],"mix-set"))return mt_game_mix_set(argv[2],pid)==0?0:1;
    if(!strcmp(argv[1],"mix-sync"))return mt_game_mix_sync(argv[2],(pid_t)pid)==0?0:1;
    if(!strcmp(argv[1],"up"))return mt_game_adjust(argv[2],(pid_t)pid,5)==0?0:1;
    if(!strcmp(argv[1],"up-at-limit"))return mt_game_adjust(argv[2],(pid_t)pid,5)==1?0:1;
    if(!strcmp(argv[1],"down"))return mt_game_adjust(argv[2],(pid_t)pid,-5)==0?0:1;
    if(!strcmp(argv[1],"restore"))return mt_game_restore(argv[2])==0?0:1;
    if(!strcmp(argv[1],"inspect"))return mt_game_diagnostic(argv[2],(pid_t)pid);
    return 2;
}
