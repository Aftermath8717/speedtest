#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>   // Interneto uzklausos
#include <cjson/cJSON.h> // JSON skaitymas

// Cia saugomas tekstas, kuris ateina is interneto
// tekstas - pats gautas tekstas
// dydis - kiek baitu jau turima
typedef struct {
    char *tekstas;
    size_t dydis;
} Atsakymas;

// Cia saugomas vienas speedtest serveris
// Reikalingas tik host, nes serveris parenkamas pagal sali
typedef struct {
    char host[256];
} Serveris;

// Saugus teksto kopijavimas i 256 simboliu masyva
// Jei tekstas per ilgas, jis nukerpamas
// Gale visada idedama teksto pabaiga
void kopijuoti(char kur[256], const char *tekstas) {
    // Jei teksto nera, naudojamas tuscias tekstas
    if (tekstas == NULL) {
        tekstas = "";
    }

    // Kopijuojamas tekstas, bet ne daugiau kaip 255 simboliai
    strncpy(kur, tekstas, 255);

    // Rankiniu budu idedama teksto pabaiga
    kur[255] = '\0';
}

// Funkcija gauna atsakyma is interneto
// Ja libcurl kviecia automatiskai
// Vienu kartu gali ateiti tik dalis teksto
// Todel kiekviena karta tekstas pridedamas prie seno
size_t gauti_teksta(void *duomenys, size_t dydis, size_t kiekis, void *vieta) {
    // Cia saugoma atsakymo struktura
    Atsakymas *atsakymas = vieta;

    // Skaiciuojama kiek baitu gauta
    size_t gauta = dydis * kiekis;

    // Padidinama atmintis, kad tilptu senas ir naujas tekstas
    char *tekstas = realloc(atsakymas->tekstas, atsakymas->dydis + gauta + 1);

    // Jei atminties nepavyksta gauti, stabdomas darbas
    if (tekstas == NULL) {
        return 0;
    }

    // Issaugoma nauja atminties vieta
    atsakymas->tekstas = tekstas;

    // Pridedami nauji duomenys prie seno teksto galo
    memcpy(atsakymas->tekstas + atsakymas->dydis, duomenys, gauta);

    // Padidinamas bendro teksto dydis
    atsakymas->dydis = atsakymas->dydis + gauta;

    // Uzdedama teksto pabaiga
    atsakymas->tekstas[atsakymas->dydis] = '\0';

    // Grazinama, kiek baitu priimta
    return gauta;
}

// Funkcija gauna duomenis, bet ju nesaugo
// Reikia tik greicio testui
// Libcurl reikalauja tokios funkcijos formos
size_t ismesti_duomenis(void *duomenys, size_t dydis, size_t kiekis, void *vieta) {
    // Sitie parametrai nenaudojami
    // Paliekami, nes ju reikia libcurl
    (void)duomenys;
    (void)vieta;

    // Grazinama kiek baitu gauta
    // Taip libcurl zino, kad viskas gerai
    return dydis * kiekis;
}

// Paprastas GET uzklausos siuntimas
// Naudojama vietoves API atsakymui gauti
// Funkcija grazina teksta is interneto
char *gauti_url_teksta(const char *url) {
    // Curl objektas interneto uzklausai
    CURL *curl;

    // Cia kaupiamas gautas tekstas
    Atsakymas atsakymas;

    // Pradzioje teksto nera
    atsakymas.tekstas = NULL;

    // Pradzioje teksto dydis yra 0
    atsakymas.dydis = 0;

    // Sukuriamas curl objektas
    curl = curl_easy_init();

    // Jei curl nesusikuria, grazinama NULL
    if (curl == NULL) {
        return NULL;
    }

    // Pasakoma, is kur siusti duomenis
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Pasakoma, kokia funkcija priims teksta
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gauti_teksta);

    // Pasakoma, kur deti gauta teksta
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &atsakymas);

    // Laukiama ne ilgiau kaip 15 sekundziu
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    // Paprastas programos pavadinimas serveriui
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "c-speedtest");

    // Vykdoma uzklausa
    curl_easy_perform(curl);

    // Atlaisvinamas curl objektas
    curl_easy_cleanup(curl);

    // Grazinamas gautas tekstas
    // Ji veliau reikes atlaisvinti su free()
    return atsakymas.tekstas;
}

// Nuskaitomas visas failas i viena teksta
// Taip skaitomas speedtest_server_list.json
// cJSON reikia viso JSON teksto
char *skaityti_faila(const char *vardas) {
    // Failo kintamasis
    FILE *failas;

    // Failo dydis baitais
    long dydis;

    // Cia bus visas failo tekstas
    char *tekstas;

    // Atidaromas failas skaitymui
    failas = fopen(vardas, "rb");

    // Jei failas neatsidaro, niekas negrazinama
    if (failas == NULL) {
        return NULL;
    }

    // Nueinama i failo gala
    fseek(failas, 0, SEEK_END);

    // Suzinoma kiek baitu turi failas
    dydis = ftell(failas);

    // Griztama i failo pradzia
    rewind(failas);

    // Paimama vietos visam tekstui
    // +1 reikia teksto pabaigai
    tekstas = malloc(dydis + 1);

    // Jei atminties nera, uzdaromas failas
    if (tekstas == NULL) {
        fclose(failas);
        return NULL;
    }

    // Nuskaitomas visas failas
    fread(tekstas, 1, dydis, failas);

    // Uzdedama teksto pabaiga
    tekstas[dydis] = '\0';

    // Uzdaromas failas
    fclose(failas);

    // Grazinamas failo tekstas
    return tekstas;
}

// Nustatoma vartotojo salis
// Naudojama paprasta API be rakto
// API grazina JSON teksta
// Is JSON paimamas country laukas
int nustatyti_vietove(char salis[256]) {
    // Cia bus API atsakymo tekstas
    char *tekstas;

    // Cia bus visas JSON objektas
    cJSON *json;

    // Cia bus country laukas is JSON
    cJSON *salis_json;

    // Pradzioje salis tuscia
    salis[0] = '\0';

    // Parsisiunciamas API atsakymas
    tekstas = gauti_url_teksta("http://ip-api.com/json/?fields=country");

    // Jei atsakymo nera, grazinama klaida
    if (tekstas == NULL) {
        return 0;
    }

    // Is teksto padaromas JSON
    json = cJSON_Parse(tekstas);

    // Teksto jau nebereikia
    free(tekstas);

    // Jei JSON blogas, grazinama klaida
    if (json == NULL) {
        return 0;
    }

    // Paimamas country laukas
    salis_json = cJSON_GetObjectItem(json, "country");

    // Jei country yra tekstas, jis issaugomas
    if (cJSON_IsString(salis_json)) {
        kopijuoti(salis, salis_json->valuestring);
    }

    // JSON nebereikia
    cJSON_Delete(json);

    // Jei salis liko tuscia, reiskia nepavyko
    if (salis[0] == '\0') {
        return 0;
    }

    // Viskas gerai
    return 1;
}

// Randamas serveris pagal sali
// Serveriai yra speedtest_server_list.json faile
// Failas skaitomas kaip JSON masyvas
// Tikrinami tik country ir host laukai
// Imamas pirmas tinkamas serveris
int rasti_serveri(const char *salis, Serveris *serveris) {
    // Cia bus visas failo tekstas
    char *tekstas;

    // Cia bus visas JSON masyvas
    cJSON *json;

    // Cia bus vienas serverio irasas is masyvo
    cJSON *irasas;

    // Cia bus JSON laukai
    cJSON *salis_json;
    cJSON *host_json;

    // Pradzioje serverio host tuscias
    kopijuoti(serveris->host, "");

    // Nuskaitomas serveriu saraso failas
    tekstas = skaityti_faila("speedtest_server_list.json");

    // Jei failo nepavyksta nuskaityti, serverio nera
    if (tekstas == NULL) {
        return 0;
    }

    // Is failo teksto padaromas JSON
    json = cJSON_Parse(tekstas);

    // Failo teksto jau nebereikia
    free(tekstas);

    // Jei JSON blogas, serverio nera
    if (json == NULL) {
        return 0;
    }

    // Einama per visus serverius JSON masyve
    cJSON_ArrayForEach(irasas, json) {
        // Paimamas salies laukas
        salis_json = cJSON_GetObjectItem(irasas, "country");

        // Paimamas host laukas
        host_json = cJSON_GetObjectItem(irasas, "host");

        // Tikrinama ar salis ir host yra tekstai
        if (cJSON_IsString(salis_json) && cJSON_IsString(host_json)) {
            // Lyginama salis
            // strcasecmp nekreipia demesio i didziasias ir mazasias raides
            if (strcasecmp(salis_json->valuestring, salis) == 0) {
                // Issaugomas host
                kopijuoti(serveris->host, host_json->valuestring);

                // JSON nebereikia
                cJSON_Delete(json);

                // Serveris rastas
                return 1;
            }
        }
    }

    // JSON nebereikia
    cJSON_Delete(json);

    // Serverio pagal tokia sali nera
    return 0;
}

// Sudaromas pilnas URL adresas
// host yra serverio adresas
// kelias yra failo arba upload kelias serveryje
// Pvz: host + kelias = http://serveris.lt:8080/speedtest/random4000x4000.jpg
void sudaryti_url(char url[512], const char *host, const char *kelias) {
    // Tikrinama ar host jau prasideda su http://
    // 7 yra "http://" ilgis
    if (strncasecmp(host, "http://", 7) == 0) {
        // Jei http jau yra, jo antra karta nepridedama
        snprintf(url, 512, "%s%s", host, kelias);
    }

    // Tikrinama ar host jau prasideda su https://
    // 8 yra "https://" ilgis
    else if (strncasecmp(host, "https://", 8) == 0) {
        // Jei https jau yra, jo antra karta nepridedama
        snprintf(url, 512, "%s%s", host, kelias);
    }

    // Jei host neturi http arba https
    else {
        // Pridedama http:// priekyje
        snprintf(url, 512, "http://%s%s", host, kelias);
    }
}

// Download testas
// Parsisiunciamas testinis failas is serverio
// Failas nesaugomas
// Skaiciuojama tik kiek baitu atejo
// Greitis grazinamas Mbps
float download_testas(const char *host) {
    // Curl objektas interneto uzklausai
    CURL *curl;

    // Cia bus pilnas download URL
    char url[512];

    // Cia libcurl irasys kiek baitu parsiunte
    curl_off_t gauti_baitai = 0;

    // Cia libcurl irasys kiek sekundziu truko testas
    double sekundes = 0;

    // Sudaromas testinio failo adresas
    // Sis kelias daznai naudojamas speedtest serveriuose
    sudaryti_url(url, host, "/speedtest/random4000x4000.jpg");

    // Sukuriamas curl objektas
    curl = curl_easy_init();

    // Jei curl nesusikuria, greitis 0
    if (curl == NULL) {
        return 0;
    }

    // Pasakoma is kur siusti faila
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Pasakoma, kad gauti duomenys butu ismetami
    // Reikia tik greicio, ne pacio failo
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ismesti_duomenis);

    // Testas negali trukti ilgiau nei 15 sekundziu
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    // Paprastas programos pavadinimas serveriui
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "c-speedtest");

    // Vykdomas download testas
    curl_easy_perform(curl);

    // Paimama kiek baitu buvo parsisiusta
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &gauti_baitai);

    // Paimama kiek laiko uztruko siuntimas
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &sekundes);

    // Sutvarkomas curl objektas
    curl_easy_cleanup(curl);

    // Jei nieko negauta arba laikas blogas, grazinama 0
    if (gauti_baitai <= 0 || sekundes <= 0) {
        return 0;
    }

    // Skaiciuojamas Mbps
    // baitai * 8 = bitai
    // / sekundes = bitai per sekunde
    // / 1000000 = megabitai per sekunde
    return (float)((gauti_baitai * 8.0) / sekundes / 1000000.0);
}

// Upload testas
// Siunciami paprasti duomenys i serveri
// Duomenys yra tik A raides
// Atsakymas nesaugomas
// Skaiciuojama kiek baitu buvo issiusta
// Greitis grazinamas Mbps
float upload_testas(const char *host) {
    // Curl objektas interneto uzklausai
    CURL *curl;

    // Cia bus pilnas upload URL
    char url[512];

    // Cia libcurl irasys kiek baitu issiunte
    curl_off_t issiusti_baitai = 0;

    // Cia libcurl irasys kiek sekundziu truko testas
    double sekundes = 0;

    // Statinis masyvas upload testui
    // Static reiskia, kad masyvas nebus dedamas i stack
    // 10000000 baitu yra apie 10 MB
    // Paprastam testui uztenka
    static char siuntimo_duomenys[10000000];

    // Uzpildomas visas masyvas A raidemis
    // Nesvarbu ka siusti, svarbu kiek baitu siusti
    memset(siuntimo_duomenys, 'A', 10000000);

    // Sudaromas upload adresas
    // Sis kelias daznai naudojamas speedtest serveriuose
    sudaryti_url(url, host, "/speedtest/upload.php");

    // Sukuriamas curl objektas
    curl = curl_easy_init();

    // Jei curl nesusikuria, greitis 0
    if (curl == NULL) {
        return 0;
    }

    // Pasakoma i koki URL siusti duomenis
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Pasakoma, kad bus POST uzklausa
    // POST naudojamas duomenu issiuntimui
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // Pasakoma kokius duomenis siusti
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, siuntimo_duomenys);

    // Pasakoma kiek baitu siusti
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 10000000L);

    // Serverio atsakymas ismetamas
    // Reikia tik upload greicio
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ismesti_duomenis);

    // Testas negali trukti ilgiau nei 15 sekundziu
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    // Paprastas programos pavadinimas serveriui
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "c-speedtest");

    // Vykdomas upload testas
    curl_easy_perform(curl);

    // Paimama kiek baitu buvo issiusta
    curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &issiusti_baitai);

    // Paimama kiek laiko uztruko siuntimas
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &sekundes);

    // Sutvarkomas curl objektas
    curl_easy_cleanup(curl);

    // Jei nieko neissiusta arba laikas blogas, grazinama 0
    if (issiusti_baitai <= 0 || sekundes <= 0) {
        return 0;
    }

    // Skaiciuojamas Mbps
    // baitai * 8 = bitai
    // / sekundes = bitai per sekunde
    // / 1000000 = megabitai per sekunde
    return (float)((issiusti_baitai * 8.0) / sekundes / 1000000.0);
}

// Parodoma kaip naudoti programa
// Cia isvedami visi galimi pasirinkimai
void pagalba(void) {
    printf("Naudojimas:\n");
    printf("  ./speedtest -a\n");
    printf("  ./speedtest -l\n");
    printf("  ./speedtest -s <salis>\n");
    printf("  ./speedtest -d <host>\n");
    printf("  ./speedtest -u <host>\n");
    printf("  ./speedtest -h\n");
    printf("\n");
    printf("Reiksmes:\n");
    printf("  -a  visas automatinis testas\n");
    printf("  -l  tik vietoves nustatymas\n");
    printf("  -s  serverio paieska pagal sali\n");
    printf("  -d  download testas su nurodytu host\n");
    printf("  -u  upload testas su nurodytu host\n");
    printf("  -h  pagalba\n");
}

// Pilnas automatinis testas
// Viskas vyksta viena tvarka
// 1. Nustatoma salis
// 2. Randamas serveris
// 3. Atliekamas download testas
// 4. Atliekamas upload testas
// 5. Spausdinami rezultatai
void visas_testas(void) {
    // Cia bus vartotojo salis
    char salis[256] = "";

    // Cia bus rastas serveris
    Serveris serveris;

    // Cia bus download greitis Mbps
    float download;

    // Cia bus upload greitis Mbps
    float upload;

    // Pradzioje serverio host tuscias
    kopijuoti(serveris.host, "");

    // 1 zingsnis
    // Nustatoma vartotojo salis pagal IP
    printf("Nustatoma vietove...\n");

    // Jei vietoves nustatyti nepavyksta, baigiamas darbas
    if (!nustatyti_vietove(salis)) {
        printf("Klaida: nepavyko nustatyti vietoves\n");
        return;
    }

    // 2 zingsnis
    // Pagal sali ieskomas serveris JSON faile
    printf("Ieskomas serveris pagal sali...\n");

    // Jei serverio nera, baigiamas darbas
    if (!rasti_serveri(salis, &serveris)) {
        printf("Klaida: serveris nerastas\n");
        return;
    }

    // 3 zingsnis
    // Atliekamas download testas
    printf("Vykdomas download testas...\n");

    // Funkcija grazina Mbps
    download = download_testas(serveris.host);

    // 4 zingsnis
    // Atliekamas upload testas
    printf("Vykdomas upload testas...\n");

    // Funkcija grazina Mbps
    upload = upload_testas(serveris.host);

    // 5 zingsnis
    // Spausdinami viso testo rezultatai
    printf("\nRezultatai:\n");

    // Spausdinama vartotojo salis
    printf("Salis: %s\n", salis);

    // Spausdinamas serverio host
    printf("Serveris: %s\n", serveris.host);

    // Spausdinamas download greitis
    printf("Download: %.2f Mbps\n", download);

    // Spausdinamas upload greitis
    printf("Upload: %.2f Mbps\n", upload);
}

// Programos pradzia
// argc rodo kiek yra komandos daliu
// argv saugo visas komandos dalis
int main(int argc, char **argv) {
    // Cia bus getopt pasirinkta raide
    int pasirinkimas;

    // Cia bus vartotojo salis
    char salis[256] = "";

    // Cia bus rastas serveris
    Serveris serveris;

    // Cia bus download arba upload greitis
    float greitis;

    // Pradzioje serverio host tuscias
    kopijuoti(serveris.host, "");

    // Paruosiama libcurl biblioteka
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Jei vartotojas nieko neparaso
    if (argc == 1) {
        // Rodoma pagalba
        pagalba();

        // Sutvarkoma libcurl
        curl_global_cleanup();

        // Baigiama programa
        return 0;
    }

    // getopt skaito raides po bruksnelio
    // a, l, h neturi papildomo teksto
    // s, d, u turi papildoma teksta
    // todel po ju yra dvitaskis
    while ((pasirinkimas = getopt(argc, argv, "als:d:u:h")) != -1) {
        // -a reiskia visas automatinis testas
        if (pasirinkimas == 'a') {
            visas_testas();
        }

        // -l reiskia tik vietoves nustatymas
        else if (pasirinkimas == 'l') {
            // Bandoma nustatyti sali
            if (nustatyti_vietove(salis)) {
                // Jei pavyksta, spausdinama salis
                printf("Salis: %s\n", salis);
            } else {
                // Jei nepavyksta, spausdinama klaida
                printf("Klaida: vietove nenustatyta\n");
            }
        }

        // -s reiskia serverio paieska pagal sali
        else if (pasirinkimas == 's') {
            // optarg yra tekstas po -s
            // Pvz: ./speedtest -s Lithuania
            if (rasti_serveri(optarg, &serveris)) {
                // Jei serveris rastas, spausdinamas host
                printf("Serveris: %s\n", serveris.host);
            } else {
                // Jei serverio nerasta, spausdinama klaida
                printf("Klaida: serveris nerastas\n");
            }
        }

        // -d reiskia download testas
        else if (pasirinkimas == 'd') {
            // optarg yra serverio host
            // Pvz: ./speedtest -d serveris.lt:8080
            greitis = download_testas(optarg);

            // Spausdinamas download greitis
            printf("Download: %.2f Mbps\n", greitis);
        }

        // -u reiskia upload testas
        else if (pasirinkimas == 'u') {
            // optarg yra serverio host
            // Pvz: ./speedtest -u serveris.lt:8080
            greitis = upload_testas(optarg);

            // Spausdinamas upload greitis
            printf("Upload: %.2f Mbps\n", greitis);
        }

        // Jei raide neaiski arba -h
        else {
            // Rodoma pagalba
            pagalba();
        }
    }

    // Sutvarkoma libcurl biblioteka
    curl_global_cleanup();

    // Programa baigesi normaliai
    return 0;
}
