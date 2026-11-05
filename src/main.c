#define _DEFAULT_SOURCE
/* MustardTube v0.9.11 Beta 2.1 - display compatibility preview; original video paths preserved.
   Copyright (c) 2026 MustardTube contributors. MIT licence.
   Home shows genuinely fetched YouTube discovery videos, not invented trending rankings. */
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
#define RADIO_MAX_STATIONS 80
#define RADIO_MAX_FAVORITES 32
#define RADIO_PLAY_EXIT 82
#define SEARCH_RESULTS 40
#define YT_MORE_EXIT 80
#define CLEAR_CACHE_EXIT 81
#define CLEAR_HISTORY_EXIT 83
#define YT_SEARCH_EXIT 78
#define YT_PLAY_EXIT 77
#define LOCAL_TEST_EXIT 79
#define TCOUNT 3
#define KEY_COUNT 39
#define MAX_QUERY 95

typedef enum { HOME, SEARCH, SAVED, RADIO, STYLE, ABOUT, PLAYER, RESULTS } Page;
typedef enum { FS_11, FS_13, FS_15, FS_19, FS_23, FS_29, FS_COUNT } FontSize;
typedef struct {
    char name[32];
    SDL_Color bg, bg2, surface, alt, selected, text, muted, accent, secondary, outline;
} Theme;
/* Input from the bundled yt-dlp resolver is parsed as bounded, validated TSV. */
typedef struct {char id[16], title[176], channel[84], duration[24];} YoutubeResult;

static const char *theme_files[TCOUNT] = {"midnight", "forest", "paper"};
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
    SDL_Texture *background_texture; /* ~1.2 MiB static background; rebuilt on theme change. */
    SDL_Texture *thumbnail; /* Selected video details thumbnail. */
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
    bool running, dirty, handoff_wasabi, request_youtube, request_more, wasabi_missing, more_available;
    bool current_is_youtube;
    bool awaiting_stream, stream_failed, request_clear, request_clear_history;
    long wait_display;
    time_t preview_started, wait_started;
    pid_t worker_pid;
    pid_t home_worker_pid;
    bool home_loading;
    Uint32 home_last_poll;
    time_t home_cache_stamp;
    int home_count;
    int home_category;
    YoutubeResult home_videos[HOME_VIDEOS];
    char home_feedback[100];
    RadioCountry countries[RADIO_MAX_COUNTRIES];
    RadioStation stations[RADIO_MAX_STATIONS], favourites[RADIO_MAX_FAVORITES];
    int country_count, country_idx, station_count, station_idx, favourite_count, favourite_idx;
    int radio_mode; /* 0 countries, 1 stations, 2 favourites */
    int about_scroll; /* Scrollable user manual and legal notice. */
    int nav_repeat_dir; /* Gamepad held-D-pad scrolling only in Radio/About. */
    Uint32 nav_repeat_at;
    pid_t radio_worker_pid;
    bool radio_loading, radio_handoff;
    pid_t radio_logo_pid;
    int radio_logo_mode, radio_logo_first;
    char radio_logo_country[4];
    SDL_Texture *radio_flag_atlas;
    SDL_Texture *radio_logos[7];
    char radio_logo_ids[7][45];
    char radio_selected_country[4], radio_status[130];
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
static void fill(App *a, int x,int y,int w,int h,SDL_Color c) {
    SDL_Rect q={x,y,w,h}; SDL_SetRenderDrawColor(a->r,c.r,c.g,c.b,c.a); SDL_RenderFillRect(a->r,&q);
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
static void circle(App *a,int cx,int cy,int radius,SDL_Color c) {
    for(int dy=-radius;dy<=radius;dy++) {
        int rx=(int)sqrt((double)(radius*radius-dy*dy));
        fill(a,cx-rx,cy+dy,2*rx+1,1,c);
    }
}
static void triangle(App *a,int cx,int cy,int size,SDL_Color c) {
    for(int j=-size;j<=size;j++) {
        int length=(j+size)*3/4+1;
        fill(a,cx-size/3,cy+j,length,1,c);
    }
}
static void print_text(App *a,FontSize fs,const char *str,int x,int y,SDL_Color c) {
    if(!str||!*str||!a->fonts[fs])return;
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
    int tw=0; if(a->fonts[fs])TTF_SizeUTF8(a->fonts[fs],str,&tw,NULL);
    print_text(a,fs,str,x+(width-tw)/2,y,c);
}
static void set_default_theme(Theme *t) {
    memset(t,0,sizeof(*t));strcpy(t->name,"Midnight");
    t->bg=color(10,17,32);t->bg2=color(17,30,52);t->surface=color(23,36,58);
    t->alt=color(29,48,73);t->selected=color(42,58,81);t->text=color(246,245,240);
    t->muted=color(165,181,201);t->accent=color(244,198,91);t->secondary=color(119,218,194);
    t->outline=color(52,69,95);
}
static void theme_field(Theme *t,const char *key,const char *v) {
    if(!strcmp(key,"name"))snprintf(t->name,sizeof(t->name),"%s",v);
#define TF(k,field) else if(!strcmp(key,k))t->field=from_hex(v,t->field)
    TF("background",bg);TF("background_bottom",bg2);TF("surface",surface);
    TF("surface_alt",alt);TF("surface_selected",selected);TF("text",text);
    TF("muted",muted);TF("accent",accent);TF("secondary",secondary);TF("outline",outline);
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
    if(a->background_texture){SDL_DestroyTexture(a->background_texture);a->background_texture=NULL;}
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
    char selected_id[sizeof(a->stations[0].id)]={0};
    if(a->station_count>0 && a->station_idx>=0 && a->station_idx<a->station_count)
        snprintf(selected_id,sizeof(selected_id),"%s",a->stations[a->station_idx].id);
    char path[PATH_MAX];if(snprintf(path,sizeof(path),"%s/radio-stations.tsv",a->data_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f)return;
    int n=0;char line[2150];
    while(n<RADIO_MAX_STATIONS && fgets(line,sizeof(line),f)){
        RadioStation st;if(!radio_parse_station(line,&st))continue;
        if(strcmp(st.country,a->radio_selected_country))continue;
        a->stations[n++]=st;
    }
    fclose(f);
    qsort(a->stations,(size_t)n,sizeof(a->stations[0]),compare_station);
    a->station_count=n;
    if(selected_id[0])for(int i=0;i<n;i++)if(!strcmp(selected_id,a->stations[i].id)){a->station_idx=i;break;}
    if(a->station_idx>=n)a->station_idx=0;a->dirty=true;
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
                SDL_RenderCopy(a->r,a->radio_flag_atlas,&src,&dst);return;
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
    if(!a->country_count)radio_refresh(a,true,0);
}
static bool radio_pick_station(App*a,const RadioStation*st){
    if(!st || !radio_url_ok(st->url))return false;
    char selected[1100];
    if(snprintf(selected,sizeof(selected),"%s\n%s\n",st->name,st->url)>=(int)sizeof(selected))return false;
    if(!write_data_file(a,"radio-selected.txt",selected))return false;
    write_data_file(a,"radio-last-country.txt",st->country);
    a->radio_handoff=true;a->running=false;return true;
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
    return quality==360?480:quality==480?720:360;
}
static void toggle_quality(App*a) {
    if(a->page!=PLAYER || !a->current_is_youtube || a->awaiting_stream)return;
    stop_prefetch(a);
    a->quality=next_quality(a->quality);
    write_data_file(a,"quality.txt",a->quality==360?"360\n":a->quality==480?"480\n":"720\n");
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
    if(!str || !a->fonts[fs] || maxwidth<=0)return;
    char clipped[180];safe_line(clipped,sizeof(clipped),str);
    int width=0;
    if(TTF_SizeUTF8(a->fonts[fs],clipped,&width,NULL)==0 && width<=maxwidth){print_text(a,fs,clipped,x,y,c);return;}
    size_t n=strlen(clipped);
    while(n>0){
        /* Keep truncation at a UTF-8 codepoint boundary. */
        n--;while(n>0&&((unsigned char)clipped[n]&0xC0)==0x80)n--;
        clipped[n]='\0';
        char tmp[180];snprintf(tmp,sizeof(tmp),"%s...",clipped);
        if(TTF_SizeUTF8(a->fonts[fs],tmp,&width,NULL)==0 && width<=maxwidth){print_text(a,fs,tmp,x,y,c);return;}
    }
}
static void save_settings(App *a) {
    if(!*a->pref_dir)return;
    char p[PATH_MAX];if(snprintf(p,sizeof(p),"%ssettings.ini",a->pref_dir)>=(int)sizeof(p))return;
    FILE *f=fopen(p,"w");if(!f)return;
    fprintf(f,"theme=%s\n",theme_files[a->theme_id]);
    fclose(f);
}
static void load_settings(App *a) {
    char*p=SDL_GetPrefPath("MustardOS","MustardTube");
    if(!p)return;
    snprintf(a->pref_dir,sizeof(a->pref_dir),"%s",p);
    SDL_free(p);
    char path[PATH_MAX];if(snprintf(path,sizeof(path),"%ssettings.ini",a->pref_dir)>=(int)sizeof(path))return;
    FILE*f=fopen(path,"r");if(!f)return;
    char line[180];while(fgets(line,sizeof(line),f)) {
        if(!strncmp(line,"theme=",6))for(int i=0;i<TCOUNT;i++)if(!strncmp(line+6,theme_files[i],strlen(theme_files[i])))a->theme_id=i;
    }
    fclose(f);
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
/* Render the decorative background once per theme, not on every key press.
   A single 640x480 ARGB texture costs 1.17 MiB but replaces ~900 small draw
   calls per frame on software SDL renderers. Preserve the old look exactly. */
static void paint_surface_circle(SDL_Surface *s,int cx,int cy,int radius,SDL_Color col) {
    const Uint32 pixel=SDL_MapRGBA(s->format,col.r,col.g,col.b,255);
    for(int dy=-radius;dy<=radius;dy++) {
        const int rx=(int)sqrt((double)(radius*radius-dy*dy));
        SDL_Rect row={cx-rx,cy+dy,2*rx+1,1};
        SDL_FillRect(s,&row,pixel); /* SDL clips to 640x480 automatically. */
    }
}
static bool prepare_background(App *a) {
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_ARGB8888);
    if(!s)return false;
    for(int y=0;y<H;y++) {
        SDL_Color c=blend(a->theme.bg,a->theme.bg2,y,H);
        SDL_Rect row={0,y,W,1};
        SDL_FillRect(s,&row,SDL_MapRGBA(s->format,c.r,c.g,c.b,255));
    }
    paint_surface_circle(s,605,192,111,blend(a->theme.bg2,a->theme.alt,2,5));
    paint_surface_circle(s,614,180,79,blend(a->theme.bg2,a->theme.surface,1,2));
    a->background_texture=SDL_CreateTextureFromSurface(a->r,s);
    SDL_FreeSurface(s);
    return a->background_texture!=NULL;
}
static void draw_background(App *a) {
    if(!a->background_texture)prepare_background(a);
    if(a->background_texture) {
        SDL_RenderCopy(a->r,a->background_texture,NULL,NULL);
        return;
    }
    /* If texture creation fails, draw using the original low-memory path. */
    for(int y=0;y<H;y++)fill(a,0,y,W,1,blend(a->theme.bg,a->theme.bg2,y,H));
    circle(a,605,192,111,blend(a->theme.bg2,a->theme.alt,2,5));
    circle(a,614,180,79,blend(a->theme.bg2,a->theme.surface,1,2));
}
static void draw_header(App *a) {
    Theme*t=&a->theme;
    rounded(a,18,17,39,39,12,t->accent);
    rounded(a,26,25,23,23,7,blend(t->accent,t->bg,1,6));
    triangle(a,37,36,8,t->bg);
    print_text(a,FS_19,"Mustard",65,17,t->text);
    int name_width=80;
    TTF_SizeUTF8(a->fonts[FS_19],"Mustard",&name_width,NULL);
    print_text(a,FS_19,"Tube",65+name_width,17,t->accent);
    print_text(a,FS_11,"Built for muOS",66,41,t->muted);
    /* Six native tabs reuse the original SDL controller handling. */
    for(int i=0;i<NAV_TAB_COUNT;i++){
        int x=279+i*57;bool selected=(int)a->page==i;
        if(selected)outline(a,x,18,55,36,11,t->outline,t->selected);
        text_center(a,FS_11,page_names[i],x,29,55,selected?t->accent:t->muted);
    }
    fill(a,18,66,604,1,t->outline);
}
static void draw_home(App*a) {
    Theme*t=&a->theme;
    char heading[72];snprintf(heading,sizeof(heading),"Videos / %s",home_category_labels[a->home_category]);
    print_text(a,FS_23,heading,20,80,t->text);
    print_text(a,FS_11,a->home_loading?"Loading selected category...  |  X Refresh":
        "LEFT/RIGHT Category   UP/DOWN Videos   X Refresh",20,108,t->muted);
    for(int i=0;i<HOME_CATEGORIES;i++){
        int x=20+i*101;bool active=i==a->home_category;
        outline(a,x,128,96,27,8,active?t->accent:t->outline,active?t->selected:t->surface);
        text_center(a,FS_11,home_category_labels[i],x,136,96,active?t->accent:t->text);
    }
    if(a->home_count==0){
        outline(a,18,164,604,254,16,t->outline,t->surface);
        rounded(a,281,184,77,77,20,t->alt);
        circle(a,319,222,27,t->accent);triangle(a,326,222,11,t->bg);
        text_center(a,FS_19,a->home_loading?"Loading video cards...":"No videos loaded yet",20,285,600,t->text);
        print_ellipsis(a,FS_13,a->home_feedback[0]?a->home_feedback:
            a->home_category==2?"Watch a video to start your local recommendations.":
            "Check Wi-Fi. Press X to retry or switch category.",52,328,544,t->muted);
        return;
    }
    for(int i=0;i<a->home_count&&i<HOME_VIDEOS;i++){
        int x=18,y=163+i*64;bool active=a->home_idx==i;
        YoutubeResult*v=&a->home_videos[i];
        outline(a,x,y,604,60,9,active?t->accent:t->outline,active?t->selected:t->surface);
        rounded(a,x+7,y+5,104,50,6,blend(t->alt,t->surface,1,3));
        if(a->home_thumbs[i]){SDL_Rect dst={x+7,y+5,104,50};SDL_RenderCopy(a->r,a->home_thumbs[i],NULL,&dst);}
        else{circle(a,x+58,y+29,16,t->accent);triangle(a,x+63,y+29,6,t->bg);}
        print_ellipsis(a,FS_15,v->title,x+121,y+7,465,t->text);
        print_ellipsis(a,FS_11,v->channel,x+121,y+35,378,t->muted);
        print_ellipsis(a,FS_11,v->duration,x+532,y+35,54,t->secondary);
    }
}
static void draw_search(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"Search",20,83,t->text);
    print_text(a,FS_13,"Enter a title, creator, or topic using the D-pad.",20,115,t->muted);
    outline(a,19,146,602,51,13,t->accent,t->surface);
    print_text(a,FS_15,"?",35,158,t->accent);
    if(!a->query[0])print_text(a,FS_15,"Search for videos...",60,160,t->muted);
    else print_text(a,FS_15,a->query,60,160,t->text);
    print_text(a,FS_11,a->search_feedback[0]?a->search_feedback:"Select SEARCH to look up live YouTube videos.",23,204,t->secondary);
    for(int i=0;i<36;i++){
        int row=i/9,col=i%9,x=25+col*66,y=227+row*40;
        bool sel=a->keyboard_idx==i;
        rounded(a,x,y,60,34,8,sel?t->accent:t->alt);
        char label[2]={keyboard_chars[i],0};
        text_center(a,FS_15,label,x,y+7,60,sel?t->bg:t->text);
    }
    const char* names[3]={"SPACE","DELETE","SEARCH"};
    for(int j=0;j<3;j++){
        int x=25+j*199;bool sel=a->keyboard_idx==36+j;
        rounded(a,x,391,191,34,8,sel?t->accent:t->alt);
        text_center(a,FS_13,names[j],x,400,191,sel?t->bg:t->text);
    }
}
static void draw_saved(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"Recent activity",20,87,t->text);
    print_text(a,FS_13,"Return to your last search without reloading YouTube.",20,120,t->muted);
    outline(a,19,158,602,223,15,t->outline,t->surface);
    print_text(a,FS_11,"LAST SEARCH",37,180,t->secondary);
    print_ellipsis(a,FS_19,a->query[0]?a->query:"Nothing searched yet",37,210,550,t->text);
    for(int i=0;i<2;i++) {
        int y=260+i*54;
        bool active=a->saved_idx==i;
        outline(a,36,y,568,45,10,active?t->accent:t->outline,active?t->selected:t->alt);
        print_text(a,FS_15,i==0?"Search again":"Open last results",52,y+11,t->text);
        print_text(a,FS_11,i==0?"A  SEARCH":"A  OPEN",488,y+15,active?t->accent:t->muted);
    }
    print_text(a,FS_11,"Previous results are stored only if a search succeeded.",36,394,t->muted);
}
static void draw_results(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"YouTube results",20,82,t->text);
    print_text(a,FS_13,a->more_available?"A Details   |   X Load more   |   B Edit search":"A Details   |   B Edit search",20,115,t->muted);
    print_ellipsis(a,FS_11,a->status_message,20,136,588,t->secondary);
    if(a->result_count==0){
        rounded(a,19,159,602,220,16,t->surface);
        print_text(a,FS_19,"No videos returned",46,208,t->text);
        print_ellipsis(a,FS_13,a->status_message[0]?a->status_message:"Check Wi-Fi and try another search.",46,248,541,t->muted);
        return;
    }
    const int perpage=4;
    int first=(a->result_idx/perpage)*perpage;
    for(int i=0;i<perpage;i++){
        int idx=first+i;if(idx>=a->result_count)break;
        YoutubeResult *v=&a->results[idx];int y=154+i*68;
        bool active=idx==a->result_idx;
        outline(a,19,y,602,62,11,active?t->accent:t->outline,active?t->selected:t->surface);
        /* Same thumbnail style as Home; only four visible images decoded. */
        rounded(a,27,y+5,104,52,7,blend(t->alt,t->surface,1,3));
        if(a->result_thumbs[i]){SDL_Rect dst={27,y+5,104,52};SDL_RenderCopy(a->r,a->result_thumbs[i],NULL,&dst);}
        else {circle(a,79,y+31,16,t->accent);triangle(a,84,y+31,7,t->bg);}
        print_ellipsis(a,FS_15,v->title,140,y+8,447,t->text);
        print_ellipsis(a,FS_11,v->channel,140,y+36,375,t->muted);
        print_ellipsis(a,FS_11,v->duration,550,y+36,56,t->secondary);
    }
    char pagination[96];
    int last=first+perpage;if(last>a->result_count)last=a->result_count;
    snprintf(pagination,sizeof(pagination),"%d-%d of %d  %s",first+1,last,a->result_count,
       a->more_available?"| X: +8":"| end");
    text_center(a,FS_11,pagination,211,421,218,t->muted);
}
/* The list begins below its heading and status. Seven compact rows fit
 * safely inside y=178..401, leaving y=413 for per-view actions. */
#define RADIO_LIST_Y 178
#define RADIO_ROW_H 32
#define RADIO_VISIBLE 7
static void draw_radio(App*a){
    Theme*t=&a->theme;
    print_text(a,FS_23,"Internet Radio",20,82,t->text);
    print_text(a,FS_11,"Browse countries and stations | Played by Wasabi",20,112,t->muted);
    if(a->radio_loading)print_text(a,FS_11,"Loading radio directory...",20,134,t->secondary);
    else if(a->radio_status[0])print_ellipsis(a,FS_11,a->radio_status,20,134,590,t->secondary);
    bool countries=a->radio_mode==0, fav=a->radio_mode==2;
    int n=countries?a->country_count:fav?a->favourite_count:a->station_count;
    int idx=countries?a->country_idx:fav?a->favourite_idx:a->station_idx;
    int first=idx/RADIO_VISIBLE*RADIO_VISIBLE;
    int last=first+RADIO_VISIBLE;if(last>n)last=n;
    print_text(a,FS_13,countries?"COUNTRIES A-Z":fav?"FAVOURITES A-Z":"STATIONS A-Z",20,156,t->accent);
    char location[72];
    if(n)snprintf(location,sizeof(location),"%d-%d / %d",first+1,last,n);
    else snprintf(location,sizeof(location),"0 stations");
    print_text(a,FS_11,location,526,157,t->muted);
    if(n>RADIO_VISIBLE){
        int track_y=RADIO_LIST_Y,track_h=RADIO_VISIBLE*RADIO_ROW_H-3;
        rounded(a,617,track_y,3,track_h,1,t->outline);
        int thumb_h=(RADIO_VISIBLE*track_h)/n;
        if(thumb_h<16)thumb_h=16;
        int thumb_y=track_y+(n>RADIO_VISIBLE?(first*(track_h-thumb_h)/(n-RADIO_VISIBLE)):0);
        rounded(a,616,thumb_y,5,thumb_h,2,t->accent);
    }
    for(int j=0;j<RADIO_VISIBLE && first+j<n;j++){
        int ix=first+j,y=RADIO_LIST_Y+j*RADIO_ROW_H;
        bool active=ix==idx;
        outline(a,20,y,593,29,7,active?t->accent:t->outline,active?t->selected:t->surface);
        if(countries){
            RadioCountry*c=&a->countries[ix];
            radio_draw_country_flag(a,c->code,30,y+3);
            print_ellipsis(a,FS_13,c->name,77,y+5,435,t->text);
            char count[32];snprintf(count,sizeof(count),"%d",c->count);
            print_text(a,FS_11,count,543,y+8,t->muted);
        }else{
            RadioStation*st=fav?&a->favourites[ix]:&a->stations[ix];
            SDL_Texture*logo=radio_station_logo(a,j,st);
            if(logo){SDL_Rect d={30,y+2,25,25};SDL_RenderCopy(a->r,logo,NULL,&d);}
            else {rounded(a,30,y+2,25,25,5,t->bg2);print_text(a,FS_11,"FM",33,y+9,t->secondary);}
            print_ellipsis(a,FS_13,st->name,63,y+5,445,t->text);
            print_ellipsis(a,FS_11,st->codec,547,y+8,54,t->muted);
            if(radio_is_favourite(a,st))print_text(a,FS_11,"*",521,y+7,t->accent);
        }
    }
    if(!n)print_ellipsis(a,FS_13,countries?"No countries. Press X to retry.":fav?"No saved stations yet.":a->radio_loading?"Loading stations...":"No stations. Press X to retry.",28,235,565,t->text);
    print_text(a,FS_11,countries?"A Open    X Refresh    Y Favourites    </> Page":
        fav?"A Play    Y Unfavourite    B Countries    </> Page":
        "A Play    X More    Y Favourite    </> Page",20,415,t->muted);
}
static void draw_style(App*a) {
    Theme*t=&a->theme;
    print_text(a,FS_23,"Settings",20,85,t->text);
    print_ellipsis(a,FS_11,a->status_message[0]?a->status_message:"Themes, storage and app performance",20,116,600,t->muted);
    print_text(a,FS_13,"APPEARANCE",20,138,t->secondary);
    for(int i=0;i<TCOUNT;i++){
        int x=19+i*206,y=161;bool active=a->style_idx==i;
        outline(a,x,y,191,154,14,active?t->accent:t->outline,active?t->selected:t->surface);
        SDL_Color p=i==0?color(14,25,43):i==1?color(24,55,47):color(231,237,230);
        SDL_Color a1=i==0?color(244,198,91):i==1?color(216,232,142):color(223,174,63);
        SDL_Color p2=i==0?color(49,69,101):i==1?color(58,104,82):color(184,210,198);
        rounded(a,x+11,y+11,169,83,10,p);
        rounded(a,x+21,y+23,85,12,4,p2);
        rounded(a,x+21,y+43,125,9,4,p2);
        rounded(a,x+21,y+64,67,12,5,a1);
        print_text(a,FS_15,i==0?"Midnight":i==1?"Forest":"Paper",x+13,y+107,t->text);
        if(i==a->theme_id)print_text(a,FS_11,"ACTIVE",x+115,y+112,t->secondary);
    }
    print_text(a,FS_13,"STORAGE & CACHE",20,321,t->secondary);
    rounded(a,20,346,600,76,13,t->surface);
    print_text(a,FS_13,"A Theme     X Clear cache     Y Clear viewing history",36,351,t->text);
    print_text(a,FS_11,"Clear Cache keeps settings, favourites and viewing history",36,379,t->muted);
    print_text(a,FS_11,"CPU governor: use muOS Application Options (Performance)",36,399,t->muted);
}
/* Self-contained manual: no WebView, markdown runtime or network dependency.
 * Each line is bounded for the 640x480 display; one logical line per row. */
typedef struct {const char *text; bool heading;} AboutLine;
static const AboutLine ABOUT_LINES[]={
    {"WELCOME",true},
    {"MustardTube - lightweight video and Internet Radio",false},
    {"Version 0.9.11 Beta 2.1 | PapaZolaRis",false},
    {"Independent community project for muOS.",false},
    {"",false},
    {"REQUIREMENTS",true},
    {"muOS 2606.0 Andromeda; multiple display formats.",false},
    {"Wi-Fi for online video and radio; stock Wasabi player.",false},
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
    {"A opens details; X refreshes the chosen feed.",false},
    {"Cards are search-based, not official YouTube Trending.",false},
    {"For You uses local viewing signals where available.",false},
    {"",false},
    {"SEARCH",true},
    {"Use D-pad and A to type with the on-screen keyboard.",false},
    {"START submits; X loads more on results screens.",false},
    {"A on results opens the video detail page.",false},
    {"Up to 40 search results; thumbnails load as needed.",false},
    {"",false},
    {"PLAYBACK & QUALITY",true},
    {"A on video details plays through stock Wasabi.",false},
    {"Y cycles 360p (default), 480p and optional 720p.",false},
    {"360p/480p seeking works on compatible recordings.",false},
    {"Recorded 720p: EXPERIMENTAL - NO SEEK.",false},
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
    {"Streams are handed to Wasabi; no login required.",false},
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
    {"MustardTube source: MIT licence.",false},
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
    print_text(a,FS_23,"About / User Guide",20,83,t->text);
    print_text(a,FS_11,"Manual, controls, troubleshooting, legal notice",20,115,t->muted);
    char progress[80];
    int last=a->about_scroll+ABOUT_VISIBLE;if(last>ABOUT_COUNT)last=ABOUT_COUNT;
    snprintf(progress,sizeof(progress),"%d-%d / %d",a->about_scroll+1,last,ABOUT_COUNT);
    print_text(a,FS_11,progress,527,140,t->secondary);
    print_text(a,FS_11,"UP/DOWN Scroll (hold)   LEFT/RIGHT Page",20,140,t->accent);
    outline(a,18,159,604,253,12,t->outline,t->surface);
    for(int i=0;i<ABOUT_VISIBLE && a->about_scroll+i<ABOUT_COUNT;i++){
        const AboutLine*line=&ABOUT_LINES[a->about_scroll+i];
        int y=167+i*18;
        if(line->heading){
            rounded(a,29,y-1,568,18,4,t->bg2);
            print_ellipsis(a,FS_11,line->text,37,y,550,t->accent);
        }else{
            print_ellipsis(a,FS_11,line->text,37,y,551,t->text);
        }
    }
    int track_y=164,track_h=241,thumb_h=32;
    rounded(a,608,track_y,4,track_h,2,t->outline);
    int max=ABOUT_COUNT-ABOUT_VISIBLE;
    int thumb_y=track_y+(max>0?a->about_scroll*(track_h-thumb_h)/max:0);
    rounded(a,607,thumb_y,6,thumb_h,3,t->accent);
    print_text(a,FS_11,"X/Y Jump sections     LB/RB Tabs     B Home",20,416,t->muted);
}
static void draw_player(App*a) {
    Theme*t=&a->theme;
    rounded(a,19,79,602,276,16,color(8,12,20));
    if(a->current_is_youtube && a->thumbnail){
        SDL_Rect dst={113,86,414,233};
        SDL_RenderCopy(a->r,a->thumbnail,NULL,&dst);
        rounded(a,297,177,46,46,23,t->accent);
        triangle(a,322,200,11,t->bg);
    }else{
        circle(a,320,187,42,t->surface);
        circle(a,320,187,35,t->accent);
        triangle(a,327,187,13,t->bg);
    }
    rounded(a,206,283,229,34,9,t->accent);
    text_center(a,FS_13,a->awaiting_stream?"PREPARING VIDEO...":a->current_is_youtube?"A  PLAY":"A  TEST SOUND",206,292,229,t->bg);
    if(a->current_is_youtube) {
        print_ellipsis(a,FS_19,a->current_video.title,26,362,566,t->text);
        char line[160];
        if(a->stream_failed)snprintf(line,sizeof(line),a->quality==720?
            "HD unavailable. Press Y for 360p, or A to retry.":
            "Stream unavailable. Press A to retry.");
        else if(a->awaiting_stream)snprintf(line,sizeof(line),"Preparing video... %lds  |  B Cancel",(long)(time(NULL)-a->wait_started));
        else if(stream_is_prepared(a)){
            char kind_path[PATH_MAX];char kind[20]={0};
            if(snprintf(kind_path,sizeof(kind_path),"%s/stream-kind.txt",a->data_dir)<(int)sizeof(kind_path)){
                FILE*kind_file=fopen(kind_path,"r");
                if(kind_file){fgets(kind,sizeof(kind),kind_file);fclose(kind_file);}
            }
            if(!strncmp(kind,"live",4))snprintf(line,sizeof(line),"LIVE ready  |  %s",a->current_video.channel);
            else if(a->quality==720)snprintf(line,sizeof(line),"Recorded HD 720p ready  |  %s",a->current_video.channel);
            else snprintf(line,sizeof(line),"Stream ready  |  %s",a->current_video.channel);
        }
        else if(a->current_video.channel[0])snprintf(line,sizeof(line),"%s  %s",a->current_video.channel,a->current_video.duration);
        else snprintf(line,sizeof(line),"Press A to play.");
        print_ellipsis(a,FS_13,line,26,391,560,t->muted);
        const char *quality_label=a->quality==360?
            "Y Quality: Fast 360p  |  MENU + START returns here":a->quality==480?
            "Y Quality: Clear 480p  |  MENU + START returns here":
            "Y Quality: HD 720p (experimental; live + recorded)";
        print_text(a,FS_11,quality_label,26,412,t->secondary);
    }else{
        print_text(a,FS_19,"Test video + audio",26,364,t->text);
        print_text(a,FS_13,"Local 10-second sample with audible test tone.",26,392,t->muted);
        print_text(a,FS_11,"H.264 / AAC  -  MENU + START to return here.",26,413,t->secondary);
    }
    if(a->wasabi_missing) {
        rounded(a,101,229,438,37,9,t->surface);
        text_center(a,FS_13,"Wasabi not found. Use muOS Andromeda.",101,238,438,t->accent);
    }
}
static void draw_footer(App*a) {
    Theme*t=&a->theme;
    fill(a,0,437,640,43,t->bg);
    fill(a,18,437,604,1,t->outline);
    const char *hint=a->page==HOME?"LEFT/RIGHT Category  UP/DOWN Videos  A Details  X Refresh  LB/RB Tabs":
     a->page==SEARCH?"D-PAD Keyboard    A Type    START Search    L/R Tabs":
     a->page==SAVED?"UP/DOWN  Select     A  Open     LB/RB  Tabs":
     a->page==RADIO?"Hold UP/DOWN Scroll   LEFT/RIGHT Page   A Select   X More   Y Fav":
     a->page==STYLE?"A Theme  X Clear cache  Y Clear history  LB/RB Tabs":
     a->page==ABOUT?"Hold UP/DOWN Scroll   LEFT/RIGHT Page   X/Y Sections   B Home":
     a->page==RESULTS?(a->more_available?"UP/DOWN Browse    A Details    X More    B Search":"UP/DOWN Browse    A Details    B Search"):
     "A Play    B Back    Y Quality    MENU + START Return";
    print_text(a,FS_11,hint,20,451,t->muted);
    print_text(a,FS_11,"v0.9.11b2.1",556,452,t->secondary);
}
static void render(App*a) {
    draw_background(a);draw_header(a);
    switch(a->page){case HOME:draw_home(a);break;case SEARCH:draw_search(a);break;
    case SAVED:draw_saved(a);break;case RADIO:draw_radio(a);break;case STYLE:draw_style(a);break;case ABOUT:draw_about(a);break;
    case PLAYER:draw_player(a);break;case RESULTS:draw_results(a);break;}
    draw_footer(a);
}
static void set_page(App*a,Page p) {
    a->nav_repeat_dir=0; /* A held D-pad never changes the next tab. */
    if(a->page==HOME && p!=HOME)stop_home_worker(a);
    if(a->page==RESULTS && p!=RESULTS)stop_result_thumb_worker(a);
    if(a->page==RADIO && p!=RADIO){stop_radio_worker(a);stop_radio_logo_worker(a);drop_radio_logos(a);}
    if(p!=PLAYER && p!=RESULTS){stop_prefetch(a);a->awaiting_stream=false;}
    a->page=p;a->dirty=true;
    if(p==HOME){load_home_file(a);if(a->home_worker_pid==0)start_home_refresh(a,false);}
    if(p==RESULTS)start_result_thumb_worker(a,true);
    if(p==RADIO)radio_open(a);
}
static void cycle_page(App*a,int step) {
    a->status_message[0]=0;
    if(a->page==PLAYER || a->page==RESULTS)set_page(a,a->from_page);
    int p=((int)a->page+step+NAV_TAB_COUNT)%NAV_TAB_COUNT;set_page(a,(Page)p);
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
        int first=(a->result_idx/RESULTS_VISIBLE)*RESULTS_VISIBLE;
        a->result_idx=(a->result_idx+dy+dx+n*3)%n;
        if(first!=(a->result_idx/RESULTS_VISIBLE)*RESULTS_VISIBLE)start_result_thumb_worker(a,true);
    }}
    else if(a->page==STYLE){a->style_idx=(a->style_idx+dx+dy+TCOUNT*3)%TCOUNT;}
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
            a->radio_mode=1;a->station_count=0;a->station_idx=0;stop_radio_logo_worker(a);drop_radio_logos(a);radio_refresh(a,false,0);
        }else if(a->radio_mode==1 && a->station_count>0){
            if(!radio_pick_station(a,&a->stations[a->station_idx]))snprintf(a->radio_status,sizeof(a->radio_status),"Station address unavailable.");
        }else if(a->radio_mode==2 && a->favourite_count>0){
            if(!radio_pick_station(a,&a->favourites[a->favourite_idx]))snprintf(a->radio_status,sizeof(a->radio_status),"Station address unavailable.");
        }
    }
    else if(a->page==STYLE){if(read_theme(a,a->style_idx))save_settings(a);}
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
    if(a->page==PLAYER){
        stop_prefetch(a);a->awaiting_stream=false;
        if(a->current_is_youtube){write_data_file(a,"selected.txt","");drop_thumbnail(a);}
        set_page(a,a->from_page);
    }
    else if(a->page==RESULTS)set_page(a,SEARCH);
    else if(a->page==RADIO && a->radio_mode!=0){stop_radio_worker(a);stop_radio_logo_worker(a);drop_radio_logos(a);a->radio_mode=0;a->dirty=true;}
    else if(a->page!=HOME)set_page(a,HOME);
    else a->running=false;
}
static void radio_x(App*a){
    if(a->radio_loading)return;
    if(a->radio_mode==0)radio_refresh(a,true,0);
    else if(a->radio_mode==1){
        int offset=((a->station_count+19)/20)*20;
        if(offset>=RADIO_MAX_STATIONS){snprintf(a->radio_status,sizeof(a->radio_status),"Reached 80 station limit.");a->dirty=true;}
        else radio_refresh(a,false,(offset/20)*20);
    }
}
static void radio_y(App*a){
    if(a->radio_mode==0){a->radio_mode=2;radio_load_favourites(a);start_radio_logo_worker(a,true);a->dirty=true;return;}
    if(a->radio_mode==1&&a->station_count>0)radio_toggle_favourite(a,&a->stations[a->station_idx]);
    else if(a->radio_mode==2&&a->favourite_count>0)radio_toggle_favourite(a,&a->favourites[a->favourite_idx]);
}
static void keydown(App*a,SDL_Keycode key) {
    switch(key){
      case SDLK_UP:navigate(a,0,-1);break;
      case SDLK_DOWN:navigate(a,0,1);break;
      case SDLK_LEFT:navigate(a,-1,0);break;
      case SDLK_RIGHT:navigate(a,1,0);break;
      case SDLK_ESCAPE:back(a);break;
      case SDLK_RETURN:case SDLK_KP_ENTER:if(a->page==SEARCH)submit_search(a);else activate(a);break;
      case SDLK_BACKSPACE:if(a->page==SEARCH)remove_char(a);else back(a);break;
      case SDLK_TAB:cycle_page(a,1);break;
      case SDLK_PAGEUP:case SDLK_LEFTBRACKET:cycle_page(a,-1);break;
      case SDLK_PAGEDOWN:case SDLK_RIGHTBRACKET:cycle_page(a,1);break;
      case SDLK_x:if(a->page==ABOUT)about_jump_section(a,1);
        else if(a->page==RADIO)radio_x(a);
        else if(a->page==HOME)start_home_refresh(a,true);
        else if(a->page==RESULTS)request_more_results(a);
        else if(a->page==STYLE)request_clear_cache(a);
        break;
      case SDLK_y:if(a->page==ABOUT)about_jump_section(a,-1);else if(a->page==STYLE)request_clear_history(a);else if(a->page==RADIO)radio_y(a);else if(a->page==HOME)home_set_category(a,(a->home_category+1)%HOME_CATEGORIES);else toggle_quality(a);break;
      case SDLK_F5:read_theme(a,a->theme_id);break;
      case SDLK_F1:set_page(a,HOME);break;
      case SDLK_F2:set_page(a,SEARCH);break;
      case SDLK_F3:set_page(a,SAVED);break;
      case SDLK_F4:set_page(a,RADIO);break;
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
    if((a->page==RADIO || a->page==ABOUT) &&
       (button==SDL_CONTROLLER_BUTTON_DPAD_UP || button==SDL_CONTROLLER_BUTTON_DPAD_DOWN)){
        a->nav_repeat_dir=button==SDL_CONTROLLER_BUTTON_DPAD_UP?-1:1;
        a->nav_repeat_at=SDL_GetTicks()+320;
    }
    switch(button){
      case SDL_CONTROLLER_BUTTON_DPAD_UP:navigate(a,0,-1);break;
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN:navigate(a,0,1);break;
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT:navigate(a,-1,0);break;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:navigate(a,1,0);break;
      case SDL_CONTROLLER_BUTTON_A:activate(a);break;
      case SDL_CONTROLLER_BUTTON_B:back(a);break;
      case SDL_CONTROLLER_BUTTON_X:if(a->page==ABOUT)about_jump_section(a,1);
        else if(a->page==RADIO)radio_x(a);
        else if(a->page==HOME)start_home_refresh(a,true);
        else if(a->page==RESULTS)request_more_results(a);
        else if(a->page==STYLE)request_clear_cache(a);
        break;
      case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:cycle_page(a,-1);break;
      case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:cycle_page(a,1);break;
      case SDL_CONTROLLER_BUTTON_START:if(a->page==SEARCH)submit_search(a);else set_page(a,HOME);break;
      case SDL_CONTROLLER_BUTTON_Y:if(a->page==ABOUT)about_jump_section(a,-1);else if(a->page==STYLE)request_clear_history(a);else if(a->page==RADIO)radio_y(a);else if(a->page==HOME)home_set_category(a,(a->home_category+1)%HOME_CATEGORIES);else if(a->page==PLAYER)toggle_quality(a);else set_page(a,STYLE);break;
      default:break;
    }
}
static void mouse_down(App*a,int x,int y){
    if(y>=18&&y<=55&&x>=279&&x<622){int tab=(x-279)/57;if(tab<NAV_TAB_COUNT)set_page(a,(Page)tab);return;}
    if(a->page==HOME&&y>=128&&y<155&&x>=20&&x<622){
        int cat=(x-20)/101;if(cat<HOME_CATEGORIES)home_set_category(a,cat);
    }else if(a->page==HOME&&y>=163&&y<419){
        int pick=(y-163)/64;
        if(pick<a->home_count){a->home_idx=pick;activate(a);}
    }else if(a->page==RADIO&&y>=RADIO_LIST_Y&&y<RADIO_LIST_Y+RADIO_ROW_H*RADIO_VISIBLE){
        int pos=(y-RADIO_LIST_Y)/RADIO_ROW_H,first=0;
        if(a->radio_mode==0)first=(a->country_idx/RADIO_VISIBLE)*RADIO_VISIBLE;
        else if(a->radio_mode==1)first=(a->station_idx/RADIO_VISIBLE)*RADIO_VISIBLE;
        else first=(a->favourite_idx/RADIO_VISIBLE)*RADIO_VISIBLE;
        int chosen=first+pos;
        if(a->radio_mode==0 && chosen<a->country_count){a->country_idx=chosen;activate(a);}
        else if(a->radio_mode==1 && chosen<a->station_count){a->station_idx=chosen;activate(a);}
        else if(a->radio_mode==2 && chosen<a->favourite_count){a->favourite_idx=chosen;activate(a);}
    }else if(a->page==ABOUT && y>=159&&y<412){
        about_scroll_by(a,y<285?-ABOUT_VISIBLE:ABOUT_VISIBLE);
    }else if(a->page==STYLE&&y>=161&&y<315){int id=(x-19)/206;
        if(id>=0&&id<TCOUNT){a->style_idx=id;activate(a);}}
    else if(a->page==SEARCH){
        if(y>=227&&y<381&&x>=25&&x<619){int col=(x-25)/66,row=(y-227)/40;
            if(col<9&&row<4){a->keyboard_idx=row*9+col;keyboard_activate(a);}}
        else if(y>=391&&y<425&&x>=25&&x<620){int col=(x-25)/199;
            if(col<3){a->keyboard_idx=36+col;keyboard_activate(a);}}
    }else if(a->page==SAVED&&y>=260&&y<360){a->saved_idx=(y-260)/54;activate(a);}
    else if(a->page==RESULTS&&y>=154&&y<426){
        int base=(a->result_idx/4)*4;int pos=base+(y-154)/68;
        if(pos<a->result_count){a->result_idx=pos;activate(a);}}
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
    if(a->background_texture)SDL_DestroyTexture(a->background_texture);
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
    bool handoff=false, resume_player=false, show_results=false, show_style=false, show_about=false, show_radio=false;
    int handoff_kind=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--version")){puts("MustardTube 0.9.11 Beta 2.1 display compatibility preview (Home categories, Search thumbnails, Internet Radio)");return 0;}
        if(!strcmp(argv[i],"--self-test")){
            if(strlen(keyboard_chars)!=36||HOME_VIDEOS!=4||HOME_CATEGORIES!=6||TCOUNT!=3||NAV_TAB_COUNT!=6){fputs("Self-test FAIL\n",stderr);return 1;}
            if(SEARCH_RESULTS!=40){fputs("Self-test FAIL: result cap\n",stderr);return 1;}
            puts("MustardTube 0.9.11 Beta 2.1 self-test PASS: 6 tabs, 6 Home categories, 4 visible Search thumbnails, 40 result cap, 3 themes");return 0;
        }
        if(!strcmp(argv[i],"--show-style"))show_style=true;
        else if(!strcmp(argv[i],"--show-about"))show_about=true;
        else if(!strcmp(argv[i],"--show-radio"))show_radio=true;
        else if(!strcmp(argv[i],"--resume-player"))resume_player=true;
        else if(!strcmp(argv[i],"--show-results"))show_results=true;
        else if(!strcmp(argv[i],"--assets")&&i+1<argc)requested_assets=argv[++i];
        else if(!strcmp(argv[i],"--screenshot")&&i+1<argc)screenfile=argv[++i];
        else if(!strcmp(argv[i],"--theme")&&i+1<argc){const char*t=argv[++i];for(int n=0;n<TCOUNT;n++)if(!strcmp(t,theme_files[n]))force_theme=n;}
        else if(!strcmp(argv[i],"--help")){
            puts("Usage: mustardtube [--version|--self-test|--assets DIR|--show-radio|--show-results|--show-about|--resume-player|--theme midnight|forest|paper|--screenshot FILE.bmp]");return 0;
        }
    }
    App*a=(App*)calloc(1,sizeof(App));if(!a){fputs("Out of memory\n",stderr);return 1;}
    a->running=true;a->dirty=true;a->page=show_about?ABOUT:show_radio?RADIO:show_style?STYLE:resume_player?PLAYER:show_results?RESULTS:HOME;
    a->from_page=resume_player?RESULTS:HOME;
    int rc=1;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)!=0){fprintf(stderr,"SDL init: %s\n",SDL_GetError());goto early;}
    if(TTF_Init()!=0){fprintf(stderr,"TTF init: %s\n",TTF_GetError());goto done;}
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG); /* Enable small JPEG video cards and PNG radio artwork; both optional. */
    if(!discover_assets(a,requested_assets)){
        fputs("Missing assets/themes/midnight.theme. Run from project root or use --assets PATH\n",stderr);goto done;}
    load_settings(a);init_data_dir(a);a->quality=360;
    char quality_path[PATH_MAX];
    if(snprintf(quality_path,sizeof(quality_path),"%s/quality.txt",a->data_dir)<(int)sizeof(quality_path)){
        FILE*f=fopen(quality_path,"r");int q=0;
        if(f){if(fscanf(f,"%d",&q)==1 && (q==480 || q==720))a->quality=q;fclose(f);}
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
        a->win=SDL_CreateWindow("MustardTube v0.9.11 Beta 2.1",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
             screen_w,screen_h,SDL_WINDOW_SHOWN|SDL_WINDOW_FULLSCREEN_DESKTOP);
        if(!a->win){
            fprintf(stderr,"Native fullscreen unavailable (%dx%d): %s; falling back to 640x480\n",
                screen_w,screen_h,SDL_GetError());
        }
    }
    if(!a->win){
        a->win=SDL_CreateWindow("MustardTube v0.9.11 Beta 2.1",SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    }
    if(!a->win){fprintf(stderr,"Window: %s\n",SDL_GetError());goto done;}
    int actual_w=W,actual_h=H;
    SDL_GetWindowSize(a->win,&actual_w,&actual_h);
    MTViewport view=mt_viewport(actual_w,actual_h,W,H);
    fprintf(stderr,"MustardTube display: detected %dx%d; window %dx%d; logical %dx%d; viewport %d,%d %dx%d\n",
        screen_w,screen_h,actual_w,actual_h,W,H,view.x,view.y,view.w,view.h);
    a->r=SDL_CreateRenderer(a->win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!a->r)a->r=SDL_CreateRenderer(a->win,-1,SDL_RENDERER_SOFTWARE);
    if(!a->r){fprintf(stderr,"Renderer: %s\n",SDL_GetError());goto done;}
    SDL_SetRenderDrawBlendMode(a->r,SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(a->r,W,H);
    a->pad=find_pad();SDL_StartTextInput();
    if(a->page==HOME)start_home_refresh(a,false);
    if(a->page==RESULTS)start_result_thumb_worker(a,true);
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
        int received=(a->awaiting_stream || preview || a->page==HOME ||
            (a->page==RESULTS && a->result_thumb_pid>0) ||
            (a->page==RADIO && (a->radio_loading || a->radio_logo_pid>0)) ||
            ((a->page==RADIO || a->page==ABOUT) && a->nav_repeat_dir))?SDL_WaitEventTimeout(&e,60):SDL_WaitEvent(&e);
        if(received){handle_event(a,&e);while(SDL_PollEvent(&e))handle_event(a,&e);}
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
    handoff_kind=a->radio_handoff?RADIO_PLAY_EXIT:a->current_is_youtube?YT_PLAY_EXIT:LOCAL_TEST_EXIT;
    if(a->request_youtube)handoff_kind=YT_SEARCH_EXIT;
    if(a->request_more)handoff_kind=YT_MORE_EXIT;
    if(a->request_clear)handoff_kind=CLEAR_CACHE_EXIT;
    if(a->request_clear_history)handoff_kind=CLEAR_HISTORY_EXIT;
    SDL_StopTextInput();
    shutdown_app(a); /* Always release video/input before the supervisor opens Wasabi. */
early:
    free(a);
    /* Special exit code requests stock Wasabi from our shell supervisor.
       The supervisor waits and starts MustardTube again after Wasabi quits. */
    return (handoff_kind==YT_SEARCH_EXIT || handoff_kind==YT_MORE_EXIT || handoff_kind==CLEAR_CACHE_EXIT || handoff_kind==CLEAR_HISTORY_EXIT) ? handoff_kind : handoff ? handoff_kind : rc;
}
