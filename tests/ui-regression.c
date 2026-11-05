#define main mustardtube_application_main
#include "../src/main.c"
#undef main
#include <assert.h>
#include <sys/stat.h>

int main(void) {
    char root[]="/tmp/mustardtube-ui-test-XXXXXX";
    assert(mkdtemp(root));
    App a={0};
    snprintf(a.data_dir,sizeof(a.data_dir),"%s",root);
    a.page=RESULTS;a.running=true;
    for(int count=8;count<=40;count+=8){
        char rows[12000]="",row[200];
        for(int i=1;i<=count;i++){
            snprintf(row,sizeof(row),"%011d|||Video %d|||Channel|||01:00\n",i,i);
            strcat(rows,row);
        }
        assert(write_data_file(&a,"search-results.tsv",rows));
        assert(write_data_file(&a,"more-available.txt",count<40?"1":"0"));
        load_result_file(&a);
        assert(a.result_count==count);
        a.running=true;a.request_more=false;
        on_button(&a,SDL_CONTROLLER_BUTTON_X);
        assert(a.request_more==(count<40));
    }
    /* Home actions: Search is local; Last results reuses cached list. */
    a.page=HOME;a.running=true;a.request_youtube=false;
    home_action(&a,0);
    assert(a.page==SEARCH && a.running && !a.request_youtube);
    a.page=HOME;home_action(&a,1);
    assert(a.page==RESULTS && a.result_count==40);
    /* Navigation uses original app inputs; About is a normal fifth tab. */
    a.page=STYLE;a.running=true;cycle_page(&a,1);assert(a.page==ABOUT);
    cycle_page(&a,1);assert(a.page==HOME);
    cycle_page(&a,-1);assert(a.page==ABOUT);
    on_button(&a,SDL_CONTROLLER_BUTTON_B);assert(a.page==HOME);
    a.page=HOME;cycle_page(&a,-1);assert(a.page==ABOUT);
    a.page=SAVED;a.saved_idx=1;recent_action(&a);
    assert(a.page==RESULTS && a.running);
    a.page=SAVED;a.saved_idx=0;strcpy(a.query,"science space");recent_action(&a);
    assert(a.request_youtube && !a.running && a.page==SEARCH);
    remove_data_file(&a,"pending-search.txt");
    remove_data_file(&a,"query.txt");
    a.page=STYLE;a.running=true;
    on_button(&a,SDL_CONTROLLER_BUTTON_X);
    assert(a.request_clear && !a.running);
    assert(next_quality(360)==480);
    assert(next_quality(480)==720);
    assert(next_quality(720)==360);
    a.page=PLAYER;a.current_is_youtube=true;a.quality=360;
    strcpy(a.current_video.id,"00000000001");
    char ready[100];snprintf(ready,sizeof(ready),"00000000001\n%ld\n360\n",(long)time(NULL)+60);
    write_data_file(&a,"prefetch-ready.txt",ready);
    assert(stream_is_prepared(&a));a.quality=480;assert(!stream_is_prepared(&a));
    a.quality=720;assert(!stream_is_prepared(&a));a.quality=480;
    write_data_file(&a,"prefetch-ready.txt","00000000001\n1\n480\n");
    assert(!stream_is_prepared(&a));
    /* A real process group: navigating away must stop the worker and child. */
    char script_dir[PATH_MAX],script_path[PATH_MAX];
    snprintf(script_dir,sizeof(script_dir),"%s/scripts",root);assert(mkdir(script_dir,0700)==0);
    snprintf(script_path,sizeof(script_path),"%s/scripts/youtube-backend.sh",root);
    FILE*f=fopen(script_path,"w");assert(f);
    fputs("#!/bin/sh\nsleep 20 &\nwait\n",f);fclose(f);
    setenv("MUSTARDTUBE_APP_DIR",root,1);
    start_prefetch(&a);assert(a.worker_pid>0);
    pid_t worker=a.worker_pid;
    usleep(100000);
    start_prefetch(&a);assert(a.worker_pid==worker); /* No duplicate extractor. */
    a.page=RESULTS; a.result_count=2;
    navigate(&a,0,1);
    assert(a.worker_pid==0);
    assert(kill(worker,0)==-1 && errno==ESRCH);
    unsetenv("MUSTARDTUBE_APP_DIR");
    const char*files[]={"search-results.tsv","more-available.txt","next-result-index.txt","prefetch-ready.txt","selected.txt"};
    for(size_t i=0;i<sizeof(files)/sizeof(files[0]);i++)remove_data_file(&a,files[i]);
    unlink(script_path);rmdir(script_dir);rmdir(root);
    puts("PASS: controller pagination, 360/480/720 quality cycle, readiness/quality/expiry, worker reuse and navigation cancellation");
    return 0;
}
