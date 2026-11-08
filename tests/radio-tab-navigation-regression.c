/* MustardTube radio-tab regression: active player, country-list escape,
 * and stale background state. Fake live-radio identity uses this test's
 * own /proc/self/exe; no actual audio stream is opened. */
#define _DEFAULT_SOURCE
#include <sys/stat.h>
#define main mustardtube_application_main
#include "../src/main.c"
#undef main
#include <assert.h>

static void write_file(const char *dir, const char *name, const char *text) {
    char path[PATH_MAX];
    assert(snprintf(path,sizeof(path),"%s/%s",dir,name)<(int)sizeof(path));
    FILE*f=fopen(path,"w");assert(f);
    assert(fputs(text,f)>=0);assert(fclose(f)==0);
}
static void remove_file(const char *dir,const char*name){
    char path[PATH_MAX];
    assert(snprintf(path,sizeof(path),"%s/%s",dir,name)<(int)sizeof(path));
    assert(unlink(path)==0);
}
int main(void){
    char tmp[]="/tmp/mt-radio-tabs-XXXXXX";assert(mkdtemp(tmp));
    char bindir[PATH_MAX],linkpath[PATH_MAX],self[PATH_MAX];
    assert(snprintf(bindir,sizeof(bindir),"%s/bin",tmp)<(int)sizeof(bindir));
    assert(mkdir(bindir,0700)==0);
    ssize_t n=readlink("/proc/self/exe",self,sizeof(self)-1);assert(n>0);
    self[n]=0;
    assert(snprintf(linkpath,sizeof(linkpath),"%s/ffplay-radio",bindir)<(int)sizeof(linkpath));
    assert(symlink(self,linkpath)==0);
    assert(setenv("MUSTARDTUBE_APP_DIR",tmp,1)==0);

    char state[128];snprintf(state,sizeof(state),"{\"pid\":%ld}\n",(long)getpid());
    write_file(tmp,"radio-background-state.json",state);
    write_file(tmp,"radio-background-name.txt","Test Station\n");
    write_file(tmp,"radio-now-meta.txt","test-id\nMY\nMP3\nTest Station\n");
    write_file(tmp,"radio-countries.tsv","MY|||Malaysia|||42\n");

    App*a=calloc(1,sizeof(App));assert(a);
    snprintf(a->data_dir,sizeof(a->data_dir),"%s",tmp);
    assert(radio_now_process_alive(a));
    a->page=SAVED;
    cycle_page(a,1); /* R1: Recent -> Radio */
    assert(a->page==RADIO_NOW && a->radio_now_active);
    assert(strcmp(a->radio_now_station.name,"Test Station")==0);
    cycle_page(a,1); /* R1: Now Playing -> Settings; no directory detour */
    assert(a->page==STYLE);
    cycle_page(a,-1); /* L1: Settings -> Radio */
    assert(a->page==RADIO_NOW && a->radio_now_active);
    cycle_page(a,-1); /* L1: Radio -> Recent */
    assert(a->page==SAVED);
    keydown(a,SDLK_F4); /* Keyboard direct Radio tab */
    assert(a->page==RADIO_NOW);
    back(a); /* B explicitly opens country list while radio plays */
    assert(a->page==RADIO && a->radio_mode==0 && a->country_count==1);
    open_nav_tab(a,RADIO); /* Tap Radio tab again to return to player */
    assert(a->page==RADIO_NOW);
    a->page=SAVED;
    mouse_down(a,460,30); /* Radio header click */
    assert(a->page==RADIO_NOW);

    remove_file(tmp,"radio-background-state.json");
    a->page=SAVED;
    cycle_page(a,1); /* Radio stopped: show directory */
    assert(a->page==RADIO && a->radio_mode==0);
    write_file(tmp,"radio-background-state.json","{\"pid\":99999999}\n");
    a->page=STYLE;
    cycle_page(a,-1); /* Stale metadata must not open Now Playing */
    assert(a->page==RADIO);

    remove_file(tmp,"radio-background-state.json");
    remove_file(tmp,"radio-background-name.txt");
    remove_file(tmp,"radio-now-meta.txt");
    remove_file(tmp,"radio-countries.tsv");
    /* This test never starts directory workers because a country cache exists. */
    unlink(linkpath);rmdir(bindir);rmdir(tmp);
    free(a);
    puts("PASS: L1/R1 Radio tab resumes native Now Playing; B opens list; stopped/stale radio opens directory");
    return 0;
}
