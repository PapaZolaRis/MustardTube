/* Host-only logic harness. No real SDL renderer; dead code stripped by linker.
   This includes the same production settings and layout functions directly. */
#define main mustardtube_device_main
#include "../src/main.c"
#undef main
#include <assert.h>
static const char *PREF_DIR;
char *SDL_GetPrefPath(const char*org,const char*app) { (void)org;(void)app;return strdup(PREF_DIR); }
void SDL_free(void*p) {free(p);}
void SDL_DestroyTexture(SDL_Texture*t) {(void)t;}

int main(void) {
    char location[]="/tmp/mustardtube-personalization-XXXXXX";
    assert(mkdtemp(location));
    char prefs[PATH_MAX];snprintf(prefs,sizeof(prefs),"%s/",location);PREF_DIR=prefs;
    App *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));assert(a && b);
    a->text_size=1;
    load_settings(a);
    snprintf(a->asset_dir,sizeof(a->asset_dir),"assets");
    assert(save_settings(a));
    change_preference(a,0,1); /* horizontal */
    change_preference(a,1,1); /* compact */
    change_preference(a,2,3); /* radio startup */
    change_preference(a,3,1); /* large */
    assert(a->nav_layout==1 && a->card_density==1 && a->startup_page==3 && a->text_size==2);
    b->text_size=1;load_settings(b);
    snprintf(b->asset_dir,sizeof(b->asset_dir),"assets");
    assert(b->nav_layout==1 && b->card_density==1 && b->startup_page==3 && b->text_size==2);
    assert(b->theme_id==0);
    b->render_content=true;
    assert(ui_project_x(b,145)==18 && ui_project_x(b,622)==622);
    assert(ui_project_y(b,83)==109 && ui_project_y(b,420)==413);
    assert(ui_unproject_x(b,18)==145 && ui_unproject_x(b,622)==622);
    assert(ui_font(b,FS_11)==FS_13);
    b->nav_layout=NAV_COMPACT;
    assert(ui_project_x(b,145)==86 && ui_project_x(b,622)==622);
    b->nav_layout=NAV_VERTICAL;
    assert(ui_project_x(b,145)==145 && ui_project_y(b,83)==83);
    assert(ui_font(b,FS_13)==FS_15);
    /* Tampered config values must not unlock invalid array indices. */
    char ini[PATH_MAX];snprintf(ini,sizeof(ini),"%ssettings.ini",prefs);
    FILE *f=fopen(ini,"w");assert(f);
    fputs("theme=bad\nnavigation=9\ncards=9\nstartup=9\ntext_size=9\n",f);fclose(f);
    App*c=calloc(1,sizeof(*c));assert(c);c->text_size=1;load_settings(c);
    assert(c->nav_layout==0 && c->card_density==0 && c->startup_page==0 && c->text_size==1);
    assert(save_settings(b));
    /* Appearance reset requires two confirmation presses and does not touch media. */
    char fav[PATH_MAX];snprintf(fav,sizeof(fav),"%sfavourites.txt",prefs);
    f=fopen(fav,"w");assert(f);fputs("keep",f);fclose(f);
    b->personalize_idx=4;b->reset_armed=false;
    personalize_activate(b);assert(b->reset_armed && b->text_size==2);
    personalize_activate(b);assert(!b->reset_armed && b->text_size==1 && b->nav_layout==0 && b->card_density==0 && b->startup_page==0);
    f=fopen(fav,"r");assert(f);char word[6]={0};assert(fread(word,1,4,f)==4 && !strcmp(word,"keep"));fclose(f);
    free(a);free(b);free(c);
    unlink(ini);unlink(fav);rmdir(location);
    puts("PASS: real C preferences persistence, defaults, bounds, projection, reset and user-data protection");
    return 0;
}
