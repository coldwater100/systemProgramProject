#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>

#include "project_macro.h"

void display_file(const char *file_path); // display.c에 있는 함수

// 명령 실행 결과를 ncurses 화면에 출력하는 함수
void execute_command_in_ncurses(const char *command) {
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1) {
        mvprintw(LINES - 1, 0, "Error: Unable to create pipe.");
        refresh();
        getch();
        return;
    }

    pid = fork();
    if (pid == -1) {
        mvprintw(LINES - 1, 0, "Error: Unable to fork process.");
        refresh();
        getch();
        return;
    }

    if (pid == 0) { // 자식 프로세스
        close(pipefd[0]); // 읽기 끝 닫기
        dup2(pipefd[1], STDOUT_FILENO); // 표준 출력을 파이프 쓰기로 연결
        dup2(pipefd[1], STDERR_FILENO); // 표준 오류도 파이프 쓰기로 연결
        close(pipefd[1]);

        execlp(command, command, NULL); // 명령 실행
        perror("execlp");
        exit(EXIT_FAILURE);
    } else { // 부모 프로세스
        close(pipefd[1]); // 쓰기 끝 닫기

        char buffer[256];
        int line = 0;

        clear();
        mvprintw(0, 0, "Output of '%s' command:", command);

        FILE *fp = fdopen(pipefd[0], "r");
        if (!fp) {
            mvprintw(LINES - 1, 0, "Error: Unable to open pipe.");
            refresh();
            getch();
            return;
        }

        while (fgets(buffer, sizeof(buffer), fp)) {
            buffer[strcspn(buffer, "\n")] = '\0'; // 개행 제거
            mvprintw(++line, 0, "%s", buffer);
            if (line >= LINES - RESERVED_LINE_LOWER) {
                mvprintw(line, 0, "-- More -- Press SPACE to continue");
                refresh();
                while (getch() != ' ') {
                    // SPACE 키 대기
                }
                clear();
                line = 0;
            }
        }

        fclose(fp); // 파일 포인터 닫기

        mvprintw(LINES - 1, 0, "Press any key to return...");
        refresh();
        getch(); // 종료 대기

        // 원래 ncurses 상태 복원
        clear();
        refresh();
    }
}

// return 99 - current_dir changed (디렉토리)
// return 0 - error, current_dir not changed
// return 1 - succeeded, current_dir not changed (일반 파일)
int execute_command(char *current_dir, const char *selected_filename) {
    char full_path[1024];

    // (1) 예상 길이 확인
    if (strlen(current_dir) + strlen(selected_filename) + 2 >= sizeof(full_path)) {
        mvprintw(1, 0, "Path 가 너무 길어 사용 불가능합니다");
        return 0;
    }

    // (2) snprintf로 파일 전체 경로 생성
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, selected_filename);

    struct stat file_stat;
    if (stat(full_path, &file_stat) == 0) {
        switch (file_stat.st_mode & S_IFMT) {
            case S_IFDIR: // 디렉토리
                if (strcmp(selected_filename, ".") == 0) {
                    break; // 현재 디렉토리 무시
                } else if (strcmp(selected_filename, "..") == 0) {
                    // 상위 디렉토리 이동
                    char *last_slash = strrchr(current_dir, '/');
                    if (last_slash != NULL) {
                        *last_slash = '\0';
                    } else {
                        strncpy(current_dir, "/", MAX_DIR_LENGTH - 1);
                        current_dir[MAX_DIR_LENGTH - 1] = '\0';
                    }
                } else {
                    // 하위 디렉토리 이동
                    strncpy(current_dir, full_path, MAX_DIR_LENGTH - 1);
                    current_dir[MAX_DIR_LENGTH - 1] = '\0';
                }
                return 99;
            case S_IFREG: // 일반 파일
                if (file_stat.st_mode & S_IXUSR) {  // 실행파일  - 실행시키기
                    clear();
                    mvprintw(1, 0, "Selected = Executable file: %s", selected_filename);
                    getch();
                    execute_command_in_ncurses(full_path);
                    return 1;
                } else {  // 일반 파일 , 볼수 있으면 cat 해서 보여주기
                    clear();
                    mvprintw(1, 0, "Selected = file: %s", selected_filename);
                    mvprintw(3, 0, "Press Space to view the file...");
                    char ch;
                    while ((ch = getch()) != ' '); // space 키 입력 시 파일 보기
                    display_file(full_path);
                    return 1;
                }
            case S_IFLNK: // 심볼릭 링크(미구현)
                clear();
                mvprintw(1, 0, "Selected = symbolic link: %s", selected_filename);
                mvprintw(3, 0, "Press any key to continue...");
                getch();
                return 1;
            default:
                clear();
                mvprintw(1, 0, "Selected = Unknown file type: %s", selected_filename);
                mvprintw(3, 0, "Press any key to continue...");
                getch();
                return 1;
        }
    } else {
        clear();
        mvprintw(1, 0, "Error reading file information: %s", selected_filename);
        mvprintw(3, 0, "Press any key to continue...");
        getch();
    }

    return 0;
}
