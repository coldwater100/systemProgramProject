#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>
#include <wchar.h>
#include <ncurses.h>

#include "project_macro.h"

// 문자열을 지정된 길이로 자르고 출력하는 함수
// 한글과 영문이 같은 넓이를 차지하도록 조정하여 max_width 길이의 문자열을 출력
void print_trimmed(const char *str, int max_width) {
    setlocale(LC_ALL, "");
    int width = 0;
    const char *ptr = str;

    while (*ptr && width < max_width) {
        wchar_t wc;
        int char_len = mbtowc(&wc, ptr, MB_CUR_MAX);
        if (char_len <= 0) {
            break;
        }

        int char_width = wcwidth(wc);
        if (char_width < 0) {
            char_width = 0;
        }

        if (width + char_width > max_width) {
            break;
        }

        addnstr(ptr, char_len);
        width += char_width;
        ptr += char_len;
    }

    for (int i = width; i < max_width; i++) {
        addch(' ');
    }
}

int sort_by_name(const struct dirent **a, const struct dirent **b) {
    return strcmp((*a)->d_name, (*b)->d_name);
}

// 디렉토리의 파일 정보를 출력하는 함수
int display_folder(const char *directory, const int print_start_idx, const int highlighted_idx, char *selected_filename, size_t filename_size) {
    struct dirent **filelist = NULL;
    int file_count = scandir(directory, &filelist, NULL, sort_by_name);

    if (file_count < 0) {
        mvprintw(1, 0, "Error reading directory: %s", directory);
        return 0;
    }

    // ncurses 색상 초기화
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK); // 파란색 글자, 검은색 배경

    int screen_height;
    int screen_width;
    getmaxyx(stdscr, screen_height, screen_width);

    mvprintw(0, 0, "======================");
    mvprintw(1, 0, "current directory :  %s", directory); 
    mvprintw(2, 0, "----------------------");
    mvprintw(3, 0, "%-30s %-10s %-10s %-20s", "Filename", "Kind", "Size", "Modified");
    mvprintw(4, 0, "----------------------");

    int print_start_screenY = RESERVED_LINE_UPPER;
    int print_end_screenY = screen_height - RESERVED_LINE_LOWER - 1;
    int current_screenY = print_start_screenY;

    for (int file_idx = print_start_idx; file_idx < file_count && current_screenY <= print_end_screenY; file_idx++, current_screenY++) {
        struct stat file_stat;
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", directory, filelist[file_idx]->d_name);

        if (stat(filepath, &file_stat) == 0) {
            char *kind = (S_ISDIR(file_stat.st_mode)) ? "DIR" : "FILE";

            char mod_time[20];
            strftime(mod_time, sizeof(mod_time), "%Y-%m-%d %H:%M", localtime(&file_stat.st_mtime));

            if (file_idx == highlighted_idx) {
                attron(A_REVERSE);
                strncpy(selected_filename, filelist[file_idx]->d_name, filename_size - 1);
                selected_filename[filename_size - 1] = '\0';
            }

            if (S_ISDIR(file_stat.st_mode)) { // 디렉토리인 경우 색상 설정
                attron(COLOR_PAIR(1));
            }

            move(current_screenY, 0);
            print_trimmed(filelist[file_idx]->d_name, 30);
            mvprintw(current_screenY, 30, "%-10s %-10ld %-20s", kind, file_stat.st_size, mod_time);

            if (S_ISDIR(file_stat.st_mode)) { // 색상 해제
                attroff(COLOR_PAIR(1));
            }

            if (file_idx == highlighted_idx) {
                attroff(A_REVERSE);
            }
        }
    }

    mvprintw(screen_height - 4, 0, "-------------------------------------------");
    mvprintw(screen_height - 3, 0, "Execute ps(p)  Execute who(w)");
    mvprintw(screen_height - 2, 0, "Quit(q)  Copy(c)  Cut(x)  Paste(v)  Execute(Space Bar)");
    mvprintw(screen_height - 1, 0, "===========================================");

    for (int i = 0; i < file_count; ++i) {
        free(filelist[i]);
    }
    free(filelist);

    return file_count;
}

// 파일 내용을 출력하는 함수
void display_file(const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        mvprintw(1, 0, "Error opening file: %s", file_path);
        return;
    }

    int y_offset = 0; // 파일 출력 시작 위치
    char line[1024];

    while (1) {
        clear();
        int y = 0;
        fseek(file, 0, SEEK_SET);
        for (int i = 0; i < y_offset; i++) {
            if (!fgets(line, sizeof(line), file)) {
                break;
            }
        }

        while (fgets(line, sizeof(line), file) != NULL && y < LINES - RESERVED_LINE_LOWER) {
            mvprintw(y++, 0, "%s", line);
        }

        mvprintw(LINES - 1, 0, "Use UP/DOWN to scroll, Q to quit");
        refresh(); // 화면 갱신

        int ch = getch();
        if (ch == 'q') {
            fclose(file);
            return;
        } else if (ch == KEY_UP && y_offset > 0) {
            y_offset--;
        } else if (ch == KEY_DOWN) {
            y_offset++;
        }
    }
}
