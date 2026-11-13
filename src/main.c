#define _DEFAULT_SOURCE
/* MustardTube v0.9.11 Beta 2.2.1 - low-power native radio improvements.
   Based on the Beta 2.1 display compatibility release; original video paths preserved.
   Copyright (c) 2026 MustardTube contributors.
   SPDX-License-Identifier: GPL-3.0-only
   Home shows genuinely fetched YouTube discovery videos, not invented trending rankings. */
#include "release.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include "display_compat.h" /* Lightweight resolution/aspect and pointer mapping. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h> /* Case-insensitive alphabetical radio sorting. */
#include <limits.h>
#include <math.h>
#include <unistd.h> /* access() - check stock Wasabi executable */
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <arpa/inet.h> /* Reject private literal IP stream addresses. */

#define WASABI_BIN "/opt/muos/frontend/muxmedia"
/* Media selection and spawning are owned by the mux_launch.sh supervisor. */

#define W 640
#define H 480
#define MAX_CACHE 96
#define HOME_VIDEOS 4
#define HOME_CATEGORIES 6
#define RESULTS_VISIBLE 4
#define HOME_CACHE_TTL 3600
#define NAV_TAB_COUNT 6
#define RADIO_MAX_COUNTRIES 250
#define RADIO_MAX_FAVORITES 32
#define RADIO_PLAY_EXIT 82
#define RADIO_BACKGROUND_START_EXIT 84
#define RADIO_BACKGROUND_STOP_EXIT 85
#define RADIO_NATIVE_START_EXIT 86
#define RADIO_NATIVE_STOP_EXIT 87
#define SEARCH_RESULTS 120
#define YT_MORE_EXIT 80
#define CLEAR_CACHE_EXIT 81
#define CLEAR_HISTORY_EXIT 83
#define UPDATE_CHECK_EXIT 88
#define UPDATE_DOWNLOAD_EXIT 89
#define YT_SEARCH_EXIT 78
#define YT_PLAY_EXIT 77
#define LOCAL_TEST_EXIT 79
#define TCOUNT 4
#define KEY_COUNT 39
#define MAX_QUERY 95

typedef enum { HOME, SEARCH, SAVED, RADIO, STYLE, ABOUT, PLAYER, RESULTS, RADIO_NOW, UPDATES, EXTRA, PERSONALIZE } Page;
typedef enum { NAV_VERTICAL=0, NAV_HORIZONTAL=1, NAV_COMPACT=2 } NavigationLayout;
typedef enum { FS_11, FS_13, FS_15, FS_19, FS_23, FS_29, FS_COUNT } FontSize;
typedef struct {
    char name[32];
    SDL_Color bg, bg2, surface, alt, selected, text, muted, accent, accent_ink, accent_text, secondary, outline;
} Theme;
/* Input from the bundled yt-dlp resolver is parsed as bounded, validated TSV. */
typedef struct {char id[16], title[176], channel[84], duration[24];} YoutubeResult;

static const char *theme_files[TCOUNT] = {"midnight", "forest", "paper", "acidbloom"};
static const char *page_names[NAV_TAB_COUNT] = {"Home", "Search", "Recent", "Radio", "Settings", "About"};
static const char *home_category_labels[HOME_CATEGORIES] = {"Explore", "Popular", "For You", "Music", "Gaming", "Tech"};
static const char *home_category_keys[HOME_CATEGORIES] = {"explore", "popular", "foryou", "music", "gaming", "tech"};
typedef struct {char code[4], name[66];int count;} RadioCountry;
typedef struct {char id[45],name[114],codec[20],url[760],country[4],logo[560];} RadioStation;
/* Two-byte country codes index a 16x16 country flag PNG atlas. */
static const char *RADIO_FLAG_CODES="ADAEAFAGAIALAMAOAQARASATAUAWAXAZBABBBDBEBFBGBHBIBJBLBMBNBOBQBRBSBTBVBWBYBZCACCCDCFCGCHCICKCLCMCNCOCRCUCVCWCXCYCZDEDJDKDMDODZECEEEGEHERESETFIFJFKFMFOFRGAGBGDGEGFGGGHGIGLGMGNGPGQGRGSGTGUGWGYHKHMHNHRHTHUIDIEILIMINIOIQIRISITJEJMJOJPKEKGKHKIKMKNKPKRKWKYKZLALBLCLILKLRLSLTLULVLYMAMCMDMEMFMGMHMKMLMMMNMOMPMQMRMSMTMUMVMWMXMYMZNANCNENFNGNINLNONPNRNUNZOMPAPEPFPGPHPKPLPMPNPRPSPTPWPYQARERORSRURWSASBSCSDSESGSHSISJSKSLSMSNSOSRSSSTSVSXSYSZTCTDTFTGTHTJTKTLTMTNTOTRTTTVTWTZUAUGUMUSUYUZVAVCVEVGVIVNVUWFWSYEYTZAZMZW";
static const char keyboard_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

typedef struct {
    char value[176];
    int font_id;
    uint32_t rgba;
    SDL_Texture *tex;
    int w, h;
} TextEntry;
typedef struct {
    SDL_Window *win;
    SDL_Renderer *r;
        SDL_Texture *thumbnail; /* Selected video details thumbnail. */
    SDL_Texture *brand_icon; /* Transparent mascot, dark themes; cached. */
    SDL_Texture *brand_icon_paper; /* Transparent mascot with ink outline, Paper only. */
    bool brand_icon_checked, brand_icon_paper_checked;
    SDL_Texture *home_thumbs[HOME_VIDEOS]; /* Maximum four bounded Home thumbnails. */
    SDL_Texture *result_thumbs[RESULTS_VISIBLE]; /* Four visible Search result thumbnails. */
    char result_thumb_ids[RESULTS_VISIBLE][16];
    pid_t result_thumb_pid;
    int result_thumb_first;
    Uint32 result_thumb_last_poll;
    char thumbnail_id[16];
    SDL_GameController *pad;
    TTF_Font *fonts[FS_COUNT];
    TextEntry text[MAX_CACHE];
    int cache_next;
    Theme theme;
    int theme_id;
    int nav_layout; /* Vertical (default), Horizontal or Compact. */
    int card_density; /* Standard 4-thumbnail results, or compact 6-row text list. */
    int startup_page; /* Home, Search, Recent or Radio. */
    int text_size; /* Small, Normal (default), Large. */
    int personalize_idx;
    bool reset_armed, render_content;
    bool running, dirty, handoff_wasabi, request_youtube, request_more, wasabi_missing, more_available;
    bool current_is_youtube;
    bool request_update_check, request_update_download;
    bool update_beta, update_auto;
    int update_selected, note_pos;
    Uint32 update_last_poll;
    char update_state[32], update_detail[150], update_release[64], update_size[40];
    int update_progress; /* -1 means no active download; bounded 0..100 otherwise. */
    bool awaiting_stream, stream_failed, request_clear, request_clear_history;
    long wait_display;
    time_t preview_started, wait_started;
    pid_t worker_pid;
    pid_t home_worker_pid;
    bool home_loading;
    Uint32 home_last_poll;
    Uint32 loading_dot_last_tick; /* Animate only during an active empty Home refresh. */
    time_t home_cache_stamp;
    int home_count;
    int home_category;
    YoutubeResult home_videos[HOME_VIDEOS];
    char home_feedback[100];
    RadioCountry countries[RADIO_MAX_COUNTRIES];
    RadioStation *stations; /* Dynamically sized; only metadata for loaded stations. */
    size_t station_capacity;
    RadioStation favourites[RADIO_MAX_FAVORITES];
    int country_count, country_idx, station_count, station_idx, favourite_count, favourite_idx;
    int radio_next_offset;
    bool radio_has_more;
    int radio_mode; /* 0 countries, 1 stations, 2 favourites */
    int about_scroll; /* Scrollable user manual and legal notice. */
    int nav_repeat_dir; /* Gamepad held-D-pad scrolling only in Radio/About. */
    Uint32 nav_repeat_at;
    pid_t radio_worker_pid;
    bool radio_loading, radio_handoff;
    int radio_handoff_kind;
    pid_t radio_logo_pid;
    int radio_logo_mode, radio_logo_first;
    char radio_logo_country[4];
    SDL_Texture *radio_flag_atlas;
    SDL_Texture *radio_logos[7];
    char radio_logo_ids[7][45];
    char radio_selected_country[4], radio_status[130];
    char radio_background_name[114];
    RadioStation radio_now_station; /* Safe display metadata only; no stream URL persisted. */
    bool radio_now_active;
    bool radio_now_blink; /* One status-dot update/second, only on the native radio screen. */
    bool radio_now_logo_checked; /* At most one cached image lookup per Now Playing visit. */
    Uint32 radio_now_last_tick;
    char radio_now_notice[140];
    int radio_mix_volume; /* PipeWire stream volume, independent of game audio. */
    int game_mix_volume; /* Saved preferred game level during active radio. */
    int radio_now_slider; /* 0 radio, 1 game; D-pad UP/DOWN selects. */
    int radio_hotkeys_mode; /* 0 OFF, 1 RADIO, 2 RADIO+GAME; no OS edits. */
    char worker_id[16];
    Uint32 play_started;
    int quality;
    Page page, from_page;
    int home_idx, saved_idx, keyboard_idx, style_idx, video_idx, result_idx, result_count;
    YoutubeResult results[SEARCH_RESULTS];
    YoutubeResult current_video;
    char status_message[176];
    char data_dir[PATH_MAX];
    char query[MAX_QUERY + 1];
    char search_feedback[100];
    char asset_dir[PATH_MAX];
    char pref_dir[PATH_MAX];
} App;

static SDL_Color color(int r, int g, int b) { SDL_Color c={(Uint8)r,(Uint8)g,(Uint8)b,255}; return c; }
static SDL_Color from_hex(const char *s, SDL_Color otherwise) {
    if (!s || s[0]!='#' || strlen(s)!=7) return otherwise;
    unsigned r,g,b; if (sscanf(s+1, "%2x%2x%2x", &r,&g,&b)!=3) return otherwise;
    return color((int)r,(int)g,(int)b);
}
static SDL_Color blend(SDL_Color a, SDL_Color b, int n, int d) {
    return color((a.r*(d-n)+b.r*n)/d,(a.g*(d-n)+b.g*n)/d,(a.b*(d-n)+b.b*n)/d);
}
static uint32_t packed(SDL_Color c) { return ((uint32_t)c.r<<24)|((uint32_t)c.g<<16)|((uint32_t)c.b<<8)|c.a; }
static void clear_cache(App *a) {
    for (int i=0;i<MAX_CACHE;i++) { if (a->text[i].tex) SDL_DestroyTexture(a->text[i].tex); memset(&a->text[i],0,sizeof(a->text[i])); }
    a->cache_next=0;
}
/* All page content is projected into one of three viewports. The default
 * projection is an exact identity, protecting the Beta 2.4.2 UI. No extra
 * textures or renderer targets are allocated for alternate layouts. */
static int ui_project_x(App *a,int x) {
    if(!a->render_content || a->nav_layout==NAV_VERTICAL)return x;
    int left=a->nav_layout==NAV_HORIZONTAL?18:86;
    int width=622-left;
    return left+(x-145)*width/477;
}
static int ui_project_y(App *a,int y) {
    return a->render_content && a->nav_layout==NAV_HORIZONTAL?35+y*9/10:y;
}
static int ui_unproject_x(App *a,int x) {
    if(a->nav_layout==NAV_VERTICAL)return x;
    int left=a->nav_layout==NAV_HORIZONTAL?18:86;
    return 145+(x-left)*477/(622-left);
}
static int ui_unproject_y(App *a,int y) {
    return a->nav_layout==NAV_HORIZONTAL?(y-35)*10/9:y;
}
static int ui_project_width(App*a,int x,int w){return ui_project_x(a,x+w)-ui_project_x(a,x);}
static FontSize ui_font(App*a,FontSize fs) {
    if(!a->render_content)return fs;
    if(a->text_size==0 && (fs==FS_13||fs==FS_15))return (FontSize)(fs-1);
    if(a->text_size==2 && fs>=FS_11 && fs<=FS_15)return (FontSize)(fs+1);
    return fs;
}
static void ui_draw_texture(App*a,SDL_Texture*tex,const SDL_Rect*src,const SDL_Rect*dst) {
    SDL_Rect d=*dst;
    d.x=ui_project_x(a,dst->x);d.y=ui_project_y(a,dst->y);
    d.w=ui_project_width(a,dst->x,dst->w);
    if(a->render_content && a->nav_layout==NAV_HORIZONTAL)d.h=(dst->h*9+5)/10;
    if(a->render_content && a->nav_layout!=NAV_VERTICAL && dst->w>0 && dst->h>0){
        /* Letterbox images into mapped slots; never stretch portrait/flag/video artwork. */
        const int fitw=d.h*dst->w/dst->h;
        if(fitw<d.w){d.x+=(d.w-fitw)/2;d.w=fitw;}
        else {const int fith=d.w*dst->h/dst->w;d.y+=(d.h-fith)/2;d.h=fith;}
    }
    SDL_RenderCopy(a->r,tex,src,&d);
}
static void fill(App *a, int x,int y,int w,int h,SDL_Color c) {
    if(w<=0||h<=0)return;
    SDL_Rect q={ui_project_x(a,x),ui_project_y(a,y),ui_project_width(a,x,w),h};
    if(a->render_content && a->nav_layout==NAV_HORIZONTAL)q.h=(h*9+9)/10;
    if(q.w<=0||q.h<=0)return;
    SDL_SetRenderDrawColor(a->r,c.r,c.g,c.b,c.a);SDL_RenderFillRect(a->r,&q);
}
static void rounded(App *a,int x,int y,int w,int h,int radius,SDL_Color c) {
    if(w<=0||h<=0)return;
    if(radius<1){fill(a,x,y,w,h,c);return;}
    if(radius*2>w)radius=w/2;
    if(radius*2>h)radius=h/2;
    fill(a,x+radius,y,w-2*radius,h,c);
    for(int i=0;i<radius;i++) {
        int cy=radius-i-1;
        int offset=radius-(int)(0.5+sqrt((double)(radius*radius-cy*cy)));
        if(offset<0)offset=0;
        fill(a,x+offset,y+i,w-2*offset,1,c);
        fill(a,x+offset,y+h-i-1,w-2*offset,1,c);
    }
    fill(a,x,y+radius,radius,h-2*radius,c);
    fill(a,x+w-radius,y+radius,radius,h-2*radius,c);
}
static void outline(App *a,int x,int y,int w,int h,int rad,SDL_Color c,SDL_Color inner) {
    rounded(a,x,y,w,h,rad,c); rounded(a,x+1,y+1,w-2,h-2,rad-1,inner);
}
/* Horizontal personalization compresses content vertically.  Projecting
 * each one-pixel corner scanline separately can make the inner fill erase the
 * top border.  For critical OTA controls, project the complete rectangle once
 * and draw its border at native screen-pixel resolution.  All other layouts
 * retain their original renderer behaviour and hit-test coordinates. */
static void outline_ota(App *a,int x,int y,int w,int h,int radius,SDL_Color border,SDL_Color inner) {
    if(a->render_content && a->nav_layout==NAV_HORIZONTAL){
        const int px=ui_project_x(a,x),py=ui_project_y(a,y);
        const int pw=ui_project_width(a,x,w),ph=(h*9+9)/10;
        a->render_content=false;
        outline(a,px,py,pw,ph,radius,border,inner);
        a->render_content=true;
    }else outline(a,x,y,w,h,radius,border,inner);
}
/* Distinctive, unbranded video-preview control: right-facing play symbol,
 * deliberately separate from MustardTube's actual app identity/mascot. */
static void preview_play_symbol(App *a,int cx,int cy,SDL_Color ink) {
    for(int dy=-8;dy<=8;dy++){
        const int width=1+(8-abs(dy))*2;
        fill(a,cx-7,cy+dy,width,1,ink);
    }
}
static void circle(App *a,int cx,int cy,int radius,SDL_Color c) {
    for(int dy=-radius;dy<=radius;dy++) {
        int rx=(int)sqrt((double)(radius*radius-dy*dy));
        fill(a,cx-rx,cy+dy,2*rx+1,1,c);
    }
}
/* Video artwork and loading indicators are distinct from the actionable play
 * control.  No image assets, textures or new dependencies are needed.  These
 * primitives inherit the current theme and the existing content projection. */
static void video_placeholder(App *a,int cx,int cy,bool large,SDL_Color stroke,SDL_Color bg) {
    const int w=large?60:34,h=large?42:26;
    const int x=cx-w/2,y=cy-h/2;
    outline(a,x,y,w,h,large?7:5,stroke,bg);
    /* Film perforations make the missing-artwork state readable even at 640x480. */
    const int strip=large?11:7;
    fill(a,x+strip,y+2,1,h-4,stroke);
    fill(a,x+w-strip-1,y+2,1,h-4,stroke);
    for(int i=0;i<3;i++) {
        int py=y+5+i*(h-12)/2;
        fill(a,x+3,py,large?4:2,3,stroke);
        fill(a,x+w-(large?7:5),py,large?4:2,3,stroke);
    }
}
static void loading_dots(App *a,int cx,int cy) {
    const int active=(int)((SDL_GetTicks()/420u)%3u);
    for(int i=0;i<3;i++) {
        SDL_Color dot=i==active?a->theme.accent:a->theme.muted;
        circle(a,cx+(i-1)*24,cy,i==active?6:4,dot);
    }
}
static void print_text(App *a,FontSize fs,const char *str,int x,int y,SDL_Color c) {
    fs=ui_font(a,fs);
    if(!str||!*str||!a->fonts[fs])return;
    x=ui_project_x(a,x);y=ui_project_y(a,y);
    int len=(int)strlen(str); if(len>175)return;
    uint32_t id=packed(c);
    for(int i=0;i<MAX_CACHE;i++) {
        TextEntry *e=&a->text[i];
        if(e->tex&&e->font_id==(int)fs&&e->rgba==id&&!strcmp(e->value,str)) {
            SDL_Rect dst={x,y,e->w,e->h}; SDL_RenderCopy(a->r,e->tex,NULL,&dst);return;
        }
    }
    SDL_Surface *s=TTF_RenderUTF8_Blended(a->fonts[fs],str,c);
    if(!s)return;
    SDL_Texture *tex=SDL_CreateTextureFromSurface(a->r,s);
    if(tex) {
        TextEntry *e=&a->text[a->cache_next];
        if(e->tex)SDL_DestroyTexture(e->tex);
        strncpy(e->value,str,sizeof(e->value)-1);e->value[sizeof(e->value)-1]='\0';
        e->tex=tex;e->font_id=(int)fs;e->rgba=id;e->w=s->w;e->h=s->h;
        SDL_Rect dst={x,y,s->w,s->h};SDL_RenderCopy(a->r,tex,NULL,&dst);
        a->cache_next=(a->cache_next+1)%MAX_CACHE;
    }
    SDL_FreeSurface(s);
}
static void text_center(App *a,FontSize fs,const char *str,int x,int y,int width,SDL_Color c) {
    int tw=0;FontSize f=ui_font(a,fs);
    if(a->fonts[f])TTF_SizeUTF8(a->fonts[f],str,&tw,NULL);
    int left=ui_project_x(a,x),right=ui_project_x(a,x+width);
    int target=left+(right-left-tw)/2;
    /* Convert back because print_text is the single place that projects X. */
    if(a->render_content) {
        if(a->nav_layout!=NAV_VERTICAL){
            int base=a->nav_layout==NAV_HORIZONTAL?18:86;
            target=145+(target-base)*477/(622-base);
        }
    }
    print_text(a,fs,str,target,y,c);
}
static void set_default_theme(Theme *t) {
    memset(t,0,sizeof(*t));strcpy(t->name,"Midnight");
    /* Default only when a theme file omits a field; matches Midnight. */
    t->bg=color(16,20,24);t->bg2=t->bg;t->surface=color(32,40,48);
    t->alt=color(40,51,60);t->selected=color(52,67,76);t->text=color(245,242,234);
    t->muted=color(186,197,200);t->accent=color(242,190,75);t->accent_ink=t->accent;
    t->accent_text=color(16,20,24);t->secondary=color(146,189,176);
    t->outline=color(83,99,109);
}
static void theme_field(Theme *t,const char *key,const char *v) {
    if(!strcmp(key,"name"))snprintf(t->name,sizeof(t->name),"%s",v);
#define TF(k,field) else if(!strcmp(key,k))t->field=from_hex(v,t->field)
    TF("background",bg);TF("background_bottom",bg2);TF("surface",surface);
    TF("surface_alt",alt);TF("surface_selected",selected);TF("text",text);
    TF("muted",muted);TF("accent",accent);TF("accent_ink",accent_ink);TF("accent_text",accent_text);TF("secondary",secondary);TF("outline",outline);
#undef TF
}
static bool read_theme(App *a,int id) {
    Theme t;set_default_theme(&t);
    char path[PATH_MAX];if(snprintf(path,sizeof(path),"%s/themes/%s.theme",a->asset_dir,theme_files[id])>=(int)sizeof(path))return false;
    FILE *f=fopen(path,"r");if(!f){fprintf(stderr,"Could not read theme: %s\n",path);return false;}
    char line[200];while(fgets(line,sizeof(line),f)) {
        char *start=line;while(isspace((unsigned char)*start))start++;
        if(*start=='#'||*start==';'||!*start)continue;
        char *eq=strchr(start,'=');if(!eq)continue;*eq++='\0';
        char *end=start+strlen(start);while(end>start&&isspace((unsigned char)end[-1]))*--end='\0';
        end=eq+strlen(eq);while(end>eq&&isspace((unsigned char)end[-1]))*--end='\0';
        theme_field(&t,start,eq);
    }
    fclose(f);a->theme=t;a->theme_id=id;a->dirty=true;
    clear_cache(a);return true;
}
static bool exists(const char *path) {FILE*f=fopen(path,"rb");if(!f)return false;fclose(f);return true;}
static bool discover_assets(App *a,const char *given) {
    char path[PATH_MAX];const char *tries[]={given,"assets","../assets","../../assets",NULL};
    for(int i=0;tries[i];i++) {
        if(!tries[i]||!*tries[i])continue;
        snprintf(path,sizeof(path),"%s/themes/midnight.theme",tries[i]);
        if(exists(path)){snprintf(a->asset_dir,sizeof(a->asset_dir),"%s",tries[i]);return true;}
    }
    char *base=SDL_GetBasePath();
    if(base) {
        const char *rel[]={"../assets","../../assets","../share/mustardtube",NULL};
        for(int i=0;rel[i];i++) {
            snprintf(path,sizeof(path),"%s%s/themes/midnight.theme",base,rel[i]);
            if(exists(path)) {snprintf(a->asset_dir,sizeof(a->asset_dir),"%s%s",base,rel[i]);SDL_free(base);return true;}
        }
        SDL_free(base);
    }
    return false;
}
static bool valid_youtube_id(const char *id) {
    if (!id || strlen(id)!=11) return false;
    for (int i=0;i<11;i++) {
        unsigned char c=(unsigned char)id[i];
        if (!isalnum(c) && c!='_' && c!='-') return false;
    }
    return true;
}
static void safe_line(char *dest,size_t cap,const char *src) {
    size_t n=0;
    if(!cap)return;
    while(src && *src && n+1<cap) {
        unsigned char c=(unsigned char)*src++;
        if(c=='\n'||c=='\r'||c=='\t')break;
        if(c<32 || c==127)continue;
        dest[n++]=(char)c;
    }
    dest[n]='\0';
}
static bool read_result_line(const char *src,YoutubeResult *out) {
    char line[700], *parts[4]={0};
    safe_line(line,sizeof(line),src);
    char *p=line;
    for(int i=0;i<3;i++){
        char *sep=strstr(p,"|||");
        if(!sep)return false;
        *sep='\0';parts[i]=p;p=sep+3;
    }
    parts[3]=p;
    if(!valid_youtube_id(parts[0]))return false;
    memset(out,0,sizeof(*out));
    safe_line(out->id,sizeof(out->id),parts[0]);
    safe_line(out->title,sizeof(out->title),parts[1]);
    safe_line(out->channel,sizeof(out->channel),parts[2]);
    safe_line(out->duration,sizeof(out->duration),parts[3]);
    if(!out->title[0])snprintf(out->title,sizeof(out->title),"Untitled video");
    if(!out->channel[0])snprintf(out->channel,sizeof(out->channel),"YouTube");
    return true;
}
static void init_data_dir(App*a) {
    const char *dir=getenv("MUSTARDTUBE_DATA_DIR");
    if(dir && *dir)snprintf(a->data_dir,sizeof(a->data_dir),"%s",dir);
    else snprintf(a->data_dir,sizeof(a->data_dir),"%s",a->pref_dir);
}
static bool write_data_file(App*a,const char *name,const char *data) {
    if(!a->data_dir[0])return false;
    char path[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/%s",a->data_dir,name)>=(int)sizeof(path))return false;
    FILE *f=fopen(path,"w");if(!f)return false;
    bool ok=fputs(data,f)!=EOF;
    if(fclose(f)!=0)ok=false;
    return ok;
}
static void set_page(App*a,Page p);
/* The mixer is applied by the bundled short-lived Node controller, which
 * validates its own FFplay process identity before calling wpctl --pid. */
static void radio_mix_load(App*a){
    a->radio_mix_volume=75;
    char path[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/radio-mix-volume.txt",a->data_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");
    if(f){int n=-1;char rest=0;
        if(fscanf(f,"%d%c",&n,&rest)>=1 && n>=0 && n<=100)a->radio_mix_volume=n;
        fclose(f);
    }
}
static void game_mix_load(App*a){
    a->game_mix_volume=100;
    char path[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/game-mix-volume.txt",a->data_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f)return;
    int n=-1;char tail='\0';
    if(fscanf(f,"%d%c",&n,&tail)>=1 && n>=0 && n<=100 &&
       (tail=='\n'||tail=='\0'))a->game_mix_volume=n;
    fclose(f);
}
static void radio_hotkeys_load(App*a){
    a->radio_hotkeys_mode=0;
    char path[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/radio-hotkeys-enabled.txt",a->data_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f)return;
    int choice=fgetc(f);fclose(f);
    a->radio_hotkeys_mode=choice=='2'?2:choice=='1'?1:0;
}
static bool radio_hotkeys_toggle(App*a){
    const char*base=getenv("MUSTARDTUBE_APP_DIR");
    if(!base || !*base || strlen(base)>PATH_MAX-65)return false;
    char node[PATH_MAX],script[PATH_MAX];
    snprintf(node,sizeof(node),"%s/bin/node",base);
    snprintf(script,sizeof(script),"%s/scripts/background-radio.js",base);
    if(access(node,X_OK)!=0 || access(script,R_OK)!=0)return false;
    int next=(a->radio_hotkeys_mode+1)%3;
    char mode[2]={(char)('0'+next),0};
    pid_t pid=fork();
    if(pid<0)return false;
    if(pid==0){
        execl(node,node,script,"set-hotkeys",mode,(char*)NULL);
        _exit(127);
    }
    int status=0;pid_t result;
    do{result=waitpid(pid,&status,0);}while(result<0 && errno==EINTR);
    if(result<0 || !WIFEXITED(status))return false;
    if(WEXITSTATUS(status)!=0 && WEXITSTATUS(status)!=4)return false;
    a->radio_hotkeys_mode=next;
    if(WEXITSTATUS(status)==4){
        snprintf(a->status_message,sizeof(a->status_message),
                 "Mode saved; game sound restoration still pending.");
        snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),
                 "Game sound restore pending; check mixer state.");
    }else if(a->page==RADIO_NOW){
        a->radio_now_notice[0]=0;
    }
    a->dirty=true;
    return true;
}
static bool game_mix_set(App*a,int level){
    if(level<0||level>100)return false;
    const char*base=getenv("MUSTARDTUBE_APP_DIR");
    if(!base||!*base||strlen(base)>PATH_MAX-65)return false;
    char helper[PATH_MAX],value[12];
    snprintf(helper,sizeof(helper),"%s/bin/radio-hotkeys",base);
    snprintf(value,sizeof(value),"%d",level);
    if(access(helper,X_OK)!=0)return false;
    pid_t pid=fork();if(pid<0)return false;
    if(pid==0){execl(helper,helper,"--set-game-mix",base,value,(char*)NULL);_exit(127);}
    int status=0;pid_t rc;
    do{rc=waitpid(pid,&status,0);}while(rc<0&&errno==EINTR);
    if(rc<0||!WIFEXITED(status) || (WEXITSTATUS(status)!=0&&WEXITSTATUS(status)!=4))return false;
    a->game_mix_volume=level;a->dirty=true;
    return true;
}
static bool game_mix_adjust(App*a,int direction){
    game_mix_load(a);
    int level=a->game_mix_volume+direction*5;
    if(level<0)level=0;
    if(level>100)level=100;
    return level==a->game_mix_volume || game_mix_set(a,level);
}
static bool radio_mix_set(App*a,int level){
    if(level<0||level>100)return false;
    radio_mix_load(a);
    if(level==a->radio_mix_volume)return true;
    /* The original radio control runs the same Node controller as before. */
    const char*base=getenv("MUSTARDTUBE_APP_DIR");
    if(!base || !*base || strlen(base)>PATH_MAX-65)return false;
    char node[PATH_MAX],script[PATH_MAX],value[12];
    snprintf(node,sizeof(node),"%s/bin/node",base);
    snprintf(script,sizeof(script),"%s/scripts/background-radio.js",base);
    snprintf(value,sizeof(value),"%d",level);
    if(access(node,X_OK)!=0 || access(script,R_OK)!=0)return false;
    pid_t pid=fork();if(pid<0)return false;
    if(pid==0){execl(node,node,script,"set-volume",value,(char*)NULL);_exit(127);}
    int status=0;pid_t rc;
    do{rc=waitpid(pid,&status,0);}while(rc<0&&errno==EINTR);
    if(rc<0||!WIFEXITED(status))return false;
    if(WEXITSTATUS(status)!=0&&WEXITSTATUS(status)!=4)return false;
    a->radio_mix_volume=level;a->dirty=true;return true;
}
static bool radio_mix_adjust(App*a,int direction){
    radio_mix_load(a);
    int level=a->radio_mix_volume+direction*5;
    if(level<0)level=0;
    if(level>100)level=100;
    return radio_mix_set(a,level);
}

/* Signed URLs never enter SDL. The worker writes an ID/time readiness marker. */
static bool stream_is_prepared(App*a) {
    if(!a->current_is_youtube || !valid_youtube_id(a->current_video.id))return false;
    char path[PATH_MAX],id[32]={0};long when=0;int quality=0;
    if(snprintf(path,sizeof(path),"%s/prefetch-ready.txt",a->data_dir)>=(int)sizeof(path))return false;
    FILE *f=fopen(path,"r");if(!f)return false;
    bool ok=fgets(id,sizeof(id),f)!=NULL && fscanf(f,"%ld%d",&when,&quality)==2;
    fclose(f);if(!ok)return false;
    id[strcspn(id,"\r\n")]=0;
    time_t now=time(NULL);
    return strcmp(id,a->current_video.id)==0 && when>now && quality==a->quality;
}
static bool stream_has_failed(App*a) {
    char path[PATH_MAX],id[32]={0};
    if(snprintf(path,sizeof(path),"%s/prefetch-failed.txt",a->data_dir)>=(int)sizeof(path))return false;
    FILE *f=fopen(path,"r");if(!f)return false;
    bool ok=fgets(id,sizeof(id),f)!=NULL;fclose(f);
    id[strcspn(id,"\r\n")]=0;
    return ok && strcmp(id,a->current_video.id)==0;
}
static void drop_thumbnail(App*a) {
    if(a->thumbnail){SDL_DestroyTexture(a->thumbnail);a->thumbnail=NULL;}
    a->thumbnail_id[0]=0;
}
static void try_thumbnail(App*a) {
    if(!a->r || !a->current_is_youtube || a->thumbnail || !valid_youtube_id(a->current_video.id))return;
    char name[128];
    snprintf(name,sizeof(name),"/tmp/mustardtube-thumb-%s.jpg",a->current_video.id);
    if(access(name,R_OK)!=0)return;
    SDL_Surface *surf=IMG_Load(name);if(!surf)return;
    /* A YouTube mqdefault thumbnail is normally 320x180. Reject weird dimensions. */
    if(surf->w>=100 && surf->w<=640 && surf->h>=50 && surf->h<=480){
        a->thumbnail=SDL_CreateTextureFromSurface(a->r,surf);
        if(a->thumbnail){snprintf(a->thumbnail_id,sizeof(a->thumbnail_id),"%s",a->current_video.id);a->dirty=true;}
    }
    SDL_FreeSurface(surf);
}
/* Country radio: one Node worker only while Radio is open; no streaming audio or decode in UI. */
static bool radio_url_ok(const char*u) {
    if(!u || strlen(u)>740 || !(strncmp(u,"https://",8)==0 || strncmp(u,"http://",7)==0))return false;
    const char*h=strstr(u,"://")+3;
    if(!*h || *h=='/' || *h=='@' || strchr(h,'#'))return false;
    for(const char*p=u;*p;p++)if((unsigned char)*p<33 || *p=='|' || *p=='\\')return false;
    const char*end=h+strcspn(h,"/:?");size_t n=(size_t)(end-h);
    if(!n||n>=240)return false;
    char host[240];memcpy(host,h,n);host[n]=0;
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)host[i];
        if(c=='@'||c=='['||c==']'||c=='%')return false;
        host[i]=(char)tolower(c);
    }
    if(!strchr(host,'.')||!strcmp(host,"localhost"))return false;
    const char*blocked[]={".local",".internal",".localhost",".home",".lan",NULL};
    for(int i=0;blocked[i];i++){
        size_t sl=strlen(blocked[i]);if(n>=sl && !strcmp(host+n-sl,blocked[i]))return false;
    }
    struct in_addr addr;
    if(inet_pton(AF_INET,host,&addr)==1){
        uint32_t ip=ntohl(addr.s_addr);uint32_t a=ip>>24,b=(ip>>16)&255;
        if(a==0||a==10||a==127||a>=224||(a==169&&b==254)||(a==172&&b>=16&&b<=31)||
           (a==192&&b==168)||(a==100&&b>=64&&b<=127)||(a==192&&b==0)||
           (a==198&&b>=18&&b<=19))return false;
    }
    return true;
}
static bool radio_parse_station(char *line,RadioStation*o){
    char *parts[6];int n=0;
    char *save=NULL;
    for(char*t=strtok_r(line,"|",&save);t;t=strtok_r(NULL,"|",&save)){
        if(n>=6)return false;
        parts[n++]=t;
    }
    if(n!=5 && n!=6)return false;
    for(int i=0;i<n;i++){parts[i][strcspn(parts[i],"\r\n")]=0;if(!*parts[i])return false;}
    if(strlen(parts[0])>40 || strlen(parts[4])!=2 || !radio_url_ok(parts[3]))return false;
    memset(o,0,sizeof(*o));
    safe_line(o->id,sizeof(o->id),parts[0]);safe_line(o->name,sizeof(o->name),parts[1]);
    safe_line(o->codec,sizeof(o->codec),parts[2]);safe_line(o->url,sizeof(o->url),parts[3]);
    safe_line(o->country,sizeof(o->country),parts[4]);
    if(n==6 && strcmp(parts[5],"-")!=0) safe_line(o->logo,sizeof(o->logo),parts[5]);
    return o->name[0] && o->url[0];
}
/* Sorting occurs after each bounded fetch as well as in the directory backend.
 * Country order never depends on station popularity; station/favourite order
 * is name-first with a stable ID tie-breaker. No additional allocations. */
static int compare_country(const void*left,const void*right){
    const RadioCountry*a=left,*b=right;
    int n=strcasecmp(a->name,b->name);
    return n?n:strcmp(a->code,b->code);
}
static int compare_station(const void*left,const void*right){
    const RadioStation*a=left,*b=right;
    int n=strcasecmp(a->name,b->name);
    return n?n:strcmp(a->id,b->id);
}
static void radio_load_countries(App*a){
    char path[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/radio-countries.tsv",a->data_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f)return;
    int n=0;char line[350];
    while(n<RADIO_MAX_COUNTRIES&&fgets(line,sizeof(line),f)){
        char code[8]={0},name[100]={0};int count=0;
        if(sscanf(line,"%7[^|]|||%99[^|]|||%d",code,name,&count)!=3)continue;
        if(strlen(code)!=2||count<=0)continue;
        snprintf(a->countries[n].code,sizeof(a->countries[n].code),"%s",code);
        safe_line(a->countries[n].name,sizeof(a->countries[n].name),name);
        a->countries[n].count=count;n++;
    }
    fclose(f);qsort(a->countries,(size_t)n,sizeof(a->countries[0]),compare_country);a->country_count=n;
    if(a->country_idx>=n)a->country_idx=0;a->dirty=true;
}
static void radio_load_stations(App*a){
    char selected_id[sizeof(((RadioStation*)0)->id)]={0};
    if(a->station_count>0 && a->station_idx>=0 && a->station_idx<a->station_count)
        snprintf(selected_id,sizeof(selected_id),"%s",a->stations[a->station_idx].id);
    char path[PATH_MAX];if(snprintf(path,sizeof(path),"%s/radio-stations.tsv",a->data_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f)return;
    RadioStation *list=NULL;size_t capacity=0;
    int n=0;bool allocation_failed=false;char line[2150];
    while(fgets(line,sizeof(line),f)){
        RadioStation st;if(!radio_parse_station(line,&st))continue;
        if(strcmp(st.country,a->radio_selected_country))continue;
        if((size_t)n==capacity){
            size_t next=capacity?capacity*2:64;
            if(next<capacity || next>SIZE_MAX/sizeof(*list) || next>(size_t)INT_MAX){allocation_failed=true;break;}
            RadioStation*grown=realloc(list,next*sizeof(*list));
            if(!grown){allocation_failed=true;break;}
            list=grown;capacity=next;
        }
        list[n++]=st;
    }
    fclose(f);
    if(allocation_failed){
        free(list);
        snprintf(a->radio_status,sizeof(a->radio_status),"Not enough memory to load this station directory.");
        a->dirty=true;return;
    }
    if(n>1)qsort(list,(size_t)n,sizeof(*list),compare_station);
    free(a->stations);a->stations=list;a->station_capacity=capacity;
    a->station_count=n;
    if(selected_id[0])for(int i=0;i<n;i++)if(!strcmp(selected_id,a->stations[i].id)){a->station_idx=i;break;}
    if(a->station_idx>=n)a->station_idx=0;
    /* The next API offset must not be inferred from filtered/deduplicated rows. */
    a->radio_next_offset=((n+19)/20)*20;a->radio_has_more=true;
    if(snprintf(path,sizeof(path),"%s/radio-pagination.tsv",a->data_dir)<(int)sizeof(path)){
        FILE *meta=fopen(path,"r");if(meta){
            char country[4]={0};int next=0,more=0;
            if(fscanf(meta,"%3[^|]|||%d|||%d",country,&next,&more)==3 &&
               !strcmp(country,a->radio_selected_country) && next>=20 && next%20==0){
                a->radio_next_offset=next;a->radio_has_more=(more!=0);
            }
            fclose(meta);
        }
    }
    a->dirty=true;
}
static void radio_load_favourites(App*a){
    char path[PATH_MAX];if(snprintf(path,sizeof(path),"%s/radio-favourites.tsv",a->data_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f){a->favourite_count=0;return;}
    int n=0;char line[2150];
    while(n<RADIO_MAX_FAVORITES && fgets(line,sizeof(line),f)){
        RadioStation st;if(radio_parse_station(line,&st))a->favourites[n++]=st;
    }
    fclose(f);qsort(a->favourites,(size_t)n,sizeof(a->favourites[0]),compare_station);
    a->favourite_count=n;if(a->favourite_idx>=n)a->favourite_idx=0;a->dirty=true;
}
static void radio_save_favourites(App*a){
    char path[PATH_MAX],tmp[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/radio-favourites.tsv",a->data_dir)>=(int)sizeof(path) ||
       snprintf(tmp,sizeof(tmp),"%s/radio-favourites.tmp",a->data_dir)>=(int)sizeof(tmp))return;
    FILE*f=fopen(tmp,"w");if(!f)return;
    for(int i=0;i<a->favourite_count;i++){
        const RadioStation*v=&a->favourites[i];
        fprintf(f,"%s|||%s|||%s|||%s|||%s|||%s\n",v->id,v->name,v->codec,v->url,v->country,v->logo[0]?v->logo:"-");
    }
    if(fclose(f)==0)rename(tmp,path);else unlink(tmp);
}

static bool radio_logo_id_ok(const char*id){
    size_t n=strlen(id);if(n<16||n>40)return false;
    for(size_t i=0;i<n;i++)if(!isxdigit((unsigned char)id[i])&&id[i]!='-')return false;
    return true;
}
static void drop_radio_logos(App*a){
    for(int i=0;i<7;i++){
        if(a->radio_logos[i])SDL_DestroyTexture(a->radio_logos[i]);
        a->radio_logos[i]=NULL;a->radio_logo_ids[i][0]=0;
    }
}
static void stop_radio_logo_worker(App*a){
    if(a->radio_logo_pid>0){
        kill(-a->radio_logo_pid,SIGKILL);kill(a->radio_logo_pid,SIGKILL);
        while(waitpid(a->radio_logo_pid,NULL,0)<0&&errno==EINTR){}
        a->radio_logo_pid=0;
    }
}
static void start_radio_logo_worker(App*a,bool force){
    if(a->page!=RADIO||a->radio_mode==0||a->radio_loading)return;
    int idx=a->radio_mode==1?a->station_idx:a->favourite_idx;
    int first=(idx/7)*7;
    if(!force && a->radio_logo_mode==a->radio_mode && a->radio_logo_first==first
       && !strcmp(a->radio_logo_country,a->radio_selected_country))return;
    stop_radio_logo_worker(a);drop_radio_logos(a);
    a->radio_logo_mode=a->radio_mode;a->radio_logo_first=first;
    snprintf(a->radio_logo_country,sizeof(a->radio_logo_country),"%s",a->radio_selected_country);
    const char*app=getenv("MUSTARDTUBE_APP_DIR");if(!app)return;
    char node[PATH_MAX],script[PATH_MAX],index[20];
    if(snprintf(node,sizeof(node),"%s/bin/node",app)>=(int)sizeof(node)||
       snprintf(script,sizeof(script),"%s/scripts/radio-backend.js",app)>=(int)sizeof(script))return;
    if(access(node,X_OK)!=0||access(script,R_OK)!=0)return;
    pid_t child=fork();if(child<0)return;
    if(child>0){setpgid(child,child);a->radio_logo_pid=child;return;}
    setpgid(0,0);int fd=open("/dev/null",O_WRONLY);
    if(fd>=0){dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);if(fd>2)close(fd);}
    snprintf(index,sizeof(index),"%d",first);
    execl(node,"node",script,"logos",a->radio_mode==1?"stations":"favourites",
          a->radio_selected_country[0]?a->radio_selected_country:"US",index,(char*)NULL);
    _exit(127);
}
static SDL_Texture*radio_station_logo(App*a,int visible_index,const RadioStation*st){
    if(visible_index<0||visible_index>=7||!st||!radio_logo_id_ok(st->id))return NULL;
    if(strcmp(a->radio_logo_ids[visible_index],st->id)){
        if(a->radio_logos[visible_index])SDL_DestroyTexture(a->radio_logos[visible_index]);
        a->radio_logos[visible_index]=NULL;
        snprintf(a->radio_logo_ids[visible_index],sizeof(a->radio_logo_ids[visible_index]),"%s",st->id);
    }
    if(a->radio_logos[visible_index])return a->radio_logos[visible_index];
    char path[PATH_MAX];
    for(int i=0;i<2;i++){
        if(snprintf(path,sizeof(path),"%s/radio-logo-cache/%s.%s",a->data_dir,st->id,i==0?"png":"jpg")>=(int)sizeof(path))continue;
        if(access(path,R_OK)!=0)continue;
        SDL_Surface*surface=IMG_Load(path);
        if(surface){
            if(surface->w>=8&&surface->h>=8&&surface->w<=128&&surface->h<=128)
               a->radio_logos[visible_index]=SDL_CreateTextureFromSurface(a->r,surface);
            SDL_FreeSurface(surface);
        }
        break;
    }
    return a->radio_logos[visible_index];
}
static void radio_draw_country_flag(App*a,const char*code,int x,int y){
    if(!code||strlen(code)!=2||code[0]<'A'||code[0]>'Z'||code[1]<'A'||code[1]>'Z')return;
    if(!a->radio_flag_atlas){
        char path[PATH_MAX];
        if(snprintf(path,sizeof(path),"%s/flags/country-atlas.png",a->asset_dir)<(int)sizeof(path)){
            SDL_Surface*surface=IMG_Load(path);
            if(surface){a->radio_flag_atlas=SDL_CreateTextureFromSurface(a->r,surface);SDL_FreeSurface(surface);}
        }
    }
    if(a->radio_flag_atlas){
        for(int i=0;RADIO_FLAG_CODES[2*i]&&RADIO_FLAG_CODES[2*i+1];i++){
            if(RADIO_FLAG_CODES[2*i]==code[0]&&RADIO_FLAG_CODES[2*i+1]==code[1]){
                SDL_Rect src={(i%16)*32,(i/16)*23,32,23},dst={x,y,32,23};
                ui_draw_texture(a,a->radio_flag_atlas,&src,&dst);return;
            }
        }
    }
    print_text(a,FS_11,code,x+4,y+3,a->theme.secondary);
}

static bool radio_is_favourite(App*a,const RadioStation*st){
    for(int i=0;i<a->favourite_count;i++)if(!strcmp(a->favourites[i].id,st->id))return true;
    return false;
}
static void radio_toggle_favourite(App*a,const RadioStation*st){
    for(int i=0;i<a->favourite_count;i++)if(!strcmp(a->favourites[i].id,st->id)){
        for(int j=i+1;j<a->favourite_count;j++)a->favourites[j-1]=a->favourites[j];
        a->favourite_count--;qsort(a->favourites,(size_t)a->favourite_count,sizeof(a->favourites[0]),compare_station);radio_save_favourites(a);a->dirty=true;return;
    }
    if(a->favourite_count>=RADIO_MAX_FAVORITES){snprintf(a->radio_status,sizeof(a->radio_status),"Favourites full (32 stations).");a->dirty=true;return;}
    a->favourites[a->favourite_count++]=*st;qsort(a->favourites,(size_t)a->favourite_count,sizeof(a->favourites[0]),compare_station);radio_save_favourites(a);a->dirty=true;
}
static void stop_radio_worker(App*a){
    if(a->radio_worker_pid>0){kill(-a->radio_worker_pid,SIGKILL);kill(a->radio_worker_pid,SIGKILL);
        while(waitpid(a->radio_worker_pid,NULL,0)<0&&errno==EINTR){}a->radio_worker_pid=0;}
    a->radio_loading=false;
}
static void radio_refresh(App*a,bool is_country,int offset){
    const char*app=getenv("MUSTARDTUBE_APP_DIR");if(!app||!*app||!a->data_dir[0])return;
    if(!is_country && !a->radio_selected_country[0])return;
    stop_radio_worker(a);
    char node[PATH_MAX],script[PATH_MAX],off[20];
    if(snprintf(node,sizeof(node),"%s/bin/node",app)>=(int)sizeof(node)||
       snprintf(script,sizeof(script),"%s/scripts/radio-backend.js",app)>=(int)sizeof(script))return;
    if(access(node,X_OK)!=0 || access(script,R_OK)!=0){
        snprintf(a->radio_status,sizeof(a->radio_status),"Radio backend unavailable.");a->dirty=true;return;}
    pid_t child=fork();if(child<0){snprintf(a->radio_status,sizeof(a->radio_status),"Cannot start radio directory.");a->dirty=true;return;}
    if(child>0){setpgid(child,child);a->radio_worker_pid=child;a->radio_loading=true;a->dirty=true;return;}
    setpgid(0,0);int fd=open("/dev/null",O_WRONLY);
    if(fd>=0){dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);if(fd>2)close(fd);}
    if(is_country)execl(node,"node",script,"countries",(char*)NULL);
    snprintf(off,sizeof(off),"%d",offset);
    execl(node,"node",script,"stations",a->radio_selected_country,off,(char*)NULL);
    _exit(127);
}
static void radio_poll(App*a){
    if(a->radio_worker_pid<=0)return;
    int code=0;pid_t r=waitpid(a->radio_worker_pid,&code,WNOHANG);
    if(r==0)return;
    a->radio_worker_pid=0;a->radio_loading=false;
    if(a->radio_mode==0)radio_load_countries(a);
    else {radio_load_stations(a);start_radio_logo_worker(a,true);}
    if(!WIFEXITED(code)||WEXITSTATUS(code)!=0){
        snprintf(a->radio_status,sizeof(a->radio_status),"Offline or directory unavailable. Cached list retained.");
    }else{
        snprintf(a->radio_status,sizeof(a->radio_status),"Loaded station directory. Press A to select.");
    }
    a->dirty=true;
}
static void radio_open(App*a){
    a->radio_mode=0;radio_load_countries(a);radio_load_favourites(a);
    /* These markers contain only a station name/notice, never its stream URL.
       The supervisor validates the persisted player's PID on every entry. */
    char path[PATH_MAX];
    a->radio_background_name[0]=0;
    if(snprintf(path,sizeof(path),"%s/radio-background-name.txt",a->data_dir)<(int)sizeof(path)){
        FILE*f=fopen(path,"r");if(f){char line[160]={0};
            if(fgets(line,sizeof(line),f))safe_line(a->radio_background_name,sizeof(a->radio_background_name),line);
            fclose(f);
        }
    }
    if(snprintf(path,sizeof(path),"%s/radio-background-notice.txt",a->data_dir)<(int)sizeof(path)){
        FILE*f=fopen(path,"r");if(f){char line[160]={0};
            if(fgets(line,sizeof(line),f))safe_line(a->radio_status,sizeof(a->radio_status),line);
            fclose(f);unlink(path);
        }
    }
    if(!a->country_count)radio_refresh(a,true,0);
}
/* Only public directory metadata is persisted for the Now Playing screen.
 * In particular, the transient stream URL is consumed and removed by the
 * short-lived Node controller; this metadata never contains a URL. */
static bool radio_remember_display(App*a,const RadioStation*st){
    char meta[350];
    if(snprintf(meta,sizeof(meta),"%s\n%s\n%s\n%s\n",
        st->id,st->country,st->codec,st->name)>=(int)sizeof(meta))return false;
    return write_data_file(a,"radio-now-meta.txt",meta);
}
/* A read-only liveness check costs no resident service or radio restart.
 * A stale name marker alone is insufficient evidence that radio is active. */
static bool radio_now_process_alive(App*a){
    char path[PATH_MAX],buf[1024]={0},proc[80],exe[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/radio-background-state.json",a->data_dir)>=(int)sizeof(path))return false;
    FILE*f=fopen(path,"r");if(!f)return false;
    size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;
    const char*where=strstr(buf,"\"pid\":");if(!where)return false;
    char*end=NULL;long pid=strtol(where+6,&end,10);
    if(end==where+6 || pid<=1 || pid>INT_MAX)return false;
    if(snprintf(proc,sizeof(proc),"/proc/%ld/exe",pid)>=(int)sizeof(proc))return false;
    ssize_t len=readlink(proc,exe,sizeof(exe)-1);
    if(len<=0)return false;
    exe[len]=0;
    const char*base=getenv("MUSTARDTUBE_APP_DIR");
    if(!base||!*base)return false;
    char expected[PATH_MAX];
    if(snprintf(expected,sizeof(expected),"%s/bin/ffplay-radio",base)>=(int)sizeof(expected))return false;
    char resolved[PATH_MAX];
    if(!realpath(expected,resolved))return false;
    return strcmp(exe,resolved)==0;
}
static void radio_now_load(App*a){
    a->radio_now_active=false;
    a->radio_now_blink=true;
    a->radio_now_logo_checked=false;
    a->radio_now_last_tick=SDL_GetTicks();
    a->radio_background_name[0]=0;
    a->radio_now_notice[0]=0;
    memset(&a->radio_now_station,0,sizeof(a->radio_now_station));
    char path[PATH_MAX],line[200];
    if(snprintf(path,sizeof(path),"%s/radio-now-meta.txt",a->data_dir)<(int)sizeof(path)){
        FILE*f=fopen(path,"r");
        if(f){
            if(fgets(line,sizeof(line),f))safe_line(a->radio_now_station.id,sizeof(a->radio_now_station.id),line);
            if(fgets(line,sizeof(line),f))safe_line(a->radio_now_station.country,sizeof(a->radio_now_station.country),line);
            if(fgets(line,sizeof(line),f))safe_line(a->radio_now_station.codec,sizeof(a->radio_now_station.codec),line);
            if(fgets(line,sizeof(line),f))safe_line(a->radio_now_station.name,sizeof(a->radio_now_station.name),line);
            fclose(f);
        }
    }
    if(snprintf(path,sizeof(path),"%s/radio-background-name.txt",a->data_dir)<(int)sizeof(path)){
        FILE*f=fopen(path,"r");
        if(f){
            if(fgets(line,sizeof(line),f))safe_line(a->radio_background_name,sizeof(a->radio_background_name),line);
            fclose(f);
        }
    }
    a->radio_now_active=(a->radio_background_name[0]!=0 && radio_now_process_alive(a));
    if(a->radio_now_active && !a->radio_now_station.name[0])
        snprintf(a->radio_now_station.name,sizeof(a->radio_now_station.name),"%s",a->radio_background_name);
    if(snprintf(path,sizeof(path),"%s/radio-background-notice.txt",a->data_dir)<(int)sizeof(path)){
        FILE*f=fopen(path,"r");
        if(f){
            if(fgets(line,sizeof(line),f))safe_line(a->radio_now_notice,sizeof(a->radio_now_notice),line);
            fclose(f);unlink(path);
        }
    }
    radio_mix_load(a);
    a->dirty=true;
}
/* The station directory and favourites cache already contain validated URLs.
 * Never create a second persistent file containing a stream URL for fallback. */
static bool radio_now_find_cached(App*a,RadioStation*out){
    if(!a->radio_now_station.id[0])return false;
    radio_load_favourites(a);
    for(int i=0;i<a->favourite_count;i++)
        if(!strcmp(a->favourites[i].id,a->radio_now_station.id)){
            *out=a->favourites[i];return radio_url_ok(out->url);
        }
    if(strlen(a->radio_now_station.country)==2){
        snprintf(a->radio_selected_country,sizeof(a->radio_selected_country),"%s",a->radio_now_station.country);
        radio_load_stations(a);
        for(int i=0;i<a->station_count;i++)
            if(!strcmp(a->stations[i].id,a->radio_now_station.id)){
                *out=a->stations[i];return radio_url_ok(out->url);
            }
    }
    return false;
}
static bool radio_pick_station(App*a,const RadioStation*st){
    if(!st || !radio_url_ok(st->url))return false;
    char selected[1100];
    if(snprintf(selected,sizeof(selected),"%s\n%s\n",st->name,st->url)>=(int)sizeof(selected))return false;
    if(!write_data_file(a,"radio-selected.txt",selected))return false;
    write_data_file(a,"radio-last-country.txt",st->country);
    a->radio_handoff=true;a->radio_handoff_kind=RADIO_PLAY_EXIT;a->running=false;return true;
}
static bool radio_start_native_station(App*a,const RadioStation*st){
    if(!st || !radio_url_ok(st->url))return false;
    char selected[1100];
    if(snprintf(selected,sizeof(selected),"%s\n%s\n",st->name,st->url)>=(int)sizeof(selected))return false;
    if(!radio_remember_display(a,st) || !write_data_file(a,"radio-selected.txt",selected))return false;
    write_data_file(a,"radio-last-country.txt",st->country);
    a->radio_handoff=true;a->radio_handoff_kind=RADIO_NATIVE_START_EXIT;
    a->running=false;return true;
}
static bool radio_start_background(App*a){
    if(a->page!=RADIO)return false;
    if(a->radio_mode==0 && a->radio_background_name[0]){
        set_page(a,RADIO_NOW);return true;
    }
    if(a->radio_mode==0)return false;
    const RadioStation*st=NULL;
    if(a->radio_mode==1 && a->station_count>0)st=&a->stations[a->station_idx];
    else if(a->radio_mode==2 && a->favourite_count>0)st=&a->favourites[a->favourite_idx];
    if(!st || !radio_url_ok(st->url))return false;
    char selected[1100];
    if(snprintf(selected,sizeof(selected),"%s\n%s\n",st->name,st->url)>=(int)sizeof(selected) ||
       !radio_remember_display(a,st) || !write_data_file(a,"radio-selected.txt",selected))return false;
    write_data_file(a,"radio-last-country.txt",st->country);
    a->radio_handoff=true;a->radio_handoff_kind=RADIO_BACKGROUND_START_EXIT;
    a->running=false;return true;
}
static void radio_stop_background(App*a){
    if(a->page!=RADIO && a->page!=RADIO_NOW)return;
    a->radio_handoff=true;a->radio_handoff_kind=a->page==RADIO_NOW?RADIO_NATIVE_STOP_EXIT:RADIO_BACKGROUND_STOP_EXIT;
    a->running=false;
}
static void radio_now_reconnect(App*a,bool wasabi){
    RadioStation st={0};
    if(!radio_now_find_cached(a,&st)){
        snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Station not in local directory. Re-select from Radio.");
        a->dirty=true;return;
    }
    if(wasabi){
        if(!radio_pick_station(a,&st)){
            snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Unable to launch Wasabi fallback.");
            a->dirty=true;
        }
    }else if(!radio_start_native_station(a,&st)){
        snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Unable to reconnect. Select the station again.");
        a->dirty=true;
    }
}
static void stop_prefetch(App*a) {
    if(a->worker_pid>0){
        /* Only our unreaped child owns this process group. Kill its extractor,
           thumbnail downloader and timers too; never use a persisted PID. */
        kill(-a->worker_pid,SIGKILL);
        kill(a->worker_pid,SIGKILL);
        while(waitpid(a->worker_pid,NULL,0)<0 && errno==EINTR){}
        a->worker_pid=0;
    }
    a->worker_id[0]=0;

}
/* Home discovery is independent from playback and cannot block the UI. */
static void stop_home_worker(App*a) {
    if(a->home_worker_pid>0){
        kill(-a->home_worker_pid,SIGKILL);
        kill(a->home_worker_pid,SIGKILL);
        while(waitpid(a->home_worker_pid,NULL,0)<0 && errno==EINTR){}
        a->home_worker_pid=0;
    }
    a->home_loading=false;
}
static void start_home_refresh(App*a,bool force) {
    const char *dir=getenv("MUSTARDTUBE_APP_DIR");
    if(!dir || !*dir || !a->data_dir[0])return;
    bool fresh=!force && a->home_count>0 && a->home_cache_stamp>0 &&
       time(NULL)>=a->home_cache_stamp && time(NULL)-a->home_cache_stamp<HOME_CACHE_TTL;
    bool missing=false;
    if(fresh)for(int i=0;i<a->home_count;i++){
        char img[128];snprintf(img,sizeof(img),"/tmp/mustardtube-thumb-%s.jpg",a->home_videos[i].id);
        if(access(img,R_OK)!=0){missing=true;break;}
    }
    if(fresh&&!missing)return;
    stop_home_worker(a);
    char script[PATH_MAX];
    if(snprintf(script,sizeof(script),"%s/scripts/youtube-backend.sh",dir)>=(int)sizeof(script))return;
    if(access(script,R_OK)!=0)return;
    pid_t child=fork();
    if(child<0){snprintf(a->home_feedback,sizeof(a->home_feedback),"Cannot start Home refresh.");a->dirty=true;return;}
    if(child>0){
        setpgid(child,child);
        a->home_worker_pid=child;a->home_loading=true;a->dirty=true;
        return;
    }
    setpgid(0,0);
    int fd=open("/dev/null",O_WRONLY);
    if(fd>=0){dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);if(fd>STDERR_FILENO)close(fd);}
    execl("/bin/sh","sh",script,fresh?"home-thumbs":"home",home_category_keys[a->home_category],(char*)NULL);
    _exit(127);
}
static void drop_home_thumbnails(App*a) {
    for(int i=0;i<HOME_VIDEOS;i++){
        if(a->home_thumbs[i])SDL_DestroyTexture(a->home_thumbs[i]);
        a->home_thumbs[i]=NULL;
    }
}
static void load_home_file(App*a) {
    char path[PATH_MAX];
    if(!a->data_dir[0])return;
    if(snprintf(path,sizeof(path),"%s/home-%s-updated.txt",a->data_dir,home_category_keys[a->home_category])>=(int)sizeof(path))return;
    time_t stamp=0;
    if(a->home_category==0 && access(path,R_OK)!=0)
        snprintf(path,sizeof(path),"%s/home-updated.txt",a->data_dir);
    FILE *timefile=fopen(path,"r");
    if(timefile){long long n=0;if(fscanf(timefile,"%lld",&n)==1 && n>0)stamp=(time_t)n;fclose(timefile);}
    if(stamp!=a->home_cache_stamp || a->home_cache_stamp==0) {
        if(snprintf(path,sizeof(path),"%s/home-%s-results.tsv",a->data_dir,home_category_keys[a->home_category])>=(int)sizeof(path))return;
        YoutubeResult fresh[HOME_VIDEOS]={0};int count=0;
        if(a->home_category==0 && access(path,R_OK)!=0)
            snprintf(path,sizeof(path),"%s/home-results.tsv",a->data_dir);
        FILE *f=fopen(path,"r");
        if(f){char line[700];
            while(count<HOME_VIDEOS && fgets(line,sizeof(line),f)) {
                YoutubeResult v;
                if(read_result_line(line,&v))fresh[count++]=v;
            }
            fclose(f);
        }
        if(count>0){
            bool changed=count!=a->home_count;
            for(int i=0;i<count&&!changed;i++)if(strcmp(fresh[i].id,a->home_videos[i].id))changed=true;
            if(changed)drop_home_thumbnails(a);
            memcpy(a->home_videos,fresh,sizeof(fresh));
            a->home_count=count;
            if(a->home_idx>=count)a->home_idx=0;
            a->dirty=true;
        }
        a->home_cache_stamp=stamp;
    }
    if(snprintf(path,sizeof(path),"%s/home-%s-status.txt",a->data_dir,home_category_keys[a->home_category])>=(int)sizeof(path))return;
    if(a->home_category==0 && access(path,R_OK)!=0)
        snprintf(path,sizeof(path),"%s/home-status.txt",a->data_dir);
    FILE *f=fopen(path,"r");
    if(f){char line[128];if(fgets(line,sizeof(line),f))safe_line(a->home_feedback,sizeof(a->home_feedback),line);fclose(f);}
}
static void poll_home_thumbnails(App*a) {
    if(!a->r || a->page!=HOME)return;
    for(int i=0;i<a->home_count;i++){
        if(a->home_thumbs[i] || !valid_youtube_id(a->home_videos[i].id))continue;
        char path[120];
        snprintf(path,sizeof(path),"/tmp/mustardtube-thumb-%s.jpg",a->home_videos[i].id);
        if(access(path,R_OK)!=0)continue;
        SDL_Surface*surf=IMG_Load(path);
        if(!surf)continue;
        if(surf->w>=100 && surf->w<=640 && surf->h>=50 && surf->h<=480){
            a->home_thumbs[i]=SDL_CreateTextureFromSurface(a->r,surf);
            if(a->home_thumbs[i])a->dirty=true;
        }
        SDL_FreeSurface(surf);
    }
}
static void poll_home_worker(App*a) {
    if(a->home_worker_pid>0){
        int code=0;pid_t r=waitpid(a->home_worker_pid,&code,WNOHANG);
        if(r==a->home_worker_pid || (r<0 && errno==ECHILD)){
            a->home_worker_pid=0;a->home_loading=false;a->home_cache_stamp=0;a->dirty=true;
        }
    }
    load_home_file(a);
    poll_home_thumbnails(a);
}
static void home_set_category(App*a,int new_category){
    if(new_category<0||new_category>=HOME_CATEGORIES||new_category==a->home_category)return;
    stop_home_worker(a);
    drop_home_thumbnails(a);
    a->home_category=new_category;a->home_count=0;a->home_idx=0;
    a->home_cache_stamp=0;a->home_feedback[0]=0;
    load_home_file(a);
    if(a->page==HOME)start_home_refresh(a,false);
    a->dirty=true;
}
static void drop_result_thumbs(App*a){
    for(int i=0;i<RESULTS_VISIBLE;i++){
        if(a->result_thumbs[i])SDL_DestroyTexture(a->result_thumbs[i]);
        a->result_thumbs[i]=NULL;a->result_thumb_ids[i][0]=0;
    }
}
static void stop_result_thumb_worker(App*a){
    if(a->result_thumb_pid>0){
        kill(-a->result_thumb_pid,SIGKILL);kill(a->result_thumb_pid,SIGKILL);
        while(waitpid(a->result_thumb_pid,NULL,0)<0&&errno==EINTR){}
        a->result_thumb_pid=0;
    }
}
static void start_result_thumb_worker(App*a,bool force){
    if(a->page!=RESULTS||a->result_count<=0)return;
    int first=(a->result_idx/RESULTS_VISIBLE)*RESULTS_VISIBLE;
    if(!force && first==a->result_thumb_first)return;
    stop_result_thumb_worker(a);drop_result_thumbs(a);
    a->result_thumb_first=first;
    const char*dir=getenv("MUSTARDTUBE_APP_DIR");
    if(!dir||!*dir)return;
    char script[PATH_MAX],offset[16];
    if(snprintf(script,sizeof(script),"%s/scripts/youtube-backend.sh",dir)>=(int)sizeof(script))return;
    if(access(script,R_OK)!=0)return;
    snprintf(offset,sizeof(offset),"%d",first);
    pid_t child=fork();if(child<0)return;
    if(child>0){setpgid(child,child);a->result_thumb_pid=child;return;}
    setpgid(0,0);
    int fd=open("/dev/null",O_WRONLY);
    if(fd>=0){dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);if(fd>STDERR_FILENO)close(fd);}
    execl("/bin/sh","sh",script,"thumbs",offset,(char*)NULL);
    _exit(127);
}
static void poll_result_thumbnails(App*a){
    if(a->page!=RESULTS||!a->r)return;
    if(a->result_thumb_pid>0){int code=0;pid_t r=waitpid(a->result_thumb_pid,&code,WNOHANG);
        if(r==a->result_thumb_pid||(r<0&&errno==ECHILD))a->result_thumb_pid=0;
    }
    int first=(a->result_idx/RESULTS_VISIBLE)*RESULTS_VISIBLE;
    if(first!=a->result_thumb_first){start_result_thumb_worker(a,true);return;}
    for(int j=0;j<RESULTS_VISIBLE&&first+j<a->result_count;j++){
        const char*id=a->results[first+j].id;
        if(strcmp(a->result_thumb_ids[j],id)){
            if(a->result_thumbs[j])SDL_DestroyTexture(a->result_thumbs[j]);
            a->result_thumbs[j]=NULL;snprintf(a->result_thumb_ids[j],sizeof(a->result_thumb_ids[j]),"%s",id);
        }
        if(a->result_thumbs[j]||!valid_youtube_id(id))continue;
        char path[128];snprintf(path,sizeof(path),"/tmp/mustardtube-thumb-%s.jpg",id);
        if(access(path,R_OK)!=0)continue;
        SDL_Surface*surf=IMG_Load(path);if(!surf)continue;
        if(surf->w>=100&&surf->w<=640&&surf->h>=50&&surf->h<=480){
            a->result_thumbs[j]=SDL_CreateTextureFromSurface(a->r,surf);
            if(a->result_thumbs[j])a->dirty=true;
        }
        SDL_FreeSurface(surf);
    }
}
static void remove_data_file(App*a,const char*name) {
    char path[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/%s",a->data_dir,name)<(int)sizeof(path))unlink(path);
}
static void start_prefetch(App*a) {
    const char*dir=getenv("MUSTARDTUBE_APP_DIR");
    if(!dir || !*dir || !a->current_is_youtube || !valid_youtube_id(a->current_video.id))return;
    if(a->worker_pid>0 && !strcmp(a->worker_id,a->current_video.id)){
        /* WNOWAIT keeps ownership until stop_prefetch kills any descendants. */
        siginfo_t info={0};
        if(waitid(P_PID,a->worker_pid,&info,WEXITED|WNOHANG|WNOWAIT)==0 && info.si_pid==0)return;
    }
    stop_prefetch(a);
    char script[PATH_MAX],chosen[256];
    snprintf(chosen,sizeof(chosen),"%s\n%s\n",a->current_video.id,a->current_video.title);
    if(!write_data_file(a,"selected.txt",chosen))return;
    if(snprintf(script,sizeof(script),"%s/scripts/youtube-backend.sh",dir)>=(int)sizeof(script))return;
    if(access(script,R_OK)!=0)return;
    remove_data_file(a,"prefetch-ready.txt");remove_data_file(a,"prefetch-failed.txt");
    pid_t child=fork();
    if(child<0){a->stream_failed=true;a->dirty=true;return;}
    if(child>0){
        setpgid(child,child);a->worker_pid=child;
        snprintf(a->worker_id,sizeof(a->worker_id),"%s",a->current_video.id);return;
    }
    setpgid(0,0); /* Normal priority; one worker only. */
    int fd=open("/dev/null",O_WRONLY);
    if(fd>=0){dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);if(fd>STDERR_FILENO)close(fd);}
    execl("/bin/sh","sh",script,"prefetch",(char*)NULL);
    _exit(127);
}
static int next_quality(int quality) {
    return quality==360?480:quality==480?720:quality==720?1080:360;
}
static void toggle_quality(App*a) {
    if(a->page!=PLAYER || !a->current_is_youtube || a->awaiting_stream)return;
    stop_prefetch(a);
    a->quality=next_quality(a->quality);
    write_data_file(a,"quality.txt",a->quality==360?"360\n":a->quality==480?"480\n":a->quality==720?"720\n":"1080\n");
    a->stream_failed=false;start_prefetch(a);a->dirty=true;
}
static void show_video_details(App*a) {
    drop_thumbnail(a);
    a->awaiting_stream=false;a->stream_failed=false;a->preview_started=time(NULL);
    a->from_page=a->page==HOME?HOME:RESULTS;a->wasabi_missing=false;
    set_page(a,PLAYER);
    char chosen[256];
    snprintf(chosen,sizeof(chosen),"%s\n%s\n",a->current_video.id,a->current_video.title);
    if(write_data_file(a,"selected.txt",chosen) && !stream_is_prepared(a))start_prefetch(a);
}
static void load_result_file(App*a) {
    a->result_count=0;
    char path[PATH_MAX];
    if(!a->data_dir[0] || snprintf(path,sizeof(path),"%s/search-results.tsv",a->data_dir)>=(int)sizeof(path))return;
    FILE *f=fopen(path,"r");
    if(f){char line[1000];
        while(a->result_count<SEARCH_RESULTS && fgets(line,sizeof(line),f)) {
            YoutubeResult res;
            if(read_result_line(line,&res)) a->results[a->result_count++]=res;
        }
        fclose(f);
    }
    if(snprintf(path,sizeof(path),"%s/status.txt",a->data_dir)>=(int)sizeof(path))return;
    f=fopen(path,"r");
    if(f){char line[256];if(fgets(line,sizeof(line),f))safe_line(a->status_message,sizeof(a->status_message),line);fclose(f);}
    if(snprintf(path,sizeof(path),"%s/more-available.txt",a->data_dir)>=(int)sizeof(path))return;
    f=fopen(path,"r");
    a->more_available=false;
    if(f){char b[8]={0};if(fgets(b,sizeof(b),f))a->more_available=(b[0]=='1');fclose(f);}
    if(snprintf(path,sizeof(path),"%s/next-result-index.txt",a->data_dir)>=(int)sizeof(path))return;
    f=fopen(path,"r");
    if(f){int next=0;if(fscanf(f,"%d",&next)==1 && next>=0 && next<a->result_count)a->result_idx=next;fclose(f);}
    if(a->result_idx>=a->result_count)a->result_idx=0;
}
static void load_selected_video(App*a) {
    char path[PATH_MAX];
    if(!a->data_dir[0]||snprintf(path,sizeof(path),"%s/selected.txt",a->data_dir)>=(int)sizeof(path))return;
    FILE *f=fopen(path,"r");if(!f)return;
    char id[32]={0},title[256]={0};
    if(fgets(id,sizeof(id),f) && fgets(title,sizeof(title),f)){
        id[strcspn(id,"\n\r")]='\0';
        if(valid_youtube_id(id)){
            for(int i=0;i<a->result_count;i++){
                if(!strcmp(id,a->results[i].id)){a->current_video=a->results[i];break;}
            }
            for(int i=0;i<a->home_count;i++){
                if(!strcmp(id,a->home_videos[i].id)){a->current_video=a->home_videos[i];break;}
            }
            safe_line(a->current_video.id,sizeof(a->current_video.id),id);
            safe_line(a->current_video.title,sizeof(a->current_video.title),title);
            a->current_is_youtube=true;
        }
    }
    fclose(f);
    if(a->current_is_youtube){
        if(snprintf(path,sizeof(path),"%s/selected-origin.txt",a->data_dir)<(int)sizeof(path)){
            FILE *origin=fopen(path,"r");
            if(origin){char from[12]={0};if(fgets(from,sizeof(from),origin)&&!strncmp(from,"home",4))a->from_page=HOME;fclose(origin);}
        }
    }
}
static void print_ellipsis(App*a,FontSize fs,const char*str,int x,int y,int maxwidth,SDL_Color c) {
    FontSize display=ui_font(a,fs);
    if(!str || !a->fonts[display] || maxwidth<=0)return;
    maxwidth=ui_project_width(a,x,maxwidth);
    char clipped[180];safe_line(clipped,sizeof(clipped),str);
    int width=0;
    if(TTF_SizeUTF8(a->fonts[display],clipped,&width,NULL)==0 && width<=maxwidth){print_text(a,fs,clipped,x,y,c);return;}
    size_t n=strlen(clipped);
    while(n>0){
        /* Keep truncation at a UTF-8 codepoint boundary. */
        n--;while(n>0&&((unsigned char)clipped[n]&0xC0)==0x80)n--;
        clipped[n]='\0';
        char tmp[180];snprintf(tmp,sizeof(tmp),"%s...",clipped);
        if(TTF_SizeUTF8(a->fonts[display],tmp,&width,NULL)==0 && width<=maxwidth){print_text(a,fs,tmp,x,y,c);return;}
    }
}
/* One small preferences file, automatically backed up by the existing OTA
 * snapshot mechanism; never store stream URLs, favs, or search history here. */
static bool save_settings(App *a) {
    if(!*a->pref_dir)return false;
    char path[PATH_MAX],tmp[PATH_MAX];
    if(snprintf(path,sizeof(path),"%ssettings.ini",a->pref_dir)>=(int)sizeof(path) ||
       snprintf(tmp,sizeof(tmp),"%ssettings.ini.tmp",a->pref_dir)>=(int)sizeof(tmp))return false;
    FILE *f=fopen(tmp,"w");if(!f)return false;
    bool ok=fprintf(f,"theme=%s\nnavigation=%d\ncards=%d\nstartup=%d\ntext_size=%d\n",
        theme_files[a->theme_id],a->nav_layout,a->card_density,a->startup_page,a->text_size)>0;
    if(fflush(f)!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    if(ok && rename(tmp,path)==0)return true;
    remove(tmp);return false;
}
static void load_settings(App *a) {
    char*p=SDL_GetPrefPath("MustardOS","MustardTube");
    if(!p)return;
    snprintf(a->pref_dir,sizeof(a->pref_dir),"%s",p);SDL_free(p);
    char path[PATH_MAX];if(snprintf(path,sizeof(path),"%ssettings.ini",a->pref_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f)return;
    char line[180];while(fgets(line,sizeof(line),f)) {
        char *end=strpbrk(line,"\r\n");if(end)*end='\0';
        if(!strncmp(line,"theme=",6)){
            for(int i=0;i<TCOUNT;i++)if(!strcmp(line+6,theme_files[i]))a->theme_id=i;
        }else {
            char *eq=strchr(line,'=');if(!eq)continue;
            *eq++='\0';if(eq[0]<'0'||eq[0]>'9'||eq[1]!='\0')continue;
            int value=eq[0]-'0';
            if(!strcmp(line,"navigation") && value<=2)a->nav_layout=value;
            else if(!strcmp(line,"cards") && value<=1)a->card_density=value;
            else if(!strcmp(line,"startup") && value<=3)a->startup_page=value;
            else if(!strcmp(line,"text_size") && value<=2)a->text_size=value;
        }
    }
    fclose(f);
}
static void change_preference(App*a,int index,int delta) {
    int *which=NULL,choices=0;
    if(index==0){which=&a->nav_layout;choices=3;}
    else if(index==1){which=&a->card_density;choices=2;}
    else if(index==2){which=&a->startup_page;choices=4;}
    else if(index==3){which=&a->text_size;choices=3;}
    if(!which)return;
    int old=*which;*which=(*which+delta+choices)%choices;
    if(!save_settings(a)){
        *which=old;
        snprintf(a->status_message,sizeof(a->status_message),"Unable to save preferences.");
    }else{
        a->status_message[0]='\0';a->reset_armed=false;
    }
    a->dirty=true;
}
static void personalize_activate(App*a) {
    if(a->personalize_idx!=4){change_preference(a,a->personalize_idx,1);return;}
    if(!a->reset_armed){a->reset_armed=true;a->dirty=true;return;}
    /* Reset appearance only: keep favourites, history, OTA and audio settings. */
    int oldtheme=a->theme_id,oldnav=a->nav_layout,oldcards=a->card_density;
    int oldstart=a->startup_page,oldtext=a->text_size;
    a->theme_id=0;a->nav_layout=NAV_VERTICAL;a->card_density=0;
    a->startup_page=0;a->text_size=1;
    if(!save_settings(a)){
        a->theme_id=oldtheme;a->nav_layout=oldnav;a->card_density=oldcards;
        a->startup_page=oldstart;a->text_size=oldtext;
        snprintf(a->status_message,sizeof(a->status_message),"Unable to save preferences.");
    }else{read_theme(a,0);a->status_message[0]='\0';}
    a->reset_armed=false;a->dirty=true;
}
static const char *font_location(void) {
    const char *custom=getenv("MUSTARDTUBE_FONT");if(custom&&exists(custom))return custom;
    const char *fonts[]={"/opt/muos/share/font/internal/Inter.ttf",
     "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
     "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
     "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
     "/usr/share/fonts/TTF/DejaVuSans.ttf",NULL};
    for(int i=0;fonts[i];i++)if(exists(fonts[i]))return fonts[i];
    return NULL;
}
/* Deliberately flat background: one clear, no 1.17 MiB texture or
 * large circles behind controls. Avoids visual noise in Paper and keeps the
 * low-power SDL renderer path cheap on the H700. */
static void draw_background(App *a) {
    fill(a,0,0,W,H,a->theme.bg);
}
/* Beta 2.3.2: unified 4:3 sidebar on ALL pages, one equal-size video-card
 * system across Home and Results. Presentation only: no new textures, timers,
 * workers or streaming/radio processes. Four thumbnails maximum per view. */
#define MT_SIDE_X 14
#define MT_SIDE_Y 88
#define MT_SIDE_W 114
#define MT_SIDE_ROW_Y 112
#define MT_SIDE_ROW_STEP 48
#define MT_HOME_X 145
#define MT_HOME_W 477
#define MT_HOME_CATEGORY_Y 143
#define MT_HOME_CATEGORY_W 77
#define MT_HOME_CARD_Y 185
#define MT_HOME_CARD_STEP 59
#define MT_HOME_CARD_H 54
#define MT_CARD_THUMB_W 86
#define MT_CARD_THUMB_H 48

/* muOS uses a monochrome glyph, while the app header uses a compact,
 * transparent rendition of the approved mascot. The Paper version has an
 * ink-coloured outline. Both are cached and loaded only when needed. */
static SDL_Texture *load_brand_texture(App *a,const char *relative) {
    char path[PATH_MAX];
    if(snprintf(path,sizeof(path),"%s/%s",a->asset_dir,relative)>=(int)sizeof(path))return NULL;
    SDL_Surface *s=IMG_Load(path);
    if(!s)return NULL;
    SDL_Texture *tex=SDL_CreateTextureFromSurface(a->r,s);
    SDL_FreeSurface(s);
    if(tex)SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
    return tex;
}
static void draw_brand_icon(App *a, int x, int y, int size) {
    bool paper=a->theme_id==2;
    if(paper && !a->brand_icon_paper_checked){
        a->brand_icon_paper_checked=true;
        a->brand_icon_paper=load_brand_texture(a,"branding/header-paper.png");
    }
    if(!paper && !a->brand_icon_checked){
        a->brand_icon_checked=true;
        /* In the installed muxapp the dark full-colour icon lives in /grid;
         * in the source checkout it lives in /assets/branding. */
        a->brand_icon=load_brand_texture(a,"branding/header-dark.png");
        if(!a->brand_icon)a->brand_icon=load_brand_texture(a,"../grid/mustardtube.png");
    }
    SDL_Texture *tex=paper?a->brand_icon_paper:a->brand_icon;
    if(tex){SDL_Rect d={x,y,size,size};SDL_RenderCopy(a->r,tex,NULL,&d);}
    else{
        /* Asset failure should remain recognizable branding, not an old
         * generic play logo also seen in loading placeholders. */
        rounded(a,x,y,size,size,9,a->theme.accent);
        text_center(a,FS_19,"MT",x,y+(size-19)/2,size,a->theme.accent_text);
    }
}
static void draw_header(App *a) {
    Theme*t=&a->theme;
    draw_brand_icon(a,18,8,51);
    print_text(a,FS_23,"Mustard",75,14,t->text);
    int name_width=84;
    TTF_SizeUTF8(a->fonts[FS_23],"Mustard",&name_width,NULL);
    print_text(a,FS_23,"Tube",75+name_width,14,t->accent_ink);
    print_text(a,FS_11,"WATCH  ·  LISTEN  ·  PLAY",76,43,t->muted);
    print_text(a,FS_11,"LB/RB  PAGES",516,26,t->muted);
    fill(a,18,68,604,1,t->outline);
}
static void draw_home_sidebar(App *a) {
    Theme*t=&a->theme;
    int active=(int)a->page;
    if(a->page==RESULTS)active=SEARCH;
    if(a->page==PLAYER)active=a->from_page==HOME?HOME:SEARCH;
    if(a->page==RADIO_NOW)active=RADIO;
    if(a->page==UPDATES || a->page==PERSONALIZE)active=STYLE;
    if(a->nav_layout==NAV_HORIZONTAL){
        outline(a,18,75,604,32,8,t->outline,t->surface);
        for(int i=0;i<NAV_TAB_COUNT;i++){
            int x=22+i*100;bool selected=i==active;
            if(selected)rounded(a,x,78,96,26,6,t->accent);
            text_center(a,FS_11,page_names[i],x,85,96,selected?t->accent_text:t->text);
        }
        return;
    }
    if(a->nav_layout==NAV_COMPACT){
        static const char *abbr[]={"HM","SR","RC","FM","ST","AB"};
        outline(a,MT_SIDE_X,MT_SIDE_Y,61,331,12,t->outline,t->surface);
        for(int i=0;i<NAV_TAB_COUNT;i++){
            int y=MT_SIDE_ROW_Y+i*MT_SIDE_ROW_STEP;
            bool selected=i==active;
            if(selected)rounded(a,MT_SIDE_X+6,y,49,38,9,t->accent);
            else if(i)fill(a,MT_SIDE_X+12,y-5,38,1,t->outline);
            text_center(a,FS_13,abbr[i],MT_SIDE_X+6,y+11,49,selected?t->accent_text:t->text);
        }
        return;
    }
    outline(a,MT_SIDE_X,MT_SIDE_Y,MT_SIDE_W,331,12,t->outline,t->surface);
    for(int i=0;i<NAV_TAB_COUNT;i++){
        int y=MT_SIDE_ROW_Y+i*MT_SIDE_ROW_STEP;
        bool selected=i==active;
        if(selected)rounded(a,MT_SIDE_X+6,y,MT_SIDE_W-12,38,9,t->accent);
        else if(i!=0)fill(a,MT_SIDE_X+15,y-5,MT_SIDE_W-30,1,blend(t->surface,t->outline,1,2));
        print_text(a,FS_13,page_names[i],MT_SIDE_X+16,y+11,selected?t->accent_text:t->text);
    }
}
static void home_video_card(App *a, int i) {
    Theme*t=&a->theme;
    YoutubeResult *v=&a->home_videos[i];
    const int x=MT_HOME_X,y=MT_HOME_CARD_Y+i*(a->card_density?46:MT_HOME_CARD_STEP);
    const int tx=x+6,ty=y+3,tw=MT_CARD_THUMB_W,th=MT_CARD_THUMB_H;
    const int desc=tx+tw+11,avail=x+MT_HOME_W-desc-9;
    bool selected=a->home_idx==i;
    int cardh=a->card_density?43:MT_HOME_CARD_H;
    outline(a,x,y,MT_HOME_W,cardh,9,selected?t->accent:t->outline,selected?t->selected:t->surface);
    if(a->card_density){
        print_ellipsis(a,FS_13,v->title,x+12,y+4,MT_HOME_W-24,t->text);
        print_ellipsis(a,FS_11,v->channel,x+12,y+23,MT_HOME_W-95,t->muted);
        print_ellipsis(a,FS_11,v->duration,x+MT_HOME_W-67,y+23,58,t->secondary);
        return;
    }
    rounded(a,tx,ty,tw,th,6,t->alt);
    if(a->home_thumbs[i]){SDL_Rect dst={tx,ty,tw,th};ui_draw_texture(a,a->home_thumbs[i],NULL,&dst);}
    else video_placeholder(a,tx+tw/2,ty+th/2,false,t->muted,t->alt);
    print_ellipsis(a,FS_13,v->title,desc,y+7,avail,t->text);
    print_ellipsis(a,FS_11,v->channel,desc,y+30,avail-61,t->muted);
    print_ellipsis(a,FS_11,v->duration,x+MT_HOME_W-63,y+30,52,t->secondary);
}
static void draw_home(App *a) {
    Theme*t=&a->theme;
    char heading[72];snprintf(heading,sizeof(heading),"Videos / %s",home_category_labels[a->home_category]);
    print_ellipsis(a,FS_19,heading,MT_HOME_X,87,MT_HOME_W,t->text);
    print_text(a,FS_11,a->home_loading?"Refreshing discovery...":"Real videos, selected for you",MT_HOME_X,115,t->muted);
    for(int i=0;i<HOME_CATEGORIES;i++){
        int x=MT_HOME_X+i*80;
        bool active=(i==a->home_category);
        outline(a,x,MT_HOME_CATEGORY_Y,MT_HOME_CATEGORY_W,26,7,active?t->accent:t->outline,active?t->accent:t->surface);
        text_center(a,FS_11,home_category_labels[i],x,MT_HOME_CATEGORY_Y+7,MT_HOME_CATEGORY_W,active?t->accent_text:t->text);
    }
    if(a->home_count==0){
        outline(a,MT_HOME_X,MT_HOME_CARD_Y,MT_HOME_W,231,14,t->outline,t->surface);
        if(a->home_loading)loading_dots(a,MT_HOME_X+MT_HOME_W/2,239);
        else video_placeholder(a,MT_HOME_X+MT_HOME_W/2,239,true,t->secondary,t->surface);
        text_center(a,FS_19,a->home_loading?"Loading videos...":"No videos yet",MT_HOME_X,289,MT_HOME_W,t->text);
        print_ellipsis(a,FS_11,a->home_feedback[0]?a->home_feedback:"Check Wi-Fi, or press X to retry.",MT_HOME_X+18,343,MT_HOME_W-36,t->muted);
        return;
    }
    for(int i=0;i<a->home_count&&i<HOME_VIDEOS;i++)home_video_card(a,i);
}
static void draw_search(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"Search",MT_HOME_X,83,t->text);
    print_ellipsis(a,FS_11,"Enter a title, creator or topic with the D-pad.",MT_HOME_X,116,MT_HOME_W,t->muted);
    outline(a,MT_HOME_X,143,MT_HOME_W,48,11,t->accent,t->surface);
    if(!a->query[0])print_text(a,FS_15,"Search for videos...",MT_HOME_X+13,156,t->muted);
    else print_ellipsis(a,FS_15,a->query,MT_HOME_X+13,156,MT_HOME_W-26,t->text);
    print_ellipsis(a,FS_11,a->search_feedback[0]?a->search_feedback:"START searches YouTube; A enters a character.",MT_HOME_X,197,MT_HOME_W,t->secondary);
    for(int i=0;i<36;i++){
        int row=i/9,col=i%9,x=149+col*51,y=219+row*40;
        bool sel=a->keyboard_idx==i;
        rounded(a,x,y,45,34,7,sel?t->accent:t->alt);
        char label[2]={keyboard_chars[i],0};
        text_center(a,FS_15,label,x,y+7,45,sel?t->accent_text:t->text);
    }
    const char* names[3]={"SPACE","DELETE","SEARCH"};
    for(int j=0;j<3;j++){
        int x=149+j*156;bool sel=a->keyboard_idx==36+j;
        rounded(a,x,384,148,35,8,sel?t->accent:t->alt);
        text_center(a,FS_13,names[j],x,394,148,sel?t->accent_text:t->text);
    }
}
static void draw_saved(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"Recent activity",MT_HOME_X,83,t->text);
    print_ellipsis(a,FS_11,"Return to your previous search without retyping.",MT_HOME_X,116,MT_HOME_W,t->muted);
    outline(a,MT_HOME_X,151,MT_HOME_W,259,14,t->outline,t->surface);
    print_text(a,FS_11,"LAST SEARCH",MT_HOME_X+15,173,t->secondary);
    print_ellipsis(a,FS_19,a->query[0]?a->query:"Nothing searched yet",MT_HOME_X+15,204,MT_HOME_W-30,t->text);
    for(int i=0;i<2;i++) {
        int y=266+i*55;
        bool active=a->saved_idx==i;
        outline(a,MT_HOME_X+14,y,MT_HOME_W-28,45,10,active?t->accent:t->outline,active?t->selected:t->alt);
        print_text(a,FS_15,i==0?"Search again":"Open last results",MT_HOME_X+26,y+11,t->text);
        print_text(a,FS_11,i==0?"A SEARCH":"A OPEN",MT_HOME_X+359,y+15,active?t->accent_ink:t->muted);
    }
    print_ellipsis(a,FS_11,"Only successful searches are retained.",MT_HOME_X+15,390,MT_HOME_W-25,t->muted);
}
static void draw_results(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"YouTube results",MT_HOME_X,83,t->text);
    print_ellipsis(a,FS_11,a->more_available?"A Details   X More results   B Edit search":"A Details   B Edit search",MT_HOME_X,114,MT_HOME_W,t->muted);
    print_ellipsis(a,FS_11,a->status_message,MT_HOME_X,139,MT_HOME_W,t->secondary);
    if(a->result_count==0){
        outline(a,MT_HOME_X,174,MT_HOME_W,237,14,t->outline,t->surface);
        video_placeholder(a,MT_HOME_X+MT_HOME_W/2,235,true,t->secondary,t->surface);
        text_center(a,FS_19,"No videos returned",MT_HOME_X,284,MT_HOME_W,t->text);
        print_ellipsis(a,FS_13,a->status_message[0]?a->status_message:"Check Wi-Fi and try another search.",MT_HOME_X+20,326,MT_HOME_W-40,t->muted);
        return;
    }
    const int perpage=a->card_density?6:RESULTS_VISIBLE;
    int first=(a->result_idx/perpage)*perpage;
    for(int i=0;i<perpage;i++){
        int idx=first+i;if(idx>=a->result_count)break;
        YoutubeResult *v=&a->results[idx];
        int y=174+i*(a->card_density?38:59);
        bool active=idx==a->result_idx;
        if(a->card_density){
            outline(a,MT_HOME_X,y,MT_HOME_W,35,7,active?t->accent:t->outline,active?t->selected:t->surface);
            print_ellipsis(a,FS_13,v->title,MT_HOME_X+10,y+2,MT_HOME_W-20,t->text);
            print_ellipsis(a,FS_11,v->channel,MT_HOME_X+10,y+19,MT_HOME_W-89,t->muted);
            print_ellipsis(a,FS_11,v->duration,MT_HOME_X+MT_HOME_W-68,y+19,57,t->secondary);
            continue;
        }
        outline(a,MT_HOME_X,y,MT_HOME_W,MT_HOME_CARD_H,9,active?t->accent:t->outline,active?t->selected:t->surface);
        rounded(a,MT_HOME_X+6,y+3,MT_CARD_THUMB_W,MT_CARD_THUMB_H,6,t->alt);
        if(a->result_thumbs[i]){SDL_Rect dst={MT_HOME_X+6,y+3,MT_CARD_THUMB_W,MT_CARD_THUMB_H};ui_draw_texture(a,a->result_thumbs[i],NULL,&dst);}
        else video_placeholder(a,MT_HOME_X+6+MT_CARD_THUMB_W/2,y+3+MT_CARD_THUMB_H/2,false,t->muted,t->alt);
        const int dx=MT_HOME_X+MT_CARD_THUMB_W+17;
        print_ellipsis(a,FS_13,v->title,dx,y+7,MT_HOME_W-MT_CARD_THUMB_W-27,t->text);
        print_ellipsis(a,FS_11,v->channel,dx,y+30,MT_HOME_W-MT_CARD_THUMB_W-90,t->muted);
        print_ellipsis(a,FS_11,v->duration,MT_HOME_X+MT_HOME_W-63,y+30,53,t->secondary);
    }
    char pagination[96];
    int last=first+perpage;if(last>a->result_count)last=a->result_count;
    snprintf(pagination,sizeof(pagination),"%d-%d of %d  %s",first+1,last,a->result_count,
       a->more_available?"| X: +8":"| end");
    text_center(a,FS_11,pagination,MT_HOME_X,414,MT_HOME_W,t->muted);
}
/* The list begins below its heading and status. Seven compact rows fit
 * safely inside y=178..401, leaving y=413 for per-view actions. */
#define RADIO_LIST_Y 178
#define RADIO_ROW_H 32
#define RADIO_VISIBLE 7
static void draw_radio(App*a){
    Theme*t=&a->theme;
    print_text(a,FS_23,"Internet Radio",MT_HOME_X,82,t->text);
    print_ellipsis(a,FS_11,"A Listen   START Background   SELECT Stop",MT_HOME_X,113,MT_HOME_W,t->muted);
    if(a->radio_background_name[0]){
        char now[175];snprintf(now,sizeof(now),"BG RADIO: %s",a->radio_background_name);
        print_ellipsis(a,FS_11,now,MT_HOME_X,133,MT_HOME_W,t->accent_ink);
    }else if(a->radio_loading)print_text(a,FS_11,"Loading radio directory...",MT_HOME_X,133,t->secondary);
    else if(a->radio_status[0])print_ellipsis(a,FS_11,a->radio_status,MT_HOME_X,133,MT_HOME_W,t->secondary);
    bool countries=a->radio_mode==0, fav=a->radio_mode==2;
    int n=countries?a->country_count:fav?a->favourite_count:a->station_count;
    int idx=countries?a->country_idx:fav?a->favourite_idx:a->station_idx;
    int first=idx/RADIO_VISIBLE*RADIO_VISIBLE;
    int last=first+RADIO_VISIBLE;if(last>n)last=n;
    print_text(a,FS_13,countries?"COUNTRIES A-Z":fav?"FAVOURITES A-Z":"STATIONS A-Z",MT_HOME_X,156,t->accent_ink);
    char location[72];
    if(n)snprintf(location,sizeof(location),"%d-%d / %d",first+1,last,n);
    else snprintf(location,sizeof(location),"0 stations");
    print_text(a,FS_11,location,542,157,t->muted);
    if(n>RADIO_VISIBLE){
        const int track_h=RADIO_VISIBLE*RADIO_ROW_H-3;
        rounded(a,619,RADIO_LIST_Y,3,track_h,1,t->outline);
        int thumb_h=(int)((long long)RADIO_VISIBLE*track_h/n);
        if(thumb_h<16)thumb_h=16;
        int thumb_y=RADIO_LIST_Y+(n>RADIO_VISIBLE?(int)((long long)first*(track_h-thumb_h)/(n-RADIO_VISIBLE)):0);
        rounded(a,618,thumb_y,4,thumb_h,2,t->accent);
    }
    for(int j=0;j<RADIO_VISIBLE && first+j<n;j++){
        int ix=first+j,y=RADIO_LIST_Y+j*RADIO_ROW_H;
        bool active=ix==idx;
        outline(a,MT_HOME_X,y,MT_HOME_W-7,29,7,active?t->accent:t->outline,active?t->selected:t->surface);
        if(countries){
            RadioCountry*c=&a->countries[ix];
            radio_draw_country_flag(a,c->code,MT_HOME_X+8,y+3);
            print_ellipsis(a,FS_13,c->name,MT_HOME_X+56,y+5,MT_HOME_W-145,t->text);
            char count[32];snprintf(count,sizeof(count),"%d",c->count);
            print_text(a,FS_11,count,MT_HOME_X+407,y+8,t->muted);
        }else{
            RadioStation*st=fav?&a->favourites[ix]:&a->stations[ix];
            SDL_Texture*logo=radio_station_logo(a,j,st);
            if(logo){SDL_Rect d={MT_HOME_X+8,y+2,25,25};ui_draw_texture(a,logo,NULL,&d);}
            else {rounded(a,MT_HOME_X+8,y+2,25,25,5,t->bg2);print_text(a,FS_11,"FM",MT_HOME_X+11,y+9,t->secondary);}
            print_ellipsis(a,FS_13,st->name,MT_HOME_X+42,y+5,MT_HOME_W-135,t->text);
            print_ellipsis(a,FS_11,st->codec,MT_HOME_X+402,y+8,53,t->muted);
            if(radio_is_favourite(a,st))print_text(a,FS_11,"*",MT_HOME_X+377,y+7,t->accent_ink);
        }
    }
    if(!n){
        if(a->radio_loading)loading_dots(a,MT_HOME_X+MT_HOME_W/2,215);
        print_ellipsis(a,FS_13,countries?"No countries. Press X to retry.":fav?"No saved stations yet.":a->radio_loading?"Loading stations...":"No stations. Press X to retry.",MT_HOME_X+10,253,MT_HOME_W-20,t->text);
    }
    print_ellipsis(a,FS_11,countries?"A Open  X Refresh  Y Favourites  START Playing":
        fav?"A Listen  START BG  SELECT Stop  Y Unfavourite":
        "A Listen  START BG  SELECT Stop  X More  Y Fav",MT_HOME_X,414,MT_HOME_W,t->muted);
}
/* One FFplay decoder powers foreground and background radio.  Only the tiny
 * status dot changes once per second.  No FFT, spinning texture, network
 * polling, or SDL renderer remains running after leaving for a game. */
static void draw_radio_now(App*a){
    Theme*t=&a->theme;
    print_text(a,FS_23,"Radio / Now Playing",MT_HOME_X,82,t->text);
    print_ellipsis(a,FS_11,"Lightweight built-in player, no video decoder",MT_HOME_X,113,MT_HOME_W,t->muted);
    outline(a,MT_HOME_X,138,MT_HOME_W,270,14,t->outline,t->surface);
    rounded(a,MT_HOME_X+14,154,86,86,12,t->alt);
    if(!a->radio_now_logo_checked){
        (void)radio_station_logo(a,0,&a->radio_now_station);
        a->radio_now_logo_checked=true;
    }
    SDL_Texture*logo=a->radio_logos[0];
    if(logo){SDL_Rect dst={MT_HOME_X+18,158,78,78};ui_draw_texture(a,logo,NULL,&dst);}
    else {circle(a,MT_HOME_X+57,197,31,t->accent);text_center(a,FS_19,"FM",MT_HOME_X+24,185,65,t->accent_text);}
    rounded(a,MT_HOME_X+112,155,175,26,7,a->radio_now_active?t->selected:t->alt);
    circle(a,MT_HOME_X+125,168,5,a->radio_now_active?
        (a->radio_now_blink?t->accent:t->outline):t->muted);
    print_text(a,FS_13,a->radio_now_active?"RADIO ACTIVE":"RADIO STOPPED",MT_HOME_X+139,159,
               a->radio_now_active?t->accent_ink:t->muted);
    print_ellipsis(a,FS_19,a->radio_now_station.name[0]?a->radio_now_station.name:"Internet Radio",MT_HOME_X+112,192,MT_HOME_W-129,t->text);
    char sub[95];snprintf(sub,sizeof(sub),"%s  %s",a->radio_now_station.codec[0]?a->radio_now_station.codec:"Audio stream",
       a->radio_now_station.country);
    print_text(a,FS_11,sub,MT_HOME_X+114,222,t->muted);
    print_ellipsis(a,FS_11,a->radio_now_active?"Player running (stream may buffer)":"Press A to retry; B for stations",MT_HOME_X+15,249,MT_HOME_W-29,t->secondary);
    char vol[80];
    snprintf(vol,sizeof(vol),"%s Radio Volume  %d%%",a->radio_now_slider==0?">":" ",a->radio_mix_volume);
    print_text(a,FS_13,vol,MT_HOME_X+15,273,a->radio_now_slider==0?t->accent_ink:t->text);
    rounded(a,MT_HOME_X+15,297,MT_HOME_W-30,9,4,t->outline);
    if(a->radio_mix_volume)rounded(a,MT_HOME_X+15,297,(MT_HOME_W-30)*a->radio_mix_volume/100,9,4,t->accent);
    snprintf(vol,sizeof(vol),"%s Game Volume  %d%%",a->radio_now_slider==1?">":" ",a->game_mix_volume);
    print_text(a,FS_13,vol,MT_HOME_X+15,322,a->radio_now_slider==1?t->accent_ink:t->text);
    rounded(a,MT_HOME_X+15,347,MT_HOME_W-30,9,4,t->outline);
    if(a->game_mix_volume)rounded(a,MT_HOME_X+15,347,(MT_HOME_W-30)*a->game_mix_volume/100,9,4,t->accent);
    print_ellipsis(a,FS_11,"UP/DOWN Mix  LEFT/RIGHT +/-5%  Stop: game 100%",MT_HOME_X+15,372,MT_HOME_W-30,t->muted);
    print_ellipsis(a,FS_11,a->radio_hotkeys_mode==2?
         "X: In-game RADIO+GAME":a->radio_hotkeys_mode==1?
         "X: In-game RADIO (press to enable GAME)":
         "X: In-game OFF",MT_HOME_X+15,392,MT_HOME_W-30,t->secondary);
    print_ellipsis(a,FS_11,a->radio_now_notice[0]?a->radio_now_notice:
         "START Games  SELECT Stop  Y Wasabi  B List",MT_HOME_X,416,MT_HOME_W,t->secondary);
}
static void draw_style(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"Settings",MT_HOME_X,83,t->text);
    print_ellipsis(a,FS_11,a->status_message[0]?a->status_message:"Appearance, storage and app performance",MT_HOME_X,114,MT_HOME_W,t->muted);
    print_text(a,FS_13,"APPEARANCE",MT_HOME_X,139,t->secondary);
    /* Four equal-size samples in a 2x2 controller-friendly grid. No per-frame
     * image load, file I/O or extra SDL texture allocation. */
    static const SDL_Color preview_bg[TCOUNT]={{16,20,24,255},{17,36,30,255},{245,240,230,255},{25,15,43,255}};
    static const SDL_Color preview_card[TCOUNT]={{32,40,48,255},{36,58,49,255},{255,252,246,255},{48,32,71,255}};
    static const SDL_Color preview_accent[TCOUNT]={{242,190,75,255},{199,219,145,255},{232,187,104,255},{223,255,99,255}};
    for(int i=0;i<TCOUNT;i++){
        int x=MT_HOME_X+(i%2)*244,y=161+(i/2)*80;
        bool active=a->style_idx==i;
        outline(a,x,y,233,72,11,active?t->accent:t->outline,active?t->selected:t->surface);
        rounded(a,x+9,y+9,83,54,7,preview_bg[i]);
        rounded(a,x+16,y+18,69,28,5,preview_card[i]);
        rounded(a,x+21,y+23,26,18,4,preview_accent[i]);
        rounded(a,x+51,y+25,29,4,2,preview_accent[i]);
        rounded(a,x+51,y+34,22,3,2,preview_accent[i]);
        print_text(a,FS_13,i==0?"Midnight":i==1?"Forest":i==2?"Paper":"Acid Bloom",x+101,y+14,t->text);
        if(i==a->theme_id)print_text(a,FS_11,"ACTIVE",x+101,y+42,t->accent_ink);
        else if(active)print_text(a,FS_11,"A  APPLY",x+101,y+42,t->muted);
    }
    print_text(a,FS_13,"MORE SETTINGS",MT_HOME_X,320,t->secondary);
    for(int i=0;i<2;i++){
        int x=MT_HOME_X+i*244,y=346;
        bool selected=a->style_idx==4+i;
        outline(a,x,y,233,58,10,selected?t->accent:t->outline,selected?t->selected:t->surface);
        print_ellipsis(a,FS_13,i==0?"Personalization":"Software updates",x+13,y+9,205,
            selected?t->accent_ink:t->text);
        print_text(a,FS_11,"A  OPEN",x+13,y+34,t->muted);
    }
    print_ellipsis(a,FS_11,"START  In-game hotkeys   SELECT  Updates",MT_HOME_X,411,MT_HOME_W,t->secondary);
}
static void draw_personalize(App*a) {
    Theme*t=&a->theme;
    static const char *navs[]={"Vertical sidebar","Horizontal top tabs","Compact sidebar"};
    static const char *densities[]={"Standard: 4 thumbnails","Compact: 6 result rows"};
    static const char *starts[]={"Home","Search","Recent","Radio"};
    static const char *sizes[]={"Small","Normal","Large"};
    const char *labels[]={"Navigation layout","Video card density","Open MustardTube on","Content text size","Reset appearance only"};
    const char *values[]={navs[a->nav_layout],densities[a->card_density],starts[a->startup_page],sizes[a->text_size],
        a->reset_armed?"Press A again to confirm":"Keep favourites and history"};
    print_text(a,FS_23,"Personalization",MT_HOME_X,83,t->text);
    print_ellipsis(a,FS_11,a->status_message[0]?a->status_message:
        "UP/DOWN Choose   LEFT/RIGHT Change   A Apply",MT_HOME_X,116,MT_HOME_W,t->muted);
    for(int i=0;i<5;i++) {
        int y=142+i*53;bool active=i==a->personalize_idx;
        outline(a,MT_HOME_X,y,MT_HOME_W,47,9,active?t->accent:t->outline,active?t->selected:t->surface);
        print_ellipsis(a,FS_11,labels[i],MT_HOME_X+12,y+5,MT_HOME_W-25,t->secondary);
        print_ellipsis(a,FS_13,values[i],MT_HOME_X+12,y+23,MT_HOME_W-35,
                       active?t->accent_ink:t->text);
    }
    print_ellipsis(a,FS_11,"Saved automatically. B returns to Settings.",MT_HOME_X,415,MT_HOME_W,t->muted);
}
static void update_read_status(App *a) {
    char state[32]="not checked", detail[150]="Select CHECK to contact GitHub Releases", release[64]="", size[40]="";
    int progress=-1;
    char file[PATH_MAX];
    if(snprintf(file,sizeof(file),"%s/ota-status.txt",a->data_dir)<(int)sizeof(file)) {
        FILE *f=fopen(file,"r");
        if(f){char line[210];while(fgets(line,sizeof(line),f)){
            if(!strncmp(line,"state=",6))safe_line(state,sizeof(state),line+6);
            else if(!strncmp(line,"detail=",7))safe_line(detail,sizeof(detail),line+7);
            else if(!strncmp(line,"release=",8))safe_line(release,sizeof(release),line+8);
            else if(!strncmp(line,"size=",5))safe_line(size,sizeof(size),line+5);
            else if(!strncmp(line,"progress=",9)){
                char *end=NULL;long v=strtol(line+9,&end,10);
                if(end!=line+9 && (*end=='\n'||*end=='\r'||*end=='\0') && v>=0 && v<=100)progress=(int)v;
            }
        }fclose(f);}
    }
    if(strcmp(a->update_state,state)||strcmp(a->update_detail,detail)||
       strcmp(a->update_release,release)||strcmp(a->update_size,size)||a->update_progress!=progress){
        snprintf(a->update_state,sizeof(a->update_state),"%s",state);
        snprintf(a->update_detail,sizeof(a->update_detail),"%s",detail);
        snprintf(a->update_release,sizeof(a->update_release),"%s",release);
        snprintf(a->update_size,sizeof(a->update_size),"%s",size);
        a->update_progress=progress;
        a->dirty=true;
    }
}
static void update_read_channel(App*a){
    a->update_beta=true;
    a->update_auto=true;
    char ac[PATH_MAX];
    if(snprintf(ac,sizeof(ac),"%s/ota-auto-check.txt",a->data_dir)<(int)sizeof(ac)){
        FILE*af=fopen(ac,"r");
        if(af){char setting[12]={0};if(fgets(setting,sizeof(setting),af) && !strncmp(setting,"off",3))a->update_auto=false;fclose(af);}
    }
    char p[PATH_MAX];if(snprintf(p,sizeof(p),"%s/ota-channel.txt",a->data_dir)>=(int)sizeof(p))return;
    FILE*f=fopen(p,"r");if(!f)return;
    char buf[20]={0};if(fgets(buf,sizeof(buf),f) && !strncmp(buf,"stable",6))a->update_beta=false;
    fclose(f);
}
static void draw_updates(App*a){
    Theme*t=&a->theme;
    print_text(a,FS_23,"Software updates",MT_HOME_X,83,t->text);
    print_text(a,FS_11,"Official GitHub Releases / muOS Andromeda",MT_HOME_X,115,t->muted);
    outline_ota(a,MT_HOME_X,144,MT_HOME_W,100,11,t->outline,t->surface);
    print_text(a,FS_13,"Installed: " MT_RELEASE,MT_HOME_X+12,157,t->text);
    print_ellipsis(a,FS_11,a->update_detail,MT_HOME_X+12,185,MT_HOME_W-24,t->secondary);
    if(a->update_release[0])print_ellipsis(a,FS_11,a->update_release,MT_HOME_X+12,207,300,t->text);
    if(a->update_size[0])print_text(a,FS_11,a->update_size,MT_HOME_X+367,207,t->muted);
    if(!strcmp(a->update_state,"downloading") && a->update_progress>=0 && a->update_progress<=100){
        // Lightweight theme-aware rectangle: no textures, animations or extra timers.
        const int x=MT_HOME_X+12,y=225,w=MT_HOME_W-83,h=8;
        rounded(a,x,y,w,h,4,t->outline);
        const int completed=w*a->update_progress/100;
        if(completed>0)rounded(a,x,y,completed,h,completed<8?1:4,t->accent);
        char pct[12];snprintf(pct,sizeof(pct),"%d%%",a->update_progress);
        print_text(a,FS_11,pct,MT_HOME_X+MT_HOME_W-53,220,t->text);
    }
    const bool ready=!strcmp(a->update_state,"ready");
    const char *options[]={"Check for updates",
        ready?"Exit for Archive Manager":"Prepare verified update",
        a->update_beta?"Channel: Beta":"Channel: Stable",
        a->update_auto?"Auto-check: On":"Auto-check: Off"};
    for(int i=0;i<4;i++){
        int y=250+i*41;bool active=i==a->update_selected;
        outline_ota(a,MT_HOME_X,y,MT_HOME_W,37,9,active?t->accent:t->outline,active?t->selected:t->surface);
        print_text(a,FS_13,options[i],MT_HOME_X+13,y+9,active?t->accent_ink:t->text);
        if(active)print_text(a,FS_11,"A",MT_HOME_X+MT_HOME_W-24,y+10,t->accent_ink);
    }
    print_text(a,FS_11,"Install: Applications > Archive Manager",MT_HOME_X,416,t->secondary);
    print_text(a,FS_11,"Verified download; installed by muOS",MT_HOME_X,431,t->muted);
}
static void open_updates(App*a) {
    update_read_channel(a);
    a->update_selected=0;
    set_page(a,UPDATES);
    update_read_status(a);
}
static void activate_updates(App*a){
    // A running update worker owns the candidate until verification completes.
    // Navigation remains responsive, but another task must not be launched.
    if(!strcmp(a->update_state,"checking") || !strcmp(a->update_state,"downloading") ||
       !strcmp(a->update_state,"verifying") || !strcmp(a->update_state,"backup"))return;
    if(a->update_selected==2){
        a->update_beta=!a->update_beta;
        if(!write_data_file(a,"ota-channel.txt",a->update_beta?"beta\n":"stable\n")){
            snprintf(a->update_detail,sizeof(a->update_detail),"Unable to save update channel");
        }else{
            char p[PATH_MAX];if(snprintf(p,sizeof(p),"%s/ota-status.txt",a->data_dir)<(int)sizeof(p))unlink(p);
            update_read_status(a);
        }
        a->dirty=true;
    }else if(a->update_selected==3){
        a->update_auto=!a->update_auto;
        if(!write_data_file(a,"ota-auto-check.txt",a->update_auto?"on\n":"off\n")){
            a->update_auto=!a->update_auto;
            snprintf(a->update_detail,sizeof(a->update_detail),"Unable to save auto-check preference");
        }
        a->dirty=true;
    }else if(a->update_selected==1 && !strcmp(a->update_state,"ready")){
        // Leave MustardTube cleanly; muOS Archive Manager performs the install.
        a->running=false;
    }else{
        a->request_update_check=a->update_selected==0;
        a->request_update_download=a->update_selected==1;
        a->running=false;
    }
}
static void reveal_line(char *dst,size_t cap,const unsigned char *src,size_t n){
    if(cap==0)return;
    if(n>=cap)n=cap-1;
    for(size_t i=0;i<n;i++)dst[i]=(char)(src[i]^0x5d);
    dst[n]=0;
}
static void draw_extra(App*a){
    Theme*t=&a->theme;
    static const unsigned char l1[]={0x09, 0x35, 0x38, 0x7d, 0x15, 0x38, 0x3c, 0x2f, 0x29, 0x7d, 0x1f, 0x38, 0x35, 0x34, 0x33, 0x39, 0x7d, 0x10, 0x28, 0x2e, 0x29, 0x3c, 0x2f, 0x39, 0x09, 0x28, 0x3f, 0x38};
    static const unsigned char l2[]={0x1b, 0x32, 0x2f, 0x7d, 0x19, 0x34, 0x2f, 0x3c, 0x71, 0x7d, 0x19, 0x38, 0x2f, 0x71, 0x7d, 0x0d, 0x2f, 0x34, 0x33, 0x3e, 0x38, 0x2e, 0x2e, 0x7d, 0x7b, 0x7d, 0x10, 0x3c, 0x30, 0x3c, 0x73};
    static const unsigned char l3[]={0x18, 0x2b, 0x38, 0x2f, 0x24, 0x7d, 0x3a, 0x2f, 0x38, 0x3c, 0x29, 0x7d, 0x3e, 0x2f, 0x38, 0x3c, 0x29, 0x34, 0x32, 0x33, 0x7d, 0x35, 0x3c, 0x2e, 0x7d, 0x3c, 0x7d, 0x2f, 0x38, 0x3c, 0x2e, 0x32, 0x33, 0x7d, 0x3f, 0x38, 0x35, 0x34, 0x33, 0x39, 0x7d, 0x34, 0x29, 0x73};
    static const unsigned char l4[]={0x04, 0x32, 0x28, 0x7d, 0x3c, 0x2f, 0x38, 0x7d, 0x30, 0x34, 0x33, 0x38, 0x73};
    static const unsigned char l5[]={0x10, 0x3c, 0x39, 0x38, 0x7d, 0x2a, 0x34, 0x29, 0x35, 0x7d, 0x31, 0x32, 0x2b, 0x38, 0x71, 0x7d, 0x19, 0x3c, 0x39, 0x39, 0x24};
    char msg[128];
    circle(a,301,77,22,t->secondary);circle(a,339,77,22,t->secondary);
    for(int j=0;j<42;j++){int w=84-2*j;fill(a,320-w/2,81+j,w,1,t->secondary);}
    reveal_line(msg,sizeof(msg),l1,sizeof(l1));text_center(a,FS_19,msg,50,148,540,t->accent_ink);
    reveal_line(msg,sizeof(msg),l2,sizeof(l2));text_center(a,FS_15,msg,50,206,540,t->text);
    reveal_line(msg,sizeof(msg),l3,sizeof(l3));text_center(a,FS_13,msg,50,255,540,t->muted);
    reveal_line(msg,sizeof(msg),l4,sizeof(l4));text_center(a,FS_13,msg,50,278,540,t->muted);
    fill(a,150,336,340,1,t->outline);
    reveal_line(msg,sizeof(msg),l5,sizeof(l5));text_center(a,FS_15,msg,50,368,540,t->text);
}
/* Self-contained manual: no WebView, markdown runtime or network dependency.
 * Each line is bounded for the 640x480 display; one logical line per row. */
typedef struct {const char *text; bool heading;} AboutLine;
static const AboutLine ABOUT_LINES[]={
    {"WELCOME",true},
    {"MustardTube - lightweight video and Internet Radio",false},
    {"Version " MT_RELEASE " | PapaZolaRis",false},
    {"Independent community project for muOS.",false},
    {"",false},
    {"REQUIREMENTS",true},
    {"muOS 2606.0 Andromeda; multiple display formats.",false},
    {"Wi-Fi for online video and radio; stock Wasabi player.",false},
    {"Radio: A opens built-in Now Playing, START background.",false},
    {"Now Playing: UP/DOWN slider, LEFT/RIGHT adjusts, SELECT stops.",false},
    {"Now Playing: START exits to games, Y opens Wasabi.",false},
    {"Background mode requires a bundled ARM64 audio player.",false},
    {"Settings START: cycle radio/game hotkeys, or OFF.",false},
    {"While playing: hold L2+R2, tap LEFT/RIGHT for radio mix.",false},
    {"Game slider sets radio-session mix; L2+R2 Up/Down in game.",false},
    {"To stop radio: return to app and use Radio SELECT.",false},
    {"Simultaneous game audio needs on-device validation.",false},
    {"No PortMaster, Google login or API key required.",false},
    {"Other devices need community hardware validation.",false},
    {"",false},
    {"INSTALL / UPDATE",true},
    {"1. Copy the .muxapp file to an SD-card ARCHIVE folder.",false},
    {"2. Open muOS Applications > Archive Manager.",false},
    {"3. Select the installer and complete installation.",false},
    {"4. Connect Wi-Fi, then launch MustardTube.",false},
    {"5. Back up the MustardTube data folder before upgrades.",false},
    {"",false},
    {"GLOBAL CONTROLS",true},
    {"LB / RB: switch top tabs; D-pad: navigate lists.",false},
    {"A: select / play; B: back (Home: exit app).",false},
    {"Hold UP/DOWN: continuous scrolling in Radio/About.",false},
    {"LEFT/RIGHT: jump one page in Radio/About.",false},
    {"",false},
    {"HOME - VIDEOS",true},
    {"Explore / Popular / For You / Music / Gaming / Tech.",false},
    {"LEFT/RIGHT selects a category, UP/DOWN a video.",false},
    {"A opens details; X shows next four discovery videos.",false},
    {"Cards are search-based, not official YouTube Trending.",false},
    {"For You uses local viewing signals where available.",false},
    {"",false},
    {"SEARCH",true},
    {"Use D-pad and A to type with the on-screen keyboard.",false},
    {"START submits; X loads more on results screens.",false},
    {"A on results opens the video detail page.",false},
    {"Up to 120 search results; 4 thumbnails at a time.",false},
    {"",false},
    {"PLAYBACK & QUALITY",true},
    {"A on video details plays through stock Wasabi.",false},
    {"Y cycles 360p (default), 480p, 720p and 1080p.",false},
    {"360p/480p seeking works on compatible recordings.",false},
    {"720p and 1080p streaming is experimental.",false},
    {"Live streams: NO SEEK (all qualities).",false},
    {"Some streams may fail due to format or URL expiry.",false},
    {"In Wasabi: MENU + START returns to MustardTube.",false},
    {"",false},
    {"INTERNET RADIO",true},
    {"Open Radio, choose a country, then select a station.",false},
    {"Country and station lists are alphabetical A-Z.",false},
    {"Hold UP/DOWN to scroll; LEFT/RIGHT jumps 7 rows.",false},
    {"X refreshes countries / loads more stations.",false},
    {"Y saves/removes a favourite; Y on countries opens",false},
    {"the Favourites list; B returns to countries.",false},
    {"Station icons are optional; some stations go offline.",false},
    {"Radio A uses built-in player; Y in Now Playing",false},
    {"offers Wasabi fallback if station is cached.",false},
    {"",false},
    {"RECENT AND SETTINGS",true},
    {"Recent: reopen the last search and results.",false},
    {"Settings: A selects Midnight, Forest or Paper.",false},
    {"Settings: X clears disposable cache, not favourites.",false},
    {"Settings: Y clears local viewing history.",false},
    {"For speed, muOS Application Options can set",false},
    {"the MustardTube CPU governor to Performance.",false},
    {"This may increase heat and battery consumption.",false},
    {"",false},
    {"STORAGE, NETWORK AND PRIVACY",true},
    {"Browsing fetches remote YouTube/radio metadata.",false},
    {"Cache stores temporary results, thumbnails and logs.",false},
    {"Favourite radio stations are saved locally.",false},
    {"Clear Cache will not delete saved favourites.",false},
    {"Data may be sent to external video/radio services",false},
    {"when you search, browse or stream content.",false},
    {"Avoid sharing private data or signed URLs in logs.",false},
    {"Radio station listings and URLs can become outdated.",false},
    {"No offline video downloads in this beta.",false},
    {"",false},
    {"TROUBLESHOOTING",true},
    {"Check Wi-Fi if feeds, search or radio will not load.",false},
    {"Retry with X; cached directory may remain available.",false},
    {"Try another station or 360p if playback fails.",false},
    {"Clear cache in Settings after extended use.",false},
    {"Report device, firmware, steps and redacted logs.",false},
    {"",false},
    {"IMPORTANT NOTICE - USE AT YOUR OWN RISK",true},
    {"UNOFFICIAL SOFTWARE, PROVIDED AS IS.",false},
    {"No warranties to the fullest extent allowed by law.",false},
    {"Use can cause crashes, data loss or device damage.",false},
    {"Back up data. You are responsible for applicable",false},
    {"laws, copyright and platform terms of service.",false},
    {"To the fullest extent permitted by applicable law,",false},
    {"PapaZolaRis and contributors disclaim liability",false},
    {"for loss, damage or legal consequences from use.",false},
    {"Mandatory rights and liabilities are not excluded.",false},
    {"Not affiliated with or endorsed by YouTube, Google,",false},
    {"Anbernic or MustardOS / muOS.",false},
    {"",false},
    {"LICENSING AND ATTRIBUTION",true},
    {"MustardTube: Free and Open Source Software.",false},
    {"License: GNU GPL version 3 only (GPL-3.0-only).",false},
    {"Official source and license information:",false},
    {"github.com/PapaZolaRis/MustardTube",false},
    {"Prior MIT releases retain their original terms.",false},
    {"Third-party licences ship in the app's licenses dir.",false},
    {"Country flag artwork requires Twemoji attribution.",false},
    {"Radio metadata is provided by external directories.",false},
    {"",false},
    {"THANK YOU",true},
    {"Please share feedback on the muOS community forum.",false},
    {"Developed by PapaZolaRis. Enjoy MustardTube!",false}
};
#define ABOUT_COUNT ((int)(sizeof(ABOUT_LINES)/sizeof(ABOUT_LINES[0])))
#define ABOUT_VISIBLE 13
static void about_scroll_by(App*a,int change){
    int max=ABOUT_COUNT-ABOUT_VISIBLE;
    if(max<0)max=0;
    int next=a->about_scroll+change;
    if(next<0)next=0;
    if(next>max)next=max;
    if(next!=a->about_scroll){a->about_scroll=next;a->dirty=true;}
}
/* Jump to the next/previous manual section without loading any extra assets. */
static void about_jump_section(App*a,int direction){
    if(direction>0){
        for(int i=a->about_scroll+1;i<ABOUT_COUNT;i++)if(ABOUT_LINES[i].heading){
            about_scroll_by(a,i-a->about_scroll);break;
        }
    }else if(direction<0){
        for(int i=a->about_scroll-1;i>=0;i--)if(ABOUT_LINES[i].heading){
            about_scroll_by(a,i-a->about_scroll);break;
        }
    }
}
static void draw_about(App *a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"About / User Guide",MT_HOME_X,83,t->text);
    print_ellipsis(a,FS_11,"Manual, controls, legal notices and credits",MT_HOME_X,115,MT_HOME_W,t->muted);
    char progress[80];
    int last=a->about_scroll+ABOUT_VISIBLE;if(last>ABOUT_COUNT)last=ABOUT_COUNT;
    snprintf(progress,sizeof(progress),"%d-%d / %d",a->about_scroll+1,last,ABOUT_COUNT);
    print_text(a,FS_11,progress,MT_HOME_X+383,141,t->secondary);
    print_text(a,FS_11,"UP/DOWN Scroll    LEFT/RIGHT Page",MT_HOME_X,141,t->accent_ink);
    outline(a,MT_HOME_X,160,MT_HOME_W,252,12,t->outline,t->surface);
    for(int i=0;i<ABOUT_VISIBLE && a->about_scroll+i<ABOUT_COUNT;i++){
        const AboutLine*line=&ABOUT_LINES[a->about_scroll+i];
        int y=168+i*18;
        if(line->heading){
            rounded(a,MT_HOME_X+9,y-1,MT_HOME_W-25,18,4,t->bg2);
            print_ellipsis(a,FS_11,line->text,MT_HOME_X+16,y,MT_HOME_W-47,t->accent_ink);
        }else print_ellipsis(a,FS_11,line->text,MT_HOME_X+16,y,MT_HOME_W-47,t->text);
    }
    const int track_y=164,track_h=240,thumb_h=32;
    rounded(a,MT_HOME_X+MT_HOME_W-13,track_y,4,track_h,2,t->outline);
    int max=ABOUT_COUNT-ABOUT_VISIBLE;
    int thumb_y=track_y+(max>0?a->about_scroll*(track_h-thumb_h)/max:0);
    rounded(a,MT_HOME_X+MT_HOME_W-14,thumb_y,6,thumb_h,3,t->accent);
    print_text(a,FS_11,"X/Y Sections    LB/RB Tabs    B Home",MT_HOME_X,418,t->muted);
}
static void draw_player(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,a->current_is_youtube?"Video details":"Playback test",MT_HOME_X,83,t->text);
    rounded(a,MT_HOME_X,129,MT_HOME_W,211,14,color(8,12,20));
    if(a->current_is_youtube && a->thumbnail){
        SDL_Rect dst={MT_HOME_X+84,137,309,174};
        ui_draw_texture(a,a->thumbnail,NULL,&dst);
        /* Keep the creator thumbnail unobscured by app branding: a small
         * dark, outlined play badge carries the only overlay. */
        outline_ota(a,MT_HOME_X+214,207,46,38,10,t->accent,color(15,19,24));
        preview_play_symbol(a,MT_HOME_X+238,226,t->accent);
    }else{
        /* No video artwork: show a neutral placeholder, not a fake preview.
         * The A PLAY control below remains the real playback affordance. */
        video_placeholder(a,MT_HOME_X+MT_HOME_W/2,212,true,t->secondary,color(8,12,20));
    }
    rounded(a,MT_HOME_X+120,299,237,33,9,t->accent);
    text_center(a,FS_13,a->awaiting_stream?"PREPARING VIDEO...":a->current_is_youtube?"A  PLAY":"A  TEST SOUND",MT_HOME_X+120,308,237,t->accent_text);
    if(a->current_is_youtube) {
        print_ellipsis(a,FS_15,a->current_video.title,MT_HOME_X+3,348,MT_HOME_W-6,t->text);
        char line[160];
        if(a->stream_failed)snprintf(line,sizeof(line),a->quality>=720?
            "HD unavailable. Y changes quality; A retries.":"Stream unavailable. Press A to retry.");
        else if(a->awaiting_stream)snprintf(line,sizeof(line),"Preparing video... %lds  |  B Cancel",(long)(time(NULL)-a->wait_started));
        else if(stream_is_prepared(a)){
            char kind_path[PATH_MAX];char kind[20]={0};
            if(snprintf(kind_path,sizeof(kind_path),"%s/stream-kind.txt",a->data_dir)<(int)sizeof(kind_path)){
                FILE*kind_file=fopen(kind_path,"r");
                if(kind_file){fgets(kind,sizeof(kind),kind_file);fclose(kind_file);}
            }
            if(!strncmp(kind,"live",4))snprintf(line,sizeof(line),"LIVE ready  |  %s",a->current_video.channel);
            else if(a->quality>=720)snprintf(line,sizeof(line),"HD %dp ready  |  %s",a->quality,a->current_video.channel);
            else snprintf(line,sizeof(line),"Stream ready  |  %s",a->current_video.channel);
        }else if(a->current_video.channel[0])snprintf(line,sizeof(line),"%s  %s",a->current_video.channel,a->current_video.duration);
        else snprintf(line,sizeof(line),"Press A to play.");
        print_ellipsis(a,FS_13,line,MT_HOME_X+3,377,MT_HOME_W-6,t->muted);
        const char *quality_label=a->quality==360?"Y 360p Fast | MENU+START to return":
            a->quality==480?"Y 480p Clear | MENU+START to return":
            a->quality==720?"Y 720p HD | MENU+START to return":"Y 1080p HD | MENU+START to return";
        print_ellipsis(a,FS_11,quality_label,MT_HOME_X+3,410,MT_HOME_W-6,t->secondary);
    }else{
        print_text(a,FS_15,"Test video + audio",MT_HOME_X+3,350,t->text);
        print_text(a,FS_11,"10-second sample with audible test tone.",MT_HOME_X+3,380,t->muted);
        print_text(a,FS_11,"H.264 / AAC - MENU+START returns here.",MT_HOME_X+3,410,t->secondary);
    }
    if(a->wasabi_missing){
        rounded(a,MT_HOME_X+22,270,MT_HOME_W-44,26,8,t->surface);
        text_center(a,FS_11,"Wasabi not found. Use muOS Andromeda.",MT_HOME_X+22,276,MT_HOME_W-44,t->accent_ink);
    }
}
static void draw_footer(App*a) {
    Theme*t=&a->theme;
    fill(a,0,437,640,43,t->bg);
    fill(a,18,437,604,1,t->outline);
    const char *action=a->page==HOME?"A  Details    X  Refresh    UP/DOWN  Videos":
     a->page==SEARCH?"D-PAD  Keyboard    A  Type    START  Search":
     a->page==SAVED?"A  Open selected    UP/DOWN  Choose":
     a->page==RADIO?"A  Listen    START  Background    SELECT  Stop":
     a->page==RADIO_NOW?"UP/DOWN  Select mix    LEFT/RIGHT  Adjust":
     a->page==STYLE?"A  Select   START  Hotkeys   X Cache   Y History":
     a->page==PERSONALIZE?"UP/DOWN  Setting    LEFT/RIGHT  Change   A  Select":
     a->page==UPDATES?"A  Select    UP/DOWN  Options    B  Back":
     a->page==ABOUT?"UP/DOWN  Scroll    X/Y  Sections    B  Home":
     a->page==RESULTS?(a->more_available?"A  Details    X  More results    B  Search":"A  Details    B  Search"):
     "A  Play    Y  Quality    B  Back";
    const char *navigation=a->page==HOME?"LEFT/RIGHT  Category    LB/RB  Pages":
     a->page==RADIO_NOW?"START  Games    SELECT  Stop    LB/RB  Pages":
     a->page==ABOUT?"LEFT/RIGHT  Page    LB/RB  Tabs":
     a->page==UPDATES?"Settings > SELECT  Updates":
     a->page==PERSONALIZE?"B  Back to Settings   Preferences saved":
     a->page==PLAYER?"MENU + START  Return from Wasabi":
     "LB/RB  Pages    B  Back";
    print_ellipsis(a,FS_11,action,20,443,490,t->text);
    print_ellipsis(a,FS_11,navigation,20,460,490,t->muted);
    print_text(a,FS_11,MT_FOOTER,530,453,t->secondary);
}
static void render(App*a) {
    draw_background(a);
    if(a->page==EXTRA){draw_extra(a);return;}
    draw_header(a);draw_home_sidebar(a);
    a->render_content=true;
    switch(a->page){case HOME:draw_home(a);break;case SEARCH:draw_search(a);break;
    case SAVED:draw_saved(a);break;case RADIO:draw_radio(a);break;case RADIO_NOW:draw_radio_now(a);break;
    case STYLE:draw_style(a);break;case ABOUT:draw_about(a);break;case UPDATES:draw_updates(a);break;case PERSONALIZE:draw_personalize(a);break;case EXTRA:break;
    case PLAYER:draw_player(a);break;case RESULTS:draw_results(a);break;}
    a->render_content=false;
    draw_footer(a);
}
static void set_page(App*a,Page p) {
    a->nav_repeat_dir=0; /* A held D-pad never changes the next tab. */
    if(a->page==RADIO_NOW && p!=RADIO_NOW)drop_radio_logos(a); /* Release player artwork when hidden. */
    if(a->page==HOME && p!=HOME)stop_home_worker(a);
    if(a->page==RESULTS && p!=RESULTS)stop_result_thumb_worker(a);
    if(a->page==RADIO && p!=RADIO){stop_radio_worker(a);stop_radio_logo_worker(a);drop_radio_logos(a);}
    if(p!=PLAYER && p!=RESULTS){stop_prefetch(a);a->awaiting_stream=false;}
    a->page=p;a->dirty=true;
    if(p==HOME){load_home_file(a);if(a->home_worker_pid==0)start_home_refresh(a,false);}
    if(p==RESULTS && !a->card_density)start_result_thumb_worker(a,true);
    if(p==RADIO)radio_open(a);
    if(p==RADIO_NOW)radio_now_load(a);
    if(p==STYLE){a->style_idx=a->theme_id;radio_mix_load(a);a->status_message[0]=0;}
    if(p==PERSONALIZE){a->personalize_idx=0;a->reset_armed=false;a->status_message[0]=0;}
    if(p==UPDATES){update_read_status(a);a->update_last_poll=0;}
}
/* Entering the Radio tab resumes the active native player.  B from Now
 * Playing deliberately calls set_page(RADIO), retaining access to the
 * directory while audio continues. This is UI-only: no player restart. */
static void open_nav_tab(App*a,Page tab){
    if(tab==RADIO && radio_now_process_alive(a))set_page(a,RADIO_NOW);
    else set_page(a,tab);
}
static void cycle_page(App*a,int step) {
    a->status_message[0]=0;
    /* Treat Now Playing as the Radio tab; do not briefly enter the country
     * directory (which could initiate an unnecessary directory worker). */
    if(a->page==RADIO_NOW){
        int p=(RADIO+step+NAV_TAB_COUNT)%NAV_TAB_COUNT;
        open_nav_tab(a,(Page)p);return;
    }
    if(a->page==PLAYER || a->page==RESULTS)set_page(a,a->from_page);
    if(a->page==UPDATES || a->page==PERSONALIZE)set_page(a,STYLE);
    if(a->page==EXTRA)set_page(a,ABOUT);
    int p=((int)a->page+step+NAV_TAB_COUNT)%NAV_TAB_COUNT;
    open_nav_tab(a,(Page)p);
}
static void submit_search(App*a); /* Defined below; reused by recent-search action. */
static void open_last_results(App*a) {
    load_result_file(a);
    if(a->result_count>0) set_page(a,RESULTS);
    else set_page(a,SEARCH);
}
static void home_action(App*a,int index) {
    if(a->home_count==0){set_page(a,SEARCH);return;}
    if(index<0 || index>=a->home_count)return;
    a->current_video=a->home_videos[index];
    a->current_is_youtube=true;
    write_data_file(a,"selected-origin.txt","home\n");
    stop_home_worker(a); /* Do not contend with video preparation. */
    show_video_details(a);
}
static void recent_action(App*a) {
    if(a->saved_idx==1)open_last_results(a);
    else {set_page(a,SEARCH);if(a->query[0])submit_search(a);}
}
static void submit_search(App*a) {
    if(!a->query[0]){snprintf(a->search_feedback,sizeof(a->search_feedback),"Enter a search phrase first.");a->dirty=true;return;}
    if(!write_data_file(a,"pending-search.txt",a->query)){
        snprintf(a->search_feedback,sizeof(a->search_feedback),"Cannot write search request.");a->dirty=true;return;
    }
    /* Preserve the query and reset selection for new searches. */
    write_data_file(a,"query.txt",a->query);
    write_data_file(a,"next-result-index.txt","0\n");
    a->request_youtube=true;
    a->running=false;
}
static void request_clear_cache(App*a) {
    stop_prefetch(a);a->request_clear=true;a->running=false;
}
static void request_clear_history(App*a) {
    stop_prefetch(a);a->request_clear_history=true;a->running=false;
}
static void request_more_results(App*a) {
    if(a->page!=RESULTS || !a->more_available || a->result_count>=SEARCH_RESULTS)return;
    char index[20];snprintf(index,sizeof(index),"%d\n",a->result_count);
    if(!write_data_file(a,"next-result-index.txt",index))return;
    a->request_more=true;a->running=false;
}
static void type_ascii(App*a,char c) {
    size_t n=strlen(a->query);if(n>=MAX_QUERY)return;
    if((unsigned char)c<32||(unsigned char)c>126)return;
    a->query[n]=c;a->query[n+1]='\0';a->dirty=true;
}
static void remove_char(App*a) {size_t n=strlen(a->query);if(n)a->query[n-1]='\0';a->dirty=true;}
static void keyboard_activate(App*a) {
    if(a->keyboard_idx<36)type_ascii(a,keyboard_chars[a->keyboard_idx]);
    else if(a->keyboard_idx==36)type_ascii(a,' ');
    else if(a->keyboard_idx==37)remove_char(a);
    else submit_search(a);
}
static void navigate(App*a,int dx,int dy) {
    if(a->page==ABOUT){
        about_scroll_by(a,dx?dx*ABOUT_VISIBLE:dy);
    }else if(a->page==UPDATES){
        if(dy || dx)a->update_selected=(a->update_selected+dy+dx+12)%4;
    }
    else if(a->page==HOME){
        if(dx)home_set_category(a,(a->home_category+dx+HOME_CATEGORIES)%HOME_CATEGORIES);
        else if(dy&&a->home_count>0)a->home_idx=(a->home_idx+dy+a->home_count)%a->home_count;
    }
    else if(a->page==SAVED){a->saved_idx=(a->saved_idx+dy+dx+4)%2;}
    else if(a->page==RADIO){
        int move=dx?dx*RADIO_VISIBLE:dy;
        int *index=a->radio_mode==0?&a->country_idx:a->radio_mode==1?&a->station_idx:&a->favourite_idx;
        int n=a->radio_mode==0?a->country_count:a->radio_mode==1?a->station_count:a->favourite_count;
        if(n>0){
            int next=*index+move;
            if(next<0)next=0;
            if(next>=n)next=n-1;
            *index=next;
        }
    }
    else if(a->page==RESULTS){int n=a->result_count;if(n){
        int perpage=a->card_density?6:RESULTS_VISIBLE;
        int first=(a->result_idx/perpage)*perpage;
        a->result_idx=(a->result_idx+dy+dx+n*3)%n;
        if(!a->card_density && first!=(a->result_idx/perpage)*perpage)start_result_thumb_worker(a,true);
    }}
    else if(a->page==STYLE){
        int col=a->style_idx%2,row=a->style_idx/2;
        if(dx)col=(col+dx+2)%2;
        if(dy)row=(row+dy+3)%3;
        a->style_idx=row*2+col;
    }
    else if(a->page==PERSONALIZE){
        if(dy){a->personalize_idx=(a->personalize_idx+dy+5)%5;a->reset_armed=false;}
        if(dx)change_preference(a,a->personalize_idx,dx);
    }
    else if(a->page==SEARCH){
        int k=a->keyboard_idx;
        if(k<36){int x=k%9,y=k/9;x=(x+dx+9)%9;y+=dy;if(y<0)y=4;if(y>4)y=0;
            if(y==4)k=36+x/3;else k=y*9+x;
        }else{if(dx)k=36+(k-36+dx+3)%3;if(dy<0)k=3*9+(k-36)*3+1;}
        a->keyboard_idx=k;
    }
    if(a->page==RADIO && a->radio_mode!=0)start_radio_logo_worker(a,false);
    a->dirty=true;
}
static void activate(App*a) {
    if(a->page==HOME)home_action(a,a->home_idx);
    else if(a->page==SAVED)recent_action(a);
    else if(a->page==RADIO){
        if(a->radio_mode==0 && a->country_count>0){
            snprintf(a->radio_selected_country,sizeof(a->radio_selected_country),"%s",a->countries[a->country_idx].code);
            a->radio_mode=1;a->station_count=0;a->station_idx=0;a->radio_next_offset=0;a->radio_has_more=true;stop_radio_logo_worker(a);drop_radio_logos(a);radio_refresh(a,false,0);
        }else if(a->radio_mode==1 && a->station_count>0){
            if(!radio_start_native_station(a,&a->stations[a->station_idx]))snprintf(a->radio_status,sizeof(a->radio_status),"Station address unavailable.");
        }else if(a->radio_mode==2 && a->favourite_count>0){
            if(!radio_start_native_station(a,&a->favourites[a->favourite_idx]))snprintf(a->radio_status,sizeof(a->radio_status),"Station address unavailable.");
        }
    }
    else if(a->page==RADIO_NOW)radio_now_reconnect(a,false);
    else if(a->page==STYLE){
        if(a->style_idx<4){if(read_theme(a,a->style_idx) && !save_settings(a)){
            snprintf(a->status_message,sizeof(a->status_message),"Unable to save theme.");
        }}else if(a->style_idx==4)set_page(a,PERSONALIZE);
        else open_updates(a);
    }
    else if(a->page==PERSONALIZE)personalize_activate(a);
    else if(a->page==UPDATES)activate_updates(a);
    else if(a->page==SEARCH)keyboard_activate(a);
    else if(a->page==RESULTS){
        if(a->result_count>0){
            a->current_video=a->results[a->result_idx];a->current_is_youtube=true;
            write_data_file(a,"selected-origin.txt","results\n");
            char index[20];snprintf(index,sizeof(index),"%d\n",a->result_idx);
            write_data_file(a,"next-result-index.txt",index);
            /* Only selected videos may start extraction; browsing results remains lightweight. */
            show_video_details(a);
        }
    }else if(a->page==PLAYER) {
        if(a->awaiting_stream)return;
        if(access(WASABI_BIN, X_OK)!=0) {a->wasabi_missing=true;a->dirty=true;return;}
        if(a->current_is_youtube){
            char chosen[256];
            snprintf(chosen,sizeof(chosen),"%s\n%s\n",a->current_video.id,a->current_video.title);
            if(!write_data_file(a,"selected.txt",chosen)){
                a->wasabi_missing=true;a->dirty=true;return;
            }
        }
        if(a->current_is_youtube && !stream_is_prepared(a)){
            a->awaiting_stream=true;a->stream_failed=false;a->dirty=true;
            a->wait_started=time(NULL);a->play_started=SDL_GetTicks();a->wait_display=-1;
            /* Launch again only if no previous worker is still active. */
            start_prefetch(a);
        }else{
            if(a->current_is_youtube){
                char path[PATH_MAX];
                if(snprintf(path,sizeof(path),"%s/timing.log",a->data_dir)<(int)sizeof(path)){
                    FILE*f=fopen(path,"a");if(f){fputs("play_to_ready_ms=0 cache=prepared\n",f);fclose(f);}
                }
            }
            a->handoff_wasabi=true;a->running=false;
        }
    }
}
static void back(App*a) {
    if(a->page==EXTRA){set_page(a,ABOUT);return;}
    if(a->page==UPDATES || a->page==PERSONALIZE){set_page(a,STYLE);return;}
    if(a->page==PLAYER){
        stop_prefetch(a);a->awaiting_stream=false;
        if(a->current_is_youtube){write_data_file(a,"selected.txt","");drop_thumbnail(a);}
        set_page(a,a->from_page);
    }
    else if(a->page==RESULTS)set_page(a,SEARCH);
    else if(a->page==RADIO_NOW)set_page(a,RADIO);
    else if(a->page==RADIO && a->radio_mode!=0){stop_radio_worker(a);stop_radio_logo_worker(a);drop_radio_logos(a);a->radio_mode=0;a->dirty=true;}
    else if(a->page!=HOME)set_page(a,HOME);
    else a->running=false;
}
static void radio_x(App*a){
    if(a->radio_loading)return;
    if(a->radio_mode==0)radio_refresh(a,true,0);
    else if(a->radio_mode==1){
        if(!a->radio_has_more){snprintf(a->radio_status,sizeof(a->radio_status),"All available stations loaded.");a->dirty=true;}
        else radio_refresh(a,false,a->radio_next_offset);
    }
}
static void radio_y(App*a){
    if(a->radio_mode==0){a->radio_mode=2;radio_load_favourites(a);start_radio_logo_worker(a,true);a->dirty=true;return;}
    if(a->radio_mode==1&&a->station_count>0)radio_toggle_favourite(a,&a->stations[a->station_idx]);
    else if(a->radio_mode==2&&a->favourite_count>0)radio_toggle_favourite(a,&a->favourites[a->favourite_idx]);
}
static void feed_note(App*a,int button){
    if(a->page!=ABOUT)return;
    static const int keys[]={1,1,2,2,3};
    if(button==keys[a->note_pos])a->note_pos++;
    else a->note_pos=button==keys[0]?1:0;
    if(a->note_pos==5){a->note_pos=0;set_page(a,EXTRA);}
}
static void keydown(App*a,SDL_Keycode key) {
    if(a->page==ABOUT){
        int keycode=(key==SDLK_UP)?1:(key==SDLK_DOWN)?2:(key==SDLK_RETURN||key==SDLK_KP_ENTER)?3:0;
        feed_note(a,keycode);
        if(a->page==EXTRA)return;
    }
    switch(key){
      case SDLK_UP:if(a->page==RADIO_NOW){a->radio_now_slider=0;a->dirty=true;}else navigate(a,0,-1);break;
      case SDLK_DOWN:if(a->page==RADIO_NOW){a->radio_now_slider=1;a->dirty=true;}else navigate(a,0,1);break;
      case SDLK_LEFT:if(a->page==RADIO_NOW){
          bool ok=a->radio_now_slider?game_mix_adjust(a,-1):radio_mix_adjust(a,-1);
          if(!ok)snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Selected volume update unavailable.");
          a->dirty=true;
        }else navigate(a,-1,0);break;
      case SDLK_RIGHT:if(a->page==RADIO_NOW){
          bool ok=a->radio_now_slider?game_mix_adjust(a,1):radio_mix_adjust(a,1);
          if(!ok)snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Selected volume update unavailable.");
          a->dirty=true;
        }else navigate(a,1,0);break;
      case SDLK_ESCAPE:back(a);break;
      case SDLK_RETURN:case SDLK_KP_ENTER:if(a->page==SEARCH)submit_search(a);else activate(a);break;
      case SDLK_BACKSPACE:if(a->page==SEARCH)remove_char(a);else back(a);break;
      case SDLK_u:if(a->page==STYLE)open_updates(a);break;
      case SDLK_p:if(a->page==STYLE)set_page(a,PERSONALIZE);break;
      case SDLK_TAB:cycle_page(a,1);break;
      case SDLK_PAGEUP:case SDLK_LEFTBRACKET:cycle_page(a,-1);break;
      case SDLK_PAGEDOWN:case SDLK_RIGHTBRACKET:cycle_page(a,1);break;
      case SDLK_x:if(a->page==ABOUT)about_jump_section(a,1);
        else if(a->page==RADIO_NOW){
            if(!radio_hotkeys_toggle(a))snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Unable to change hotkey mode.");
            a->dirty=true;
        }else if(a->page==RADIO)radio_x(a);
        else if(a->page==HOME)start_home_refresh(a,true);
        else if(a->page==RESULTS)request_more_results(a);
        else if(a->page==STYLE)request_clear_cache(a);
        break;
      case SDLK_h:if(a->page==STYLE && !radio_hotkeys_toggle(a)){
          snprintf(a->status_message,sizeof(a->status_message),"Unable to save in-game hotkeys setting.");a->dirty=true;
        }break;
      case SDLK_s:if(a->page==RADIO_NOW){a->running=false;break;}
        if(a->page==RADIO && !radio_start_background(a)){
          snprintf(a->radio_status,sizeof(a->radio_status),"Choose a station before starting background radio.");a->dirty=true;
        }break;
      case SDLK_k:if(a->page==RADIO || a->page==RADIO_NOW)radio_stop_background(a);break;
      case SDLK_y:if(a->page==RADIO_NOW)radio_now_reconnect(a,true);
        else if(a->page==ABOUT)about_jump_section(a,-1);else if(a->page==STYLE)request_clear_history(a);else if(a->page==RADIO)radio_y(a);else if(a->page==HOME)home_set_category(a,(a->home_category+1)%HOME_CATEGORIES);else toggle_quality(a);break;
      case SDLK_F5:read_theme(a,a->theme_id);break;
      case SDLK_F1:set_page(a,HOME);break;
      case SDLK_F2:set_page(a,SEARCH);break;
      case SDLK_F3:set_page(a,SAVED);break;
      case SDLK_F4:open_nav_tab(a,RADIO);break;
      case SDLK_F7:set_page(a,STYLE);break;
      case SDLK_F6:set_page(a,ABOUT);break;
      default:break;
    }
}
static SDL_GameController* find_pad(void) {
    for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)) {
        SDL_GameController*p=SDL_GameControllerOpen(i);if(p)return p;
    }
    return NULL;
}
static void on_button(App*a,Uint8 button) {
    if(a->page==ABOUT){
        int c=button==SDL_CONTROLLER_BUTTON_DPAD_UP?1:button==SDL_CONTROLLER_BUTTON_DPAD_DOWN?2:
            button==SDL_CONTROLLER_BUTTON_A?3:0;
        feed_note(a,c);
        if(a->page==EXTRA)return;
    }
    if((a->page==RADIO || a->page==ABOUT) &&
       (button==SDL_CONTROLLER_BUTTON_DPAD_UP || button==SDL_CONTROLLER_BUTTON_DPAD_DOWN)){
        a->nav_repeat_dir=button==SDL_CONTROLLER_BUTTON_DPAD_UP?-1:1;
        a->nav_repeat_at=SDL_GetTicks()+320;
    }
    switch(button){
      case SDL_CONTROLLER_BUTTON_DPAD_UP:if(a->page==RADIO_NOW){a->radio_now_slider=0;a->dirty=true;}else navigate(a,0,-1);break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN:if(a->page==RADIO_NOW){a->radio_now_slider=1;a->dirty=true;}else navigate(a,0,1);break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT:if(a->page==RADIO_NOW){
          bool ok=a->radio_now_slider?game_mix_adjust(a,-1):radio_mix_adjust(a,-1);
          if(!ok)snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Selected volume update unavailable.");
          a->dirty=true;
        }else navigate(a,-1,0);break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:if(a->page==RADIO_NOW){
          bool ok=a->radio_now_slider?game_mix_adjust(a,1):radio_mix_adjust(a,1);
          if(!ok)snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Selected volume update unavailable.");
          a->dirty=true;
        }else navigate(a,1,0);break;
      case SDL_CONTROLLER_BUTTON_A:activate(a);break;
      case SDL_CONTROLLER_BUTTON_B:back(a);break;
      case SDL_CONTROLLER_BUTTON_X:if(a->page==ABOUT)about_jump_section(a,1);
        else if(a->page==RADIO_NOW){
            if(!radio_hotkeys_toggle(a))snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Unable to change hotkey mode.");
            a->dirty=true;
        }else if(a->page==RADIO)radio_x(a);
        else if(a->page==HOME)start_home_refresh(a,true);
        else if(a->page==RESULTS)request_more_results(a);
        else if(a->page==STYLE)request_clear_cache(a);
        break;
      case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:cycle_page(a,-1);break;
      case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:cycle_page(a,1);break;
      case SDL_CONTROLLER_BUTTON_START:if(a->page==RADIO_NOW){a->running=false;}
        else if(a->page==RADIO){
            if(!radio_start_background(a)){snprintf(a->radio_status,sizeof(a->radio_status),"Choose a station before starting background radio.");a->dirty=true;}
        }else if(a->page==STYLE){
            if(!radio_hotkeys_toggle(a))snprintf(a->status_message,sizeof(a->status_message),"Unable to save in-game hotkeys setting.");
            a->dirty=true;
        }else if(a->page==SEARCH)submit_search(a);else set_page(a,HOME);break;
      case SDL_CONTROLLER_BUTTON_BACK:
        if(a->page==STYLE)open_updates(a);
        else if(a->page==RADIO || a->page==RADIO_NOW)radio_stop_background(a);
        break;
      case SDL_CONTROLLER_BUTTON_Y:if(a->page==RADIO_NOW)radio_now_reconnect(a,true);
        else if(a->page==ABOUT)about_jump_section(a,-1);else if(a->page==STYLE)request_clear_history(a);else if(a->page==RADIO)radio_y(a);else if(a->page==HOME)home_set_category(a,(a->home_category+1)%HOME_CATEGORIES);else if(a->page==PLAYER)toggle_quality(a);else set_page(a,STYLE);break;
      default:break;
    }
}
static void mouse_down(App*a,int x,int y){
    if(a->nav_layout==NAV_HORIZONTAL && x>=22 && x<622 && y>=75 && y<107){
        int tab=(x-22)/100;if(tab>=0 && tab<NAV_TAB_COUNT)open_nav_tab(a,(Page)tab);
        return;
    }
    int side_width=a->nav_layout==NAV_COMPACT?61:MT_SIDE_W;
    if(a->nav_layout!=NAV_HORIZONTAL && x>=MT_SIDE_X+6 && x<MT_SIDE_X+side_width-6 &&
       y>=MT_SIDE_ROW_Y && y<MT_SIDE_ROW_Y+MT_SIDE_ROW_STEP*NAV_TAB_COUNT){
        int tab=(y-MT_SIDE_ROW_Y)/MT_SIDE_ROW_STEP;
        if(tab<NAV_TAB_COUNT && (y-MT_SIDE_ROW_Y)%MT_SIDE_ROW_STEP<38)open_nav_tab(a,(Page)tab);
        return;
    }
    /* Input uses the same inverse transform as drawing, ensuring both
       mouse/touch and controller selection work in every navigation layout. */
    x=ui_unproject_x(a,x);y=ui_unproject_y(a,y);
    if(a->page==RADIO_NOW && x>=MT_HOME_X+15 && x<=MT_HOME_X+MT_HOME_W-15 &&
       ((y>=292 && y<=310)||(y>=342 && y<=360))){
        bool game=(y>=342);a->radio_now_slider=game?1:0;
        int level=((x-(MT_HOME_X+15))*100+(MT_HOME_W-30)/2)/(MT_HOME_W-30);
        level=((level+2)/5)*5;
        if(level>100)level=100;
        bool ok=game?game_mix_set(a,level):radio_mix_set(a,level);
        if(!ok)snprintf(a->radio_now_notice,sizeof(a->radio_now_notice),"Selected volume update unavailable.");
        a->dirty=true;return;
    }
    if(a->page==HOME && x>=MT_HOME_X && x<MT_HOME_X+MT_HOME_W &&
       y>=MT_HOME_CATEGORY_Y && y<MT_HOME_CATEGORY_Y+26){
        int cat=(x-MT_HOME_X)/80;
        if(cat<HOME_CATEGORIES && (x-MT_HOME_X)%80<MT_HOME_CATEGORY_W)home_set_category(a,cat);
    }else if(a->page==HOME&&x>=MT_HOME_X&&x<MT_HOME_X+MT_HOME_W){
        int pick=-1;
        if(y>=MT_HOME_CARD_Y){
            int offset=y-MT_HOME_CARD_Y,step=a->card_density?46:MT_HOME_CARD_STEP;
            int slot=offset/step;
            if(slot>=0&&slot<HOME_VIDEOS&&offset%step<(a->card_density?43:MT_HOME_CARD_H))pick=slot;
        }
        if(pick>=0&&pick<a->home_count){a->home_idx=pick;activate(a);}
    }else if(a->page==STYLE && x>=MT_HOME_X && x<MT_HOME_X+MT_HOME_W && y>=346 && y<404){
        int col=(x-MT_HOME_X)/244;if(col<2){a->style_idx=4+col;activate(a);} }
    else if(a->page==PERSONALIZE && x>=MT_HOME_X && x<MT_HOME_X+MT_HOME_W && y>=142 && y<407){
        a->personalize_idx=(y-142)/53;personalize_activate(a);
    }
    else if(a->page==UPDATES && x>=MT_HOME_X && x<MT_HOME_X+MT_HOME_W && y>=250 && y<410){
        int row=(y-250)/41;
        if((y-250)%41<37 && row<4){a->update_selected=row;activate(a);}
    }else if(a->page==RADIO&&x>=MT_HOME_X&&y>=RADIO_LIST_Y&&y<RADIO_LIST_Y+RADIO_ROW_H*RADIO_VISIBLE){
        int pos=(y-RADIO_LIST_Y)/RADIO_ROW_H,first=0;
        if(a->radio_mode==0)first=(a->country_idx/RADIO_VISIBLE)*RADIO_VISIBLE;
        else if(a->radio_mode==1)first=(a->station_idx/RADIO_VISIBLE)*RADIO_VISIBLE;
        else first=(a->favourite_idx/RADIO_VISIBLE)*RADIO_VISIBLE;
        int chosen=first+pos;
        if(a->radio_mode==0 && chosen<a->country_count){a->country_idx=chosen;activate(a);}
        else if(a->radio_mode==1 && chosen<a->station_count){a->station_idx=chosen;activate(a);}
        else if(a->radio_mode==2 && chosen<a->favourite_count){a->favourite_idx=chosen;activate(a);}
    }else if(a->page==ABOUT && x>=MT_HOME_X && x<MT_HOME_X+MT_HOME_W&&y>=160&&y<412){
        about_scroll_by(a,y<285?-ABOUT_VISIBLE:ABOUT_VISIBLE);
    }else if(a->page==STYLE&&x>=MT_HOME_X&&x<MT_HOME_X+MT_HOME_W&&y>=161&&y<313){
        int col=(x-MT_HOME_X)/244,row=(y-161)/80;
        if(col<2&&row<2&&(x-MT_HOME_X)%244<233&&(y-161)%80<72){
            a->style_idx=row*2+col;activate(a);
        }
    }
    else if(a->page==SEARCH){
        if(y>=219&&y<373&&x>=149&&x<608){int col=(x-149)/51,row=(y-219)/40;
            if(col<9&&row<4&&(x-149)%51<45&&(y-219)%40<34){a->keyboard_idx=row*9+col;keyboard_activate(a);}}
        else if(y>=384&&y<419&&x>=149&&x<609){int col=(x-149)/156;
            if(col<3&&(x-149)%156<148){a->keyboard_idx=36+col;keyboard_activate(a);}}
    }else if(a->page==SAVED&&x>=MT_HOME_X+14&&y>=266&&y<367){a->saved_idx=(y-266)/55;activate(a);}
    else if(a->page==RESULTS&&x>=MT_HOME_X&&y>=174&&y<410){
        int step=a->card_density?38:59,perpage=a->card_density?6:RESULTS_VISIBLE;
        int offset=y-174,base=(a->result_idx/perpage)*perpage;
        int pos=base+offset/step;
        if(offset%step<(a->card_density?35:MT_HOME_CARD_H)&&pos<a->result_count){a->result_idx=pos;activate(a);}}
    else if(a->page==PLAYER)activate(a);
    a->dirty=true;
}
static void handle_event(App*a,SDL_Event*e){
    switch(e->type){
      case SDL_QUIT:a->running=false;break;
      case SDL_KEYDOWN:
        if(!e->key.repeat || ((a->page==RADIO || a->page==ABOUT) &&
            (e->key.keysym.sym==SDLK_UP||e->key.keysym.sym==SDLK_DOWN)))
            keydown(a,e->key.keysym.sym);
        break;
      case SDL_TEXTINPUT:if(a->page==SEARCH){const char*p=e->text.text;
        for(;*p;p++)if((unsigned char)*p>=32&&(unsigned char)*p<127)type_ascii(a,*p);}break;
      case SDL_CONTROLLERBUTTONDOWN:on_button(a,e->cbutton.button);break;
      case SDL_CONTROLLERBUTTONUP:
        if((e->cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_UP && a->nav_repeat_dir==-1)||
           (e->cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_DOWN && a->nav_repeat_dir==1))
            a->nav_repeat_dir=0;
        break;
      case SDL_CONTROLLERDEVICEADDED:if(!a->pad)a->pad=find_pad();break;
      case SDL_CONTROLLERDEVICEREMOVED:
        a->nav_repeat_dir=0;
        if(a->pad&&SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(a->pad))==e->cdevice.which) {
            SDL_GameControllerClose(a->pad);a->pad=find_pad();}break;
      case SDL_MOUSEBUTTONDOWN:if(e->button.button==SDL_BUTTON_LEFT){
        int w,h,x,y;SDL_GetWindowSize(a->win,&w,&h);
        /* A letterboxed 1:1 or 16:9 screen needs viewport offset handling. */
        if(mt_window_to_logical(mt_viewport(w,h,W,H),e->button.x,e->button.y,W,H,&x,&y))
            mouse_down(a,x,y);
      }break;
      case SDL_WINDOWEVENT:if(e->window.event==SDL_WINDOWEVENT_EXPOSED||e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED)a->dirty=true;break;
      default:break;
    }
}
static void shutdown_app(App*a) {
    stop_prefetch(a);
    stop_home_worker(a);
    stop_result_thumb_worker(a);drop_result_thumbs(a);
    stop_radio_worker(a);stop_radio_logo_worker(a);drop_radio_logos(a);
    if(a->radio_flag_atlas)SDL_DestroyTexture(a->radio_flag_atlas);
    clear_cache(a);
    drop_home_thumbnails(a);
    if(a->brand_icon)SDL_DestroyTexture(a->brand_icon);
    if(a->brand_icon_paper)SDL_DestroyTexture(a->brand_icon_paper);
    drop_thumbnail(a);
    if(a->pad)SDL_GameControllerClose(a->pad);
    for(int i=0;i<FS_COUNT;i++)if(a->fonts[i])TTF_CloseFont(a->fonts[i]);
    if(a->r)SDL_DestroyRenderer(a->r);
    if(a->win)SDL_DestroyWindow(a->win);
    IMG_Quit();TTF_Quit();SDL_Quit();
}
static bool screenshot(App*a,const char*filename) {
    /* ReadPixels copies physical output pixels, NOT 640x480 logical pixels.
       Allocate accordingly to prevent overflow on square, HD and FHD devices. */
    int rw=0,rh=0;
    if(SDL_GetRendererOutputSize(a->r,&rw,&rh)!=0 || !mt_valid_display(rw,rh))return false;
    SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,rw,rh,32,SDL_PIXELFORMAT_ARGB8888);
    if(!s)return false;
    bool ok=SDL_RenderReadPixels(a->r,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)==0;
    if(ok)ok=SDL_SaveBMP(s,filename)==0;
    SDL_FreeSurface(s);return ok;
}
int main(int argc,char**argv) {
    const char*requested_assets=NULL,*screenfile=NULL;
    int force_theme=-1;
    bool handoff=false, resume_player=false, show_results=false, show_style=false, show_personalize=false, show_updates=false, show_about=false, show_radio=false, show_radio_now=false;
    int handoff_kind=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--version")){puts("MustardTube " MT_RELEASE " (Andromeda ARM64)");return 0;}
        if(!strcmp(argv[i],"--self-test")){
            if(strlen(keyboard_chars)!=36||HOME_VIDEOS!=4||HOME_CATEGORIES!=6||TCOUNT!=4||NAV_TAB_COUNT!=6){fputs("Self-test FAIL\n",stderr);return 1;}
            if(SEARCH_RESULTS!=120){fputs("Self-test FAIL: result cap\n",stderr);return 1;}
            puts("MustardTube " MT_RELEASE " self-test PASS: 6 tabs, 6 Home categories, 4 visible thumbnails, 120 search results, 4 themes");return 0;
        }
        if(!strcmp(argv[i],"--show-style"))show_style=true;
        else if(!strcmp(argv[i],"--show-personalize"))show_personalize=true;
        else if(!strcmp(argv[i],"--show-updates"))show_updates=true;
        else if(!strcmp(argv[i],"--show-about"))show_about=true;
        else if(!strcmp(argv[i],"--show-radio"))show_radio=true;
        else if(!strcmp(argv[i],"--show-radio-now"))show_radio_now=true;
        else if(!strcmp(argv[i],"--resume-player"))resume_player=true;
        else if(!strcmp(argv[i],"--show-results"))show_results=true;
        else if(!strcmp(argv[i],"--assets")&&i+1<argc)requested_assets=argv[++i];
        else if(!strcmp(argv[i],"--screenshot")&&i+1<argc)screenfile=argv[++i];
        else if(!strcmp(argv[i],"--theme")&&i+1<argc){const char*t=argv[++i];for(int n=0;n<TCOUNT;n++)if(!strcmp(t,theme_files[n]))force_theme=n;}
        else if(!strcmp(argv[i],"--help")){
            puts("Usage: mustardtube [--version|--self-test|--assets DIR|--show-radio|--show-results|--show-about|--show-style|--show-personalize|--show-updates|--resume-player|--theme midnight|forest|paper|acidbloom|--screenshot FILE.bmp]");return 0;
        }
    }
    App*a=(App*)calloc(1,sizeof(App));if(!a){fputs("Out of memory\n",stderr);return 1;}
    a->running=true;a->dirty=true;a->text_size=1;a->page=show_about?ABOUT:show_radio_now?RADIO_NOW:show_radio?RADIO:show_updates?UPDATES:show_personalize?PERSONALIZE:show_style?STYLE:resume_player?PLAYER:show_results?RESULTS:HOME;
    a->from_page=resume_player?RESULTS:HOME;
    int rc=1;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)!=0){fprintf(stderr,"SDL init: %s\n",SDL_GetError());goto early;}
    if(TTF_Init()!=0){fprintf(stderr,"TTF init: %s\n",TTF_GetError());goto done;}
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG); /* Enable small JPEG video cards and PNG radio artwork; both optional. */
    if(!discover_assets(a,requested_assets)){
        fputs("Missing assets/themes/midnight.theme. Run from project root or use --assets PATH\n",stderr);goto done;}
    load_settings(a);init_data_dir(a);radio_mix_load(a);game_mix_load(a);radio_hotkeys_load(a);a->quality=360;
    if(!show_about && !show_radio_now && !show_radio && !show_updates && !show_style &&
       !resume_player && !show_results && !show_personalize){
        static const Page start_tabs[]={HOME,SEARCH,SAVED,RADIO};
        a->page=start_tabs[a->startup_page];
    }
    if(show_updates){update_read_channel(a);update_read_status(a);}
    char quality_path[PATH_MAX];
    if(snprintf(quality_path,sizeof(quality_path),"%s/quality.txt",a->data_dir)<(int)sizeof(quality_path)){
        FILE*f=fopen(quality_path,"r");int q=0;
        if(f){if(fscanf(f,"%d",&q)==1 && (q==480 || q==720 || q==1080))a->quality=q;fclose(f);}
    }
    /* Search runs in a separate process. Restore the query when returning to Search. */
    if(a->data_dir[0]) {
        char last_query[PATH_MAX];
        if(snprintf(last_query,sizeof(last_query),"%s/query.txt",a->data_dir)<(int)sizeof(last_query)) {
            FILE *qf=fopen(last_query,"r");
            if(qf) {char line[MAX_QUERY+4];
                if(fgets(line,sizeof(line),qf))safe_line(a->query,sizeof(a->query),line);
                fclose(qf);
            }
        }
    }
    load_home_file(a);
    if(show_results || resume_player || show_style)load_result_file(a);
    if(resume_player)a->status_message[0]=0;
    if(resume_player){load_selected_video(a);a->preview_started=time(NULL);}
    if(force_theme>=0)a->theme_id=force_theme;
    if(!read_theme(a,a->theme_id))goto done;
    const char *font=font_location();if(!font){fputs("No installed font. Install fonts-dejavu-core or set MUSTARDTUBE_FONT.\n",stderr);goto done;}
    int sizes[]={11,13,15,19,23,29};
    for(int i=0;i<FS_COUNT;i++){a->fonts[i]=TTF_OpenFont(font,sizes[i]);if(!a->fonts[i]){fprintf(stderr,"TTF font: %s\n",TTF_GetError());goto done;}}
    /* Preserve the original 640x480 behaviour on H700 devices. On other
       Andromeda displays, request the native mode and let SDL's logical size
       preserve the 4:3 interface without clipping or stretching cards.
       A fullscreen rejection falls back to the old windowed path. */
    int screen_w=W,screen_h=H;
    SDL_DisplayMode display_mode;
    memset(&display_mode,0,sizeof(display_mode));
    if(SDL_GetCurrentDisplayMode(0,&display_mode)==0 &&
       mt_valid_display(display_mode.w,display_mode.h)){
        screen_w=display_mode.w; screen_h=display_mode.h;
    }
    if(screen_w!=W || screen_h!=H){
        a->win=SDL_CreateWindow("MustardTube v" MT_RELEASE,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
             screen_w,screen_h,SDL_WINDOW_SHOWN|SDL_WINDOW_FULLSCREEN_DESKTOP);
        if(!a->win){
            fprintf(stderr,"Native fullscreen unavailable (%dx%d): %s; falling back to 640x480\n",
                screen_w,screen_h,SDL_GetError());
        }
    }
    if(!a->win){
        a->win=SDL_CreateWindow("MustardTube v" MT_RELEASE,SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    }
    if(!a->win){fprintf(stderr,"Window: %s\n",SDL_GetError());goto done;}
    int actual_w=W,actual_h=H;
    SDL_GetWindowSize(a->win,&actual_w,&actual_h);
    MTViewport view=mt_viewport(actual_w,actual_h,W,H);
    fprintf(stderr,"MustardTube display: detected %dx%d; window %dx%d; logical %dx%d; viewport %d,%d %dx%d\n",
        screen_w,screen_h,actual_w,actual_h,W,H,view.x,view.y,view.w,view.h);
    /* Bilinear scaling improves the 45px logo and bounded 4-thumbnail cards;
     * no additional textures or upload passes. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
    a->r=SDL_CreateRenderer(a->win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!a->r)a->r=SDL_CreateRenderer(a->win,-1,SDL_RENDERER_SOFTWARE);
    if(!a->r){fprintf(stderr,"Renderer: %s\n",SDL_GetError());goto done;}
    SDL_SetRenderDrawBlendMode(a->r,SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(a->r,W,H);
    a->pad=find_pad();SDL_StartTextInput();
    if(a->page==HOME)start_home_refresh(a,false);
    if(a->page==RADIO)radio_open(a);
    if(a->page==RESULTS && !a->card_density)start_result_thumb_worker(a,true);
    if(a->page==RADIO_NOW)radio_now_load(a);
    if(a->page==RADIO){
        radio_open(a);
        if(show_radio){
            char last[4]={0},p[PATH_MAX];
            if(snprintf(p,sizeof(p),"%s/radio-last-country.txt",a->data_dir)<(int)sizeof(p)){
                FILE*f=fopen(p,"r");if(f){fgets(last,sizeof(last),f);fclose(f);}
            }
            if(strlen(last)==2){
                snprintf(a->radio_selected_country,sizeof(a->radio_selected_country),"%s",last);
                a->radio_mode=1;radio_load_stations(a);
                if(!a->station_count)radio_refresh(a,false,0);
                else start_radio_logo_worker(a,true);
            }
        }
    }
    if(screenfile){render(a);if(!screenshot(a,screenfile)){fprintf(stderr,"Screenshot failed: %s\n",SDL_GetError());goto done;}
        printf("Wrote preview: %s\n",screenfile);rc=0;goto done;}
    while(a->running){
        if(a->dirty){render(a);SDL_RenderPresent(a->r);a->dirty=false;}
        /* Details preview polls briefly while loading; all other pages sleep on input. */
        bool preview=a->page==PLAYER && a->current_is_youtube && !a->thumbnail &&
                     (time(NULL)-a->preview_started)<6;
        SDL_Event e;
        int received;
        if(a->page==RADIO_NOW || a->page==UPDATES)received=SDL_WaitEventTimeout(&e,1000);
        else received=(a->awaiting_stream || preview || a->page==HOME ||
            (a->page==RESULTS && a->result_thumb_pid>0) ||
            (a->page==RADIO && (a->radio_loading || a->radio_logo_pid>0)) ||
            ((a->page==RADIO || a->page==ABOUT) && a->nav_repeat_dir))?SDL_WaitEventTimeout(&e,60):SDL_WaitEvent(&e);
        if(received){handle_event(a,&e);while(SDL_PollEvent(&e))handle_event(a,&e);}
        if(a->page==UPDATES && SDL_GetTicks()-a->update_last_poll>=1000){
            a->update_last_poll=SDL_GetTicks();update_read_status(a);
        }
        if(a->page==RADIO_NOW){
            Uint32 tick=SDL_GetTicks();
            if((Uint32)(tick-a->radio_now_last_tick)>=1000){
                a->radio_now_last_tick=tick;
                /* The evdev helper saves radio changes in the same file.
                 * The Now Playing page refreshes that one small local file
                 * at most once per second; no new threads or controllers. */
                int former_mix=a->radio_mix_volume,former_game=a->game_mix_volume;
                radio_mix_load(a);game_mix_load(a);
                if(a->radio_mix_volume!=former_mix || a->game_mix_volume!=former_game)a->dirty=true;
                bool alive=radio_now_process_alive(a);
                if(alive!=a->radio_now_active){
                    a->radio_now_active=alive;
                    a->radio_now_blink=true;
                    a->dirty=true;
                }else if(alive){
                    a->radio_now_blink=!a->radio_now_blink;
                    a->dirty=true;
                }
            }
        }
        if((a->page==RADIO || a->page==ABOUT) && a->nav_repeat_dir){
            Uint32 now=SDL_GetTicks();
            if((int32_t)(now-a->nav_repeat_at)>=0){
                navigate(a,0,a->nav_repeat_dir);
                a->nav_repeat_at=now+85;
            }
        }
        if(a->page==RADIO && a->radio_loading)radio_poll(a);
        if(a->radio_logo_pid>0){int code;pid_t child=waitpid(a->radio_logo_pid,&code,WNOHANG);
            if(child!=0){a->radio_logo_pid=0;a->dirty=true;}
        }
        if(((a->page==HOME && a->home_count==0 && a->home_loading) ||
            (a->page==RADIO && a->radio_loading &&
             (a->radio_mode==0?a->country_count==0:
              a->radio_mode==2?a->favourite_count==0:a->station_count==0))) &&
           (Uint32)(SDL_GetTicks()-a->loading_dot_last_tick)>=420u){
            a->loading_dot_last_tick=SDL_GetTicks();
            a->dirty=true;
        }
        if(a->page==HOME && SDL_GetTicks()-a->home_last_poll>500){
            a->home_last_poll=SDL_GetTicks();poll_home_worker(a);
        }
        if(a->page==RESULTS && SDL_GetTicks()-a->result_thumb_last_poll>250){
            a->result_thumb_last_poll=SDL_GetTicks();poll_result_thumbnails(a);
        }
        if(a->page==PLAYER && a->current_is_youtube){
            if(preview)try_thumbnail(a);
            if(a->awaiting_stream){
                long elapsed=(long)(time(NULL)-a->wait_started);
                if(elapsed!=a->wait_display){a->wait_display=elapsed;a->dirty=true;}
                if(stream_is_prepared(a)){
                    char timing[PATH_MAX];
                    if(snprintf(timing,sizeof(timing),"%s/timing.log",a->data_dir)<(int)sizeof(timing)){
                        FILE*f=fopen(timing,"a");if(f){fprintf(f,"play_to_ready_ms=%u\n",SDL_GetTicks()-a->play_started);fclose(f);}
                    }
                    a->handoff_wasabi=true;a->running=false;
                }
                else if(stream_has_failed(a) || SDL_GetTicks()-a->play_started>66000){
                    stop_prefetch(a);a->awaiting_stream=false;a->stream_failed=true;a->dirty=true;
                }
            }
        }
    }
    rc=0;
done:
    handoff=a->handoff_wasabi || a->radio_handoff;
    handoff_kind=a->radio_handoff?a->radio_handoff_kind:a->current_is_youtube?YT_PLAY_EXIT:LOCAL_TEST_EXIT;
    if(a->request_youtube)handoff_kind=YT_SEARCH_EXIT;
    if(a->request_more)handoff_kind=YT_MORE_EXIT;
    if(a->request_clear)handoff_kind=CLEAR_CACHE_EXIT;
    if(a->request_clear_history)handoff_kind=CLEAR_HISTORY_EXIT;
    if(a->request_update_check)handoff_kind=UPDATE_CHECK_EXIT;
    if(a->request_update_download)handoff_kind=UPDATE_DOWNLOAD_EXIT;
    SDL_StopTextInput();
    shutdown_app(a); /* Always release video/input before the supervisor opens Wasabi. */
early:
    free(a->stations);
    free(a);
    /* Special exit code requests stock Wasabi from our shell supervisor.
       The supervisor waits and starts MustardTube again after Wasabi quits. */
    return (handoff_kind==YT_SEARCH_EXIT || handoff_kind==YT_MORE_EXIT || handoff_kind==CLEAR_CACHE_EXIT || handoff_kind==CLEAR_HISTORY_EXIT || handoff_kind==UPDATE_CHECK_EXIT || handoff_kind==UPDATE_DOWNLOAD_EXIT) ? handoff_kind : handoff ? handoff_kind : rc;
}
