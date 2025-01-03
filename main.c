#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <ncurses.h>
#include <pthread.h> 

#include "project_macro.h"

// 함수 선언
int display_folder(const char *directory, const int startline_idx, const int highlighted_idx, char *selected_filename, size_t filename_size);
void display_file(const char *file_path);
int execute_command(char *current_dir, const char *selected_filename);
void execute_command_in_ncurses(const char *command);

// 클립보드 관련 함수 선언
void set_clipboard_copy(const char *file_path);
void set_clipboard_cut(const char *file_path);
void paste_clipboard_file(const char *current_dir);

// ALRM 시그널 핸들러
void aram_sig_handle(int sig) {
    endwin();
    printf("Program ended because of inactivity.\n");
    exit(0);
}

char current_dir[MAX_DIR_LENGTH] = "~"; // 초기 디렉토리 설정
int highlighted_idx = 0;     // 파일 리스트 중 선택된 파일 인덱스, 0 - filecount-1
int print_start_idx = 0;     // 파일 리스트 중 출력 시작 줄 인덱스 0 - filecount-1

int main() {
    setlocale(LC_ALL, "");
    // ncurses 초기화
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    // 타임아웃 시그널 핸들러 설정
    signal(SIGALRM, aram_sig_handle);
    alarm(300); // 300초 타임아웃 설정

    char *home_dir = getenv("HOME");
    if (home_dir != NULL) {
        strncpy(current_dir, home_dir, sizeof(current_dir) - 1);
    }

    char selected_filename[MAX_FILENAME_LENGTH];

    // 현재 상태: 99 - 폴더 출력, 1 - 파일 출력
    int return_value = 99;

    while (1) {
        clear();

        
        // 폴더 내용을 출력하고 파일 개수 반환
        int file_count = display_folder(current_dir, print_start_idx, highlighted_idx, selected_filename, sizeof(selected_filename));

        // 키 입력 처리
        int ch = getch();
        if (ch != ERR) {
            alarm(300); // 키 입력 시 타이머 재설정
        }
        int screen_height, screen_width; // screen_width 유지
        getmaxyx(stdscr, screen_height, screen_width); // 올바른 lvalue 사용



        char full_path[1024];
        // (1) 예상 길이 확인
        if (strlen(current_dir) + strlen(selected_filename) + 2 >= sizeof(full_path)) {
            mvprintw(1, 0, "Path 가 너무 길어 사용 불가능합니다");
            return 0;
        }
        // (2) snprintf로 파일 전체 경로 생성
        snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, selected_filename);

        switch (ch) {
            case KEY_UP:
                if (highlighted_idx > 0) {
                    highlighted_idx--;
                    if (highlighted_idx < print_start_idx) {
                        print_start_idx--;
                    }
                }
                break;
            case KEY_DOWN:
                if (highlighted_idx < file_count - 1) {
                    highlighted_idx++;
                    if (highlighted_idx - print_start_idx == screen_height - RESERVED_LINE_NO - 1) {
                        print_start_idx++;
                    }
                }
                break;
            case 'p': // 'p'로 ps 명령 실행
                execute_command_in_ncurses("ps");
                break;
            case 'w': // 'w'로 who 명령 실행
                execute_command_in_ncurses("who");
                break;
            case ' ': 
                return_value = execute_command(current_dir, selected_filename);
                if (return_value == 99) {
                    highlighted_idx = 0;
                    print_start_idx = 0;
                }

                break; 
            case 'q': // 종료
                endwin();
                return 0;
            case 'c': // 복사
                set_clipboard_copy(full_path);
                break;
            case 'x': // 잘라내기
                set_clipboard_cut(full_path);
                break;
            case 'v': // 붙여넣기
                paste_clipboard_file(current_dir);
                break;
            default:
                break;
        }
        
    }
}
