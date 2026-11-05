/* Test real Home parser/navigation with no SDL video or network needed. */
#define main mustardtube_application_main
#include "../src/main.c"
#undef main
#include <assert.h>
int main(void) {
    char dir[]="/tmp/mt-home-ui-XXXXXX";
    assert(mkdtemp(dir));
    App *a=calloc(1,sizeof(*a));assert(a);
    snprintf(a->data_dir,sizeof(a->data_dir),"%s",dir);
    char path[PATH_MAX];
    snprintf(path,sizeof(path),"%s/home-results.tsv",dir);
    FILE*f=fopen(path,"w");assert(f);
    fputs("00000000001|||One video title|||Channel One|||03:42\n",f);
    fputs("../../invalid|||Unsafe|||Bad channel|||00:12\n",f);
    fputs("00000000002|||Second video|||Channel Two|||11:24\n",f);
    fputs("00000000003|||Third video|||Channel Three|||04:57\n",f);
    fputs("00000000004|||Fourth video|||Channel Four|||02:13\n",f);
    fclose(f);
    snprintf(path,sizeof(path),"%s/home-updated.txt",dir);
    f=fopen(path,"w");assert(f);fputs("1111111111\n",f);fclose(f);
    a->page=HOME;load_home_file(a);
    assert(a->home_count==4);
    assert(!strcmp(a->home_videos[0].id,"00000000001"));
    assert(!strcmp(a->home_videos[3].title,"Fourth video"));
    navigate(a,0,1);assert(a->home_idx==1);
    navigate(a,0,1);assert(a->home_idx==2);
    navigate(a,0,1);assert(a->home_idx==3);
    navigate(a,0,1);assert(a->home_idx==0);
    navigate(a,0,-1);assert(a->home_idx==3);
    /* An unchanged cache cannot silently erase previous cards. */
    unlink(path);load_home_file(a);assert(a->home_count==4);
    assert(!strcmp(a->home_videos[0].channel,"Channel One"));
    /* Selecting a Home video must use the original details path and return Home. */
    a->home_idx=2;
    home_action(a,2);
    assert(a->page==PLAYER && a->from_page==HOME);
    assert(!strcmp(a->current_video.id,"00000000003"));
    a->current_is_youtube=false;a->from_page=RESULTS;
    load_selected_video(a);
    assert(a->current_is_youtube && a->from_page==HOME);
    assert(!strcmp(a->current_video.channel,"Channel Three"));
    YoutubeResult bad;
    assert(!read_result_line("#invalid|||Fake|||Channel|||10:00",&bad));
    snprintf(path,sizeof(path),"%s/selected.txt",dir);unlink(path);
    snprintf(path,sizeof(path),"%s/selected-origin.txt",dir);unlink(path);
    snprintf(path,sizeof(path),"%s/home-results.tsv",dir);unlink(path);
    snprintf(path,sizeof(path),"%s/home-updated.txt",dir);unlink(path);
    rmdir(dir);free(a);
    puts("PASS: Home parses real metadata, rejects invalid IDs, keeps stale cards and navigates all four entries");
    return 0;
}
