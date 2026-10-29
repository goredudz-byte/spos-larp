#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <time.h>

typedef struct {
    double x, y, z;
    int r, g, b;
    char ch;
} Point;

void get_terminal_size(int *width, int *height) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 && w.ws_row > 0) {
        *width = w.ws_col;
        *height = w.ws_row;
    } else {
        *width = 80;
        *height = 24;
    }
}

void get_real_os_distro(char *dest, size_t size) {
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "ID=", 3) == 0) {
                char *start = line + 3;
                if (*start == '"') start++;
                size_t len = strlen(start);
                if (len > 0 && start[len-1] == '\n') start[--len] = '\0';
                if (len > 0 && start[len-1] == '"') start[--len] = '\0';
                snprintf(dest, size, "%s", start);
                fclose(f);
                return;
            }
        }
        fclose(f);
    }
    snprintf(dest, size, "android");
}

int is_android_env() {
    return (access("/system/bin/app_process", F_OK) == 0 || getenv("ANDROID_ROOT") != NULL);
}

void show_help() {
    printf("===================================================\n");
    printf("                 SPOS FULL USAGE GUIDE             \n");
    printf("===================================================\n");
    printf("  ./spos                          - Spin your real OS distro in 3D\n");
    printf("  ./spos 2                        - Spin your real OS distro in flat 2D\n");
    printf("  ./spos <distro>                 - Spin a specific distro image in 3D (e.g., ./spos debian)\n");
    printf("  ./spos <distro> 2               - Spin a specific distro image in flat 2D\n");
    printf("  ./spos <distro> 2 <speed>%%      - Spin with custom speed percentage (e.g., ./spos debian 2 50%%)\n");
    printf("  ./spos larp                     - Spawn a 'larp window' instance (PC / Samsung DeX only)\n");
    printf("  ./spos crash                    - Cycle through all images in 'distros/' (1 sec each)\n");
    printf("  ./spos help                     - Show this comprehensive master help manual\n");
    printf("  ./spos all                      - Alias for help manual display\n");
    printf("===================================================\n");
    exit(0);
}

int load_image_points(const char *filepath, Point **out_points, int make_thick) {
    int img_w, img_h, channels;
    unsigned char *img_data = stbi_load(filepath, &img_w, &img_h, &channels, 4);
    if (!img_data) return 0;

    int target_grid = 50;
    int max_points = target_grid * target_grid * (make_thick ? 5 : 1);
    Point *points = malloc(max_points * sizeof(Point));
    int point_count = 0;
    double cx = target_grid / 2.0;
    double cy = target_grid / 2.0;

    for (int y = 0; y < target_grid; y++) {
        for (int x = 0; x < target_grid; x++) {
            int px = (int)((double)x / target_grid * img_w);
            int py = (int)((double)y / target_grid * img_h);
            int idx = (py * img_w + px) * 4;

            unsigned char r = img_data[idx];
            unsigned char g = img_data[idx + 1];
            unsigned char b = img_data[idx + 2];
            unsigned char a = img_data[idx + 3];

            if (a < 128) continue; // Skip transparency

            int max_val = (r > g) ? (r > b ? r : b) : (g > b ? g : b);

            if (max_val > 25) {
                char ch;
                if (max_val > 90) {
                    ch = '#';
                } else {
                    if ((x + y) % 2 == 0) {
                        ch = ';';
                    } else {
                        continue;
                    }
                }

                points[point_count++] = (Point){
                    .x = (x - cx) / cx,
                    .y = (cy - y) / cy,
                    .z = 0.0,
                    .r = r, .g = g, .b = b,
                    .ch = ch
                };

                if (make_thick) {
                    double offsets[4][2] = {{-0.02, 0}, {0.02, 0}, {0, -0.02}, {0, 0.02}};
                    for (int o = 0; o < 4; o++) {
                        points[point_count++] = (Point){
                            .x = ((x - cx) / cx) + offsets[o][0],
                            .y = ((cy - y) / cy) + offsets[o][1],
                            .z = 0.0,
                            .r = r, .g = g, .b = b,
                            .ch = ch
                        };
                    }
                }
            }
        }
    }
    stbi_image_free(img_data);
    *out_points = points;
    return point_count;
}

void render_loop(Point *points, int point_count, int mode_2d, double duration_limit, double speed_multiplier) {
    double A = 0, B = 0, C = 0;
    printf("\x1b[2J\x1b[?25l");
    fflush(stdout);

    int prev_w = 0, prev_h = 0;
    char **old_ch = NULL;
    int **old_r = NULL, **old_g = NULL, **old_b = NULL;

    time_t start_time = time(NULL);

    while (1) {
        if (duration_limit > 0 && (difftime(time(NULL), start_time) >= duration_limit)) {
            break;
        }

        int term_width, term_height;
        get_terminal_size(&term_width, &term_height);

        if (term_width != prev_w || term_height != prev_h) {
            if (old_ch) {
                for (int i = 0; i < prev_h; i++) {
                    free(old_ch[i]); free(old_r[i]); free(old_g[i]); free(old_b[i]);
                }
                free(old_ch); free(old_r); free(old_g); free(old_b);
            }
            old_ch = malloc(term_height * sizeof(char *));
            old_r = malloc(term_height * sizeof(int *));
            old_g = malloc(term_height * sizeof(int *));
            old_b = malloc(term_height * sizeof(int *));
            for (int i = 0; i < term_height; i++) {
                old_ch[i] = malloc(term_width);
                old_r[i] = malloc(term_width * sizeof(int));
                old_g[i] = malloc(term_width * sizeof(int));
                old_b[i] = malloc(term_width * sizeof(int));
                memset(old_ch[i], ' ', term_width);
                for (int j = 0; j < term_width; j++) {
                    old_r[i][j] = 0; old_g[i][j] = 0; old_b[i][j] = 0;
                }
            }
            prev_w = term_width;
            prev_h = term_height;
            printf("\x1b[2J");
        }

        char **screen_ch = malloc(term_height * sizeof(char *));
        int **screen_r = malloc(term_height * sizeof(int *));
        int **screen_g = malloc(term_height * sizeof(int *));
        int **screen_b = malloc(term_height * sizeof(int *));
        double **screen_z = malloc(term_height * sizeof(double *));

        for (int i = 0; i < term_height; i++) {
            screen_ch[i] = malloc(term_width);
            screen_r[i] = malloc(term_width * sizeof(int));
            screen_g[i] = malloc(term_width * sizeof(int));
            screen_b[i] = malloc(term_width * sizeof(int));
            screen_z[i] = malloc(term_width * sizeof(double));

            memset(screen_ch[i], ' ', term_width);
            for (int j = 0; j < term_width; j++) {
                screen_r[i][j] = 0; screen_g[i][j] = 0; screen_b[i][j] = 0;
                screen_z[i][j] = -9999.0;
            }
        }

        A += 0.03 * speed_multiplier;
        B += 0.02 * speed_multiplier;
        C += 0.06 * speed_multiplier;

        // Force a true square virtual bounding box, accounting for terminal character cell height/width aspect ratio (~2:1)
        double effective_width = term_width / 2.0;
        double square_size = (effective_width < term_height) ? effective_width : term_height;
        double scale = square_size * (mode_2d ? 0.35 : 0.55);

        for (int i = 0; i < point_count; i++) {
            double x = points[i].x;
            double y = points[i].y;
            double z = points[i].z;

            double x3, y3, z3 = 0.0;

            if (mode_2d) {
                double cosC = cos(C), sinC = sin(C);
                x3 = x * cosC - y * sinC;
                y3 = x * sinC + y * cosC;

                int xp = (int)(term_width / 2 + x3 * scale * 2.0);
                int yp = (int)(term_height / 2 - y3 * scale);

                if (xp >= 0 && xp < term_width && yp >= 0 && yp < term_height) {
                    screen_ch[yp][xp] = points[i].ch;
                    screen_r[yp][xp] = points[i].r;
                    screen_g[yp][xp] = points[i].g;
                    screen_b[yp][xp] = points[i].b;
                }
            } else {
                double sinA = sin(A), cosA = cos(A);
                double sinB = sin(B), cosB = cos(B);
                double sinC = sin(C), cosC = cos(C);

                double y1 = y * cosA - z * sinA;
                double z1 = y * sinA + z * cosA;
                double x1 = x;

                double x2 = x1 * cosB + z1 * sinB;
                z3 = -x1 * sinB + z1 * cosB;
                double y2 = y1;

                x3 = x2 * cosC - y2 * sinC;
                y3 = x2 * sinC + y2 * cosC;

                double ooz = 1.0 / (z3 + 2.0);
                int xp = (int)(term_width / 2 + x3 * ooz * scale * 2.0);
                int yp = (int)(term_height / 2 - y3 * ooz * scale);

                if (xp >= 0 && xp < term_width && yp >= 0 && yp < term_height) {
                    if (z3 > screen_z[yp][xp]) {
                        screen_z[yp][xp] = z3;
                        screen_ch[yp][xp] = points[i].ch;
                        screen_r[yp][xp] = points[i].r;
                        screen_g[yp][xp] = points[i].g;
                        screen_b[yp][xp] = points[i].b;
                    }
                }
            }
        }

        for (int i = 0; i < term_height; i++) {
            for (int j = 0; j < term_width; j++) {
                char c = screen_ch[i][j];
                int r = screen_r[i][j];
                int g = screen_g[i][j];
                int b = screen_b[i][j];

                if (c != old_ch[i][j] || (c != ' ' && (r != old_r[i][j] || g != old_g[i][j] || b != old_b[i][j]))) {
                    printf("\x1b[%d;%dH", i + 1, j + 1);
                    if (c == ' ') {
                        putchar(' ');
                    } else {
                        printf("\x1b[38;2;%d;%d;%dm%c\x1b[0m", r, g, b, c);
                    }
                    old_ch[i][j] = c;
                    old_r[i][j] = r;
                    old_g[i][j] = g;
                    old_b[i][j] = b;
                }
            }
        }
        fflush(stdout);

        for (int i = 0; i < term_height; i++) {
            free(screen_ch[i]); free(screen_r[i]); free(screen_g[i]); free(screen_b[i]); free(screen_z[i]);
        }
        free(screen_ch); free(screen_r); free(screen_g); free(screen_b); free(screen_z);

        usleep(25000);
    }

    if (old_ch) {
        for (int i = 0; i < prev_h; i++) {
            free(old_ch[i]); free(old_r[i]); free(old_g[i]); free(old_b[i]);
        }
        free(old_ch); free(old_r); free(old_g); free(old_b);
    }
}

int main(int argc, char *argv[]) {
    char distro[128] = "";
    int mode_2d = 0;
    int run_crash = 0;
    int is_larp = 0;
    double speed_multiplier = 1.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "2") == 0) {
            mode_2d = 1;
        } else if (strcmp(argv[i], "all") == 0 || strcmp(argv[i], "help") == 0) {
            show_help();
        } else if (strcmp(argv[i], "crash") == 0) {
            run_crash = 1;
        } else if (strcmp(argv[i], "larp") == 0) {
            is_larp = 1;
        } else {
            size_t len = strlen(argv[i]);
            if (len > 0 && argv[i][len - 1] == '%') {
                char temp[32];
                strncpy(temp, argv[i], len - 1);
                temp[len - 1] = '\0';
                double pct = atof(temp);
                speed_multiplier = pct / 100.0;
            } else {
                if (distro[0] == '\0') {
                    strncpy(distro, argv[i], sizeof(distro) - 1);
                }
            }
        }
    }

    if (is_larp) {
        int has_display = (getenv("DISPLAY") != NULL);
        if (is_android_env() && !has_display) {
            printf("Error: no pc wm (unless using Samsung Dex)\n");
            return 1;
        }
        printf("[larp window] Spawning graphical window wrapper...\n");
        int ret = system("xterm -hold -e ./spos debian &");
        if (ret != 0) {
            system("kitty ./spos debian &");
        }
        return 0;
    }

    if (run_crash) {
        DIR *d = opendir("distros");
        if (!d) {
            printf("Error: 'distros' directory not found!\n");
            return 1;
        }
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            char *dot = strrchr(dir->d_name, '.');
            if (dot && (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)) {
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "distros/%s", dir->d_name);
                Point *points = NULL;
                int count = load_image_points(filepath, &points, 0);
                if (count > 0) {
                    render_loop(points, count, mode_2d, 1.0, speed_multiplier);
                    free(points);
                }
            }
        }
        closedir(d);
        printf("\x1b[2J\x1b[?25h");
        return 0;
    }

    if (distro[0] == '\0') {
        get_real_os_distro(distro, sizeof(distro));
    }

    char path[256];
    Point *points = NULL;
    int count = 0;

    snprintf(path, sizeof(path), "distros/%s.png", distro);
    count = load_image_points(path, &points, is_larp);
    if (!count) {
        snprintf(path, sizeof(path), "distros/%s.jpg", distro);
        count = load_image_points(path, &points, is_larp);
    }
    if (!count) {
        snprintf(path, sizeof(path), "distros/%s.jpeg", distro);
        count = load_image_points(path, &points, is_larp);
    }
    if (!count) {
        snprintf(path, sizeof(path), "distros/%s", distro);
        count = load_image_points(path, &points, is_larp);
    }

    if (!count) {
        char real_distro[128];
        get_real_os_distro(real_distro, sizeof(real_distro));
        snprintf(path, sizeof(path), "distros/%s.png", real_distro);
        count = load_image_points(path, &points, is_larp);
        if (!count) {
            snprintf(path, sizeof(path), "distros/%s.jpg", real_distro);
            count = load_image_points(path, &points, is_larp);
        }
    }

    if (!count) {
        printf("Error: Could not find image for distro '%s' in 'distros/' folder!\n", distro);
        return 1;
    }

    render_loop(points, count, mode_2d, 0.0, speed_multiplier);
    free(points);
    return 0;
}
