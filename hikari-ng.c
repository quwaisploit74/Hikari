/* HIKARI-NG 7.0.0 © Muhammad Quwais Saputra
 * Feature: Video Automation & Eporner Link Scanner
 * Fix: Filter Complex, Space Handling, and Real-time Percent Log
 * compile: gcc -o hikari-ng hikari-ng.c -lcurl -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_FOLDERS 12
#define FILES_PER_FOLDER 5

// --- STRUCTURES ---

typedef struct {
    int total_processed;
    int current_folder;
    int current_file_idx;
    int is_running;
    int current_percent;
} EditorStatus;

typedef struct {
    char username[100];
    int target_link;
    int *total_muncul;
    int *is_running;
} ScannerData;

struct MemoryStruct {
    char *memory;
    size_t size;
};

typedef struct {
    char *info;
    char *ask;
    char *error;
} HackProgram;

// --- HELPERS ---

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

const char* get_signal_display(int kb) {
    if (kb == 0) return "\033[35m";
    if (kb < 50) return "\033[90m";
    if (kb < 150) return "\033[31m";
    if (kb < 500) return "\033[33m";
    return "\033[32m";
}

// --- VIDEO EDITOR SUBSYSTEM ---

void* editor_animator(void* arg) {
    EditorStatus *status = (EditorStatus*)arg;
    const char *BANNER = "Hikari video editor with ffmpeg";
    int len = strlen(BANNER);
    int idx = 0;
    while (status->is_running) {
        printf("\r\033[K[%d/60 | %d%% | F%02d-V%d] \033[36m",
               status->total_processed, status->current_percent,
               status->current_folder, status->current_file_idx);
        for (int i = 0; i < len; i++) {
            if (i == idx) putchar(toupper(BANNER[i]));
            else putchar(tolower(BANNER[i]));
        }
        printf("\033[0m");
        fflush(stdout);
        idx = (idx + 1) % len;
        usleep(200000);
    }
    return NULL;
}

void process_video(const char* input, const char* output, const char* gif, const char* png, int duration, EditorStatus *status) {
    char cmd[4096];
    // Filter Complex: [0:v] Main, [1:v] Top Gif, [2:v] Bottom Gif, [3:v] Logo Png
    sprintf(cmd, "ffmpeg -y -i \"%s\" -t %d -ignore_loop 0 -i \"%s\" -ignore_loop 0 -i \"%s\" -i \"%s\" "
                 "-filter_complex \"[0:v]scale=720:480:force_original_aspect_ratio=decrease,pad=720:480:(720-iw)/2:(480-ih)/2,setsar=1[main];"
                 "[1:v]scale=720:40[top];[2:v]scale=720:40[bottom];[3:v]scale=100:-1,format=rgba,colorchannelmixer=aa=0.8[logo];"
                 "[main][top]overlay=0:0:shortest=1[v1];[v1][bottom]overlay=0:H-h:shortest=1[v2];[v2][logo]overlay=20:50\" "
                 "-c:v libx264 -preset veryfast -crf 28 -pix_fmt yuv420p -c:a aac -b:a 64k -shortest \"%s\" 2>&1",
                 input, duration, gif, gif, png, output);

    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return;

    char log_buffer[1024];
    while (fgets(log_buffer, sizeof(log_buffer), fp) != NULL) {
        log_buffer[strcspn(log_buffer, "\n")] = 0;

        // Parsing waktu untuk persentase
        char *time_ptr = strstr(log_buffer, "time=");
        if (time_ptr) {
            int hh, mm, ss;
            if (sscanf(time_ptr + 5, "%d:%d:%d", &hh, &mm, &ss) == 3) {
                int current_secs = (hh * 3600) + (mm * 60) + ss;
                status->current_percent = (current_secs * 100) / duration;
                if (status->current_percent > 100) status->current_percent = 100;
            }
        }

        printf("\r\033[K\033[90m[-][%d%%] %s\033[0m\n", status->current_percent, log_buffer);
    }
    pclose(fp);
    status->current_percent = 0;
}

void video_editor(const char *gif_path, const char *png_path, const char *target_dir, int duration) {
    char abs_gif[PATH_MAX], abs_png[PATH_MAX];
    if (!realpath(gif_path, abs_gif) || !realpath(png_path, abs_png)) {
        printf("\n\033[31m[!] Resource (GIF/PNG) tidak ditemukan!\033[0m\n");
        return;
    }

    if (chdir(target_dir) != 0) {
        printf("\n\033[31m[!] Directory video tidak ditemukan!\033[0m\n");
        return;
    }

    EditorStatus status = {0, 1, 0, 1, 0};
    pthread_t tid;
    pthread_create(&tid, NULL, editor_animator, &status);

    DIR *d = opendir(".");
    struct dirent *dir;
    if (d) {
        while ((dir = readdir(d)) != NULL && status.total_processed < 60) {
            if (strstr(dir->d_name, ".mp4") && !strstr(dir->d_name, "edit_")) {
                char folder_name[64];
                sprintf(folder_name, "edit_%d", status.current_folder);
                mkdir(folder_name, 0777);

                status.current_file_idx++;
                if (status.current_file_idx > FILES_PER_FOLDER) {
                    status.current_file_idx = 1;
                    status.current_folder++;
                }

                char output_path[512];
                sprintf(output_path, "%s/edit_%d.mp4", folder_name, status.current_file_idx);

                process_video(dir->d_name, output_path, abs_gif, abs_png, duration, &status);
                status.total_processed++;
            }
        }
        closedir(d);
    }
    status.is_running = 0;
    pthread_join(tid, NULL);
    printf("\n\033[32m[+] Batch Rendering Selesai.\033[0m\n");
}

// --- LINK SCANNER SUBSYSTEM ---

void* scanner_animator(void* arg) {
    ScannerData *data = (ScannerData*)arg;
    const char *BANNER = "Hikari-ng Eporner scanner";
    int len = strlen(BANNER);
    int idx = 0;
    while (*(data->is_running)) {
        int current_kb = rand() % 1201;
        const char* color = get_signal_display(current_kb);
        printf("\r\033[K[%d/%d | %s | %s%4d KBps\033[0m] \033[36m",
               *(data->total_muncul), data->target_link, data->username, color, current_kb);
        for (int i = 0; i < len; i++) {
            if (i == idx) putchar(toupper(BANNER[i]));
            else putchar(tolower(BANNER[i]));
        }
        printf("\033[0m");
        fflush(stdout);
        idx = (idx + 1) % len;
        usleep(200000);
    }
    return NULL;
}

int is_blacklisted(const char *url, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        strtok(line, "\n");
        if (strcmp(line, url) == 0) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

void linkscanner(char *username, int target_link, char *blacklist_path) {
    int total_muncul = 0;
    int is_running = 1;
    pthread_t tid;
    ScannerData s_data;
    strncpy(s_data.username, username, 99);
    s_data.target_link = target_link;
    s_data.total_muncul = &total_muncul;
    s_data.is_running = &is_running;

    pthread_create(&tid, NULL, scanner_animator, &s_data);

    char target_url[256];
    sprintf(target_url, "https://www.eporner.com/profile/%s/", username);
    curl_global_init(CURL_GLOBAL_ALL);

    while (is_running && total_muncul < target_link) {
        struct MemoryStruct chunk = {malloc(1), 0};
        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, target_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hikari-Scanner/7.0");

        if (curl_easy_perform(curl) == CURLE_OK) {
            char *p = chunk.memory;
            while ((p = strstr(p, "href=\"/video-"))) {
                p += 6;
                char path[256] = {0};
                sscanf(p, "%[^\"]", path);
                char full_url[512];
                sprintf(full_url, "https://www.eporner.com%s", path);

                if (!is_blacklisted(full_url, blacklist_path)) {
                    printf("\r\033[K\033[32m%s\033[0m\n", full_url);
                    FILE *fb = fopen(blacklist_path, "a");
                    if (fb) { fprintf(fb, "%s\n", full_url); fclose(fb); }
                    FILE *fl = fopen("link.txt", "a");
                    if (fl) { fprintf(fl, "%s\n", full_url); fclose(fl); }
                    total_muncul++;
                    if (total_muncul >= target_link) break;
                }
            }
        }
        curl_easy_cleanup(curl);
        free(chunk.memory);
        sleep(2);
    }
    is_running = 0;
    pthread_join(tid, NULL);
    curl_global_cleanup();
}

// --- MAIN INTERFACE ---
                                                                      int main() {
    srand(time(NULL));
    HackProgram Hack = {"\033[37m[\033[31m+\033[37m]\033[33m", "\033[33m[?]\033[37m", "\033[31m[!] \033[41m\033[30mERROR\033[40m\033[37m"};

    printf("%s Starting Hikari-ng 7.0.0 (Quwais)\n", Hack.info);

    int choice;
    while(1) {
        printf("\033[37m\n1. Edit Video (FFmpeg)\n2. Link Scanner (Eporner)\n3. Exit\n");
        printf("%s Pilihan: ", Hack.ask);
        if (scanf("%d", &choice) != 1) { while(getchar() != '\n'); continue; }
        getchar();

        if (choice == 1) {
            char g[256], i[256], d[256]; int dur;
            printf("%s Path GIF: ", Hack.ask); scanf("%255[^\n]", g); getchar();
            printf("%s Path PNG: ", Hack.ask); scanf("%255[^\n]", i); getchar();
            printf("%s Path Video Folder: ", Hack.ask); scanf("%255[^\n]", d); getchar();
            printf("%s Durasi (detik): ", Hack.ask); scanf("%d", &dur);
            video_editor(g, i, d, dur);
        } else if (choice == 2) {
            char u[100], b[256]; int t;
            printf("%s Username: ", Hack.ask); scanf("%s", u);
            printf("%s Target: ", Hack.ask); scanf("%d", &t);
            printf("%s Blacklist: ", Hack.ask); scanf("%s", b);
            linkscanner(u, t, b);
        } else if (choice == 3) return 0;
    }
    return 0;
}
