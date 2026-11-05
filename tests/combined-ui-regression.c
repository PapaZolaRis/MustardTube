/* Native no-render regression: six Home categories and four-at-a-time Search. */
#define main mustardtube_app_main
#include "../src/main.c"
#undef main
#include <assert.h>
int main(void){
    assert(HOME_CATEGORIES==6 && NAV_TAB_COUNT==6 && RESULTS_VISIBLE==4);
    assert(!strcmp(home_category_keys[0],"explore"));
    assert(!strcmp(home_category_keys[5],"tech"));
    char dir[]="/tmp/mt-combined-ui-XXXXXX";assert(mkdtemp(dir));
    App*a=calloc(1,sizeof(*a));assert(a);
    snprintf(a->data_dir,sizeof(a->data_dir),"%s",dir);
    char path[PATH_MAX];time_t now=time(NULL);
    for(int cat=0;cat<HOME_CATEGORIES;cat++){
        snprintf(path,sizeof(path),"%s/home-%s-results.tsv",dir,home_category_keys[cat]);
        FILE*f=fopen(path,"w");assert(f);
        fprintf(f,"%011d|||Category %d video|||Channel|||03:00\n",cat+1,cat);
        fclose(f);
        snprintf(path,sizeof(path),"%s/home-%s-updated.txt",dir,home_category_keys[cat]);
        f=fopen(path,"w");assert(f);fprintf(f,"%lld\n",(long long)now);fclose(f);
    }
    a->page=HOME;load_home_file(a);assert(a->home_count==1 && a->home_category==0);
    for(int c=1;c<HOME_CATEGORIES;c++){
        navigate(a,1,0);assert(a->home_category==c);
        assert(a->home_count==1 && !strncmp(a->home_videos[0].title,"Category",8));
        assert(a->home_videos[0].id[10]=='0'+c+1);
    }
    navigate(a,1,0);assert(a->home_category==0);
    navigate(a,0,1);assert(a->home_idx==0);
    a->page=RESULTS;a->result_count=8;a->result_idx=3;
    for(int i=0;i<8;i++)snprintf(a->results[i].id,sizeof(a->results[i].id),"%011d",i+1);
    navigate(a,0,1);assert(a->result_idx==4 && a->result_thumb_first==4);
    navigate(a,0,1);assert(a->result_idx==5 && a->result_thumb_first==4);
    navigate(a,0,-1);assert(a->result_idx==4);
    navigate(a,0,-1);assert(a->result_idx==3 && a->result_thumb_first==0);
    for(int c=0;c<HOME_CATEGORIES;c++){
        snprintf(path,sizeof(path),"%s/home-%s-results.tsv",dir,home_category_keys[c]);unlink(path);
        snprintf(path,sizeof(path),"%s/home-%s-updated.txt",dir,home_category_keys[c]);unlink(path);
    }
    rmdir(dir);free(a);
    puts("PASS 6 Home categories, independent cached feeds, 4-at-a-time Search pagination and thumbnail page changes");
    return 0;
}
