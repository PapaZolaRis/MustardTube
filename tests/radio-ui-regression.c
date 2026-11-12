#define main mustardtube_application_main
#include "../src/main.c"
#undef main
#include <assert.h>
int main(void){
    char tmp[]="/tmp/mt-radio-ui-XXXXXX";assert(mkdtemp(tmp));
    App*a=calloc(1,sizeof(App));assert(a);
    snprintf(a->data_dir,sizeof(a->data_dir),"%s",tmp);
    assert(radio_url_ok("https://stream.example.org:8000/stream.mp3"));
    assert(!radio_url_ok("file:///tmp/evil"));
    assert(!radio_url_ok("http://192.168.0.1/private"));
    assert(!radio_url_ok("http://127.0.0.1/live"));
    assert(!radio_url_ok("https://bob:secret@stream.example.org/test"));
    assert(!radio_url_ok("https://stream.example.org/test\nother"));
    char row[]="aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa|||Example Station|||MP3|||https://radio.example.org/live.mp3|||US\n";
    RadioStation st;assert(radio_parse_station(row,&st));
    assert(!strcmp(st.name,"Example Station"));
    char six[]="cccccccc-cccc-cccc-cccc-cccccccccccc|||Station Logo|||AAC|||https://radio.example.org/station|||MY|||https://logo.example.org/logo.png\n";
    assert(radio_parse_station(six,&st));assert(!strcmp(st.country,"MY"));assert(!strcmp(st.logo,"https://logo.example.org/logo.png"));
    char invalid[]="bad|||Example|||MP3|||file:///tmp/evil|||US\n";
    assert(!radio_parse_station(invalid,&st));
    char file[PATH_MAX];snprintf(file,sizeof(file),"%s/radio-countries.tsv",tmp);
    FILE*f=fopen(file,"w");assert(f);fputs("GB|||United Kingdom|||300\nUS|||United States|||200\n",f);fclose(f);
    radio_load_countries(a);assert(a->country_count==2);
    assert(!strcmp(a->countries[1].name,"United States"));
    snprintf(a->radio_selected_country,sizeof(a->radio_selected_country),"US");
    snprintf(file,sizeof(file),"%s/radio-stations.tsv",tmp);
    f=fopen(file,"w");assert(f);
    fputs("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa|||Example Station|||MP3|||https://radio.example.org/live.mp3|||US\n",f);
    fputs("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb|||Other Station|||AAC|||https://radio.example.org/other|||GB\n",f);
    fclose(f);radio_load_stations(a);assert(a->station_count==1);
    assert(!strcmp(a->stations[0].name,"Example Station"));
    radio_toggle_favourite(a,&a->stations[0]);assert(a->favourite_count==1);
    radio_load_favourites(a);assert(radio_is_favourite(a,&a->stations[0]));
    a->running=true;assert(radio_pick_station(a,&a->stations[0]));assert(a->radio_handoff && !a->running);
    snprintf(file,sizeof(file),"%s/radio-selected.txt",tmp);
    f=fopen(file,"r");assert(f);char name[140],url[800];assert(fgets(name,sizeof(name),f));assert(fgets(url,sizeof(url),f));fclose(f);
    assert(!strncmp(url,"https://radio.example.org",25));
    radio_toggle_favourite(a,&a->stations[0]);assert(a->favourite_count==0);
    /* More than 80 stations must load safely, retaining alphabetical navigation. */
    snprintf(file,sizeof(file),"%s/radio-stations.tsv",tmp);
    f=fopen(file,"w");assert(f);
    for(int i=0;i<155;i++)
        assert(fprintf(f,"%032x|||Station %03d|||MP3|||https://radio.example.org/%d|||US|||-\n",i+1,i,i)>0);
    assert(fclose(f)==0);
    snprintf(file,sizeof(file),"%s/radio-pagination.tsv",tmp);
    f=fopen(file,"w");assert(f);fputs("US|||160|||0\n",f);assert(fclose(f)==0);
    radio_load_stations(a);assert(a->station_count==155);
    assert(a->radio_next_offset==160 && !a->radio_has_more);
    assert(!strcmp(a->stations[154].name,"Station 154"));
    snprintf(file,sizeof(file),"%s/radio-pagination.tsv",tmp);unlink(file);
    snprintf(file,sizeof(file),"%s/radio-stations.tsv",tmp);unlink(file);
    snprintf(file,sizeof(file),"%s/radio-countries.tsv",tmp);unlink(file);
    snprintf(file,sizeof(file),"%s/radio-favourites.tsv",tmp);unlink(file);
    snprintf(file,sizeof(file),"%s/radio-selected.txt",tmp);unlink(file);
    snprintf(file,sizeof(file),"%s/radio-last-country.txt",tmp);unlink(file);
    rmdir(tmp);free(a->stations);free(a);
    puts("PASS: 155-station dynamic UI, safe paging, favourites, validated radio selection and handoff");return 0;
}
