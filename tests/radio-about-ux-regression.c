#define main mustardtube_application_main
#include "../src/main.c"
#undef main
#include <assert.h>
int main(void){
    char tmp[]="/tmp/mt-radio-about-XXXXXX";
    assert(mkdtemp(tmp));
    App*a=calloc(1,sizeof(App));assert(a);
    snprintf(a->data_dir,sizeof(a->data_dir),"%s",tmp);
    char file[PATH_MAX];
    snprintf(file,sizeof(file),"%s/radio-countries.tsv",tmp);
    FILE*f=fopen(file,"w");assert(f);
    fputs("US|||United States|||500\nDE|||Germany|||300\nJP|||Japan|||200\nAU|||Australia|||100\nGB|||United Kingdom|||400\nMY|||Malaysia|||100\nFR|||France|||50\nNZ|||New Zealand|||60\nSE|||Sweden|||70\n",f);
    fclose(f);
    radio_load_countries(a);assert(a->country_count==9);
    assert(!strcmp(a->countries[0].name,"Australia"));
    assert(!strcmp(a->countries[1].name,"France"));
    assert(!strcmp(a->countries[8].name,"United States"));
    a->page=RADIO;
    navigate(a,1,0);assert(a->country_idx==7);
    navigate(a,1,0);assert(a->country_idx==8);
    navigate(a,-1,0);assert(a->country_idx==1);
    navigate(a,0,-1);assert(a->country_idx==0);
    navigate(a,0,-1);assert(a->country_idx==0);
    SDL_Event ev={0};ev.type=SDL_CONTROLLERBUTTONDOWN;ev.cbutton.button=SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    handle_event(a,&ev);
    assert(a->nav_repeat_dir==1&&a->country_idx==1);
    a->nav_repeat_at=0;
    if((int32_t)(SDL_GetTicks()-a->nav_repeat_at)>=0){navigate(a,0,a->nav_repeat_dir);a->nav_repeat_at=SDL_GetTicks()+85;}
    assert(a->country_idx==2);
    ev.type=SDL_CONTROLLERBUTTONUP;handle_event(a,&ev);assert(a->nav_repeat_dir==0);
    snprintf(a->radio_selected_country,sizeof(a->radio_selected_country),"US");
    snprintf(file,sizeof(file),"%s/radio-stations.tsv",tmp);
    f=fopen(file,"w");assert(f);
    fputs("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa|||Zebra FM|||MP3|||https://radio.example.org/z|||US\n",f);
    fputs("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb|||Alpha Radio|||AAC|||https://radio.example.org/a|||US\n",f);
    fputs("cccccccc-cccc-cccc-cccc-cccccccccccc|||middle FM|||MP3|||https://radio.example.org/m|||US\n",f);
    fclose(f);a->station_count=0;radio_load_stations(a);
    assert(a->station_count==3);
    assert(!strcmp(a->stations[0].name,"Alpha Radio"));
    assert(!strcmp(a->stations[1].name,"middle FM"));
    assert(!strcmp(a->stations[2].name,"Zebra FM"));
    a->page=ABOUT;
    assert(ABOUT_COUNT>90);
    navigate(a,0,1);assert(a->about_scroll==1);
    navigate(a,1,0);assert(a->about_scroll==1+ABOUT_VISIBLE);
    about_jump_section(a,1);assert(ABOUT_LINES[a->about_scroll].heading);
    about_jump_section(a,-1);assert(a->about_scroll<ABOUT_COUNT);
    for(int i=0;i<300;i++)navigate(a,0,1);
    assert(a->about_scroll==ABOUT_COUNT-ABOUT_VISIBLE);
    for(int i=0;i<300;i++)navigate(a,0,-1);
    assert(a->about_scroll==0);
    a->nav_repeat_dir=1;set_page(a,HOME);assert(a->nav_repeat_dir==0);
    snprintf(file,sizeof(file),"%s/radio-countries.tsv",tmp);unlink(file);
    snprintf(file,sizeof(file),"%s/radio-stations.tsv",tmp);unlink(file);
    rmdir(tmp);free(a);
    puts("PASS alphabetical Radio lists, bounded page navigation, held-D-pad input, About scrolling and section jumps");
    return 0;
}
