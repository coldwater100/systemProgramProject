#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/stat.h>

#include "project_macro.h"

void generate_unique_filename(char *destination, const char *base_path, const char *filename) {
    // 기본 파일 경로 생성
    snprintf(destination, 1024, "%s/%s", base_path, filename);

    // 파일이 존재하지 않으면 그대로 반환
    if (access(destination, F_OK) != 0) {
        return;
    }

    // 파일이 존재하면 시간 정보를 포함하여 새로운 이름 생성
    // 파일 확장자 추출
    const char *ext = strrchr(filename, '.');
    char name_without_ext[1024];
   
    if (ext != NULL) {
        // 확장자가 있다면 이름과 확장자를 분리
        snprintf(name_without_ext, ext - filename + 1, "%s", filename);
    } else {
        // 확장자가 없다면 그대로 전체 파일 이름 사용
        snprintf(name_without_ext, sizeof(name_without_ext), "%s", filename);
        ext = ""; // 확장자가 없는 경우 빈 문자열로 처리
    }

    // 시스템 시간을 기반으로 새 파일 이름 생성
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char time_str[20];
    snprintf(time_str, sizeof(time_str), "_%04d%02d%02d%02d%02d%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    // 새로운 파일 이름을 생성
    snprintf(destination, 1024, "%s/%s%s%s", base_path, name_without_ext, time_str, ext);
}

void display_file(const char *file_path); // display.c에 있는 함수

// 클립보드 관련 변수 정의
char clipboard_file[1024] = "";  // 클립보드에 저장된 파일의 전체 경로
int clipboard_action = 0;         // 클립보드 작업 유형 (0: 없음, 1: 복사, 2: 잘라내기)

// 복사 작업을 처리하는 스레드 함수
void *copy_file_thread(void *args) {
    char **paths = (char **)args;
    char *source = paths[0];
    char *destination = paths[1];

    FILE *src = fopen(source, "rb");
    if (!src) {
        mvprintw(LINES - 1, 0, "Error: Cannot open source file %s.            ", source);
        refresh();
        free(paths);
        return NULL;
    }

    FILE *dest = fopen(destination, "wb");
    if (!dest) {
        mvprintw(LINES - 1, 0, "Error: Cannot create destination file %s.            ", destination);
        refresh();
        fclose(src);
        free(paths);
        return NULL;
    }

    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }

    fclose(src);
    fclose(dest);
    mvprintw(LINES - 1, 0, "File copied successfully: %s            ", destination);
    free(paths);
    
    return NULL;
}

// 붙여넣기 작업 처리 함수
void paste_clipboard_file(const char *current_dir) {
    if (clipboard_action == 0) {
        mvprintw(LINES - 1, 0, "Clipboard is empty.                        ");
        getch();
        refresh();
        return;
    }

    char destination[1024];
    snprintf(destination, sizeof(destination), "%s/%s", current_dir, strrchr(clipboard_file, '/') + 1);
    if (clipboard_action == 1) {  // 복사 작업
        generate_unique_filename(destination, current_dir, strrchr(clipboard_file, '/') + 1);
        char **paths = malloc(2 * sizeof(char *));
        paths[0] = strdup(clipboard_file);
        paths[1] = strdup(destination);

        pthread_t tid;
        pthread_create(&tid, NULL, copy_file_thread, paths);
        pthread_detach(tid);
        refresh();//작업이 끝나면 화면 refresh
        getch();
        clear();
    } else if (clipboard_action == 2) {  // 잘라내기 작업
        if (rename(clipboard_file, destination) == 0) {
            mvprintw(LINES - 1, 0, "File moved successfully: %s           ", destination);
            clipboard_file[0] = '\0';
            clipboard_action = 0;  // 클립보드 초기화
            refresh();
            getch();
            clear();
        } else {
            mvprintw(LINES - 1, 0, "Error: Unable to move file.            ");
            refresh();
            getch();
            clear();
        }
    }

    refresh();
}

// 복사 작업 설정 함수
void set_clipboard_copy(const char *file_path) {
    strncpy(clipboard_file, file_path, sizeof(clipboard_file) - 1);
    clipboard_action = 1;  // 복사 작업으로 설정
    mvprintw(LINES - 1, 0, "File copied to clipboard: %s            ", clipboard_file);
    getch();
    refresh();
}

// 잘라내기 작업 설정 함수
void set_clipboard_cut(const char *file_path) {
    strncpy(clipboard_file, file_path, sizeof(clipboard_file) - 1);
    clipboard_action = 2;  // 잘라내기 작업으로 설정
    mvprintw(LINES - 1, 0, "File cut to clipboard: %s            ", clipboard_file);
    getch();
    refresh();
}


// 명령 실행 결과를 ncurses 화면에 출력하는 함수
void execute_command_in_ncurses(const char *command) {
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1) {
        mvprintw(LINES - 1, 0, "Error: Unable to create pipe.            ");
        refresh();
        getch();
        return;
    }

    pid = fork();
    if (pid == -1) {
        mvprintw(LINES - 1, 0, "Error: Unable to fork process.            ");
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
            mvprintw(LINES - 1, 0, "Error: Unable to open pipe.            ");
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
                    mvprintw(3, 0, "Press any key to execute the file...");
                    getch();
                    execute_command_in_ncurses(full_path);
                    return 1;
                } else {  // 일반 파일 , 볼수 있으면 cat 해서 보여주기
                    clear();
                    mvprintw(1, 0, "Selected = file: %s", selected_filename);
                    mvprintw(3, 0, "Press any key to view the file...");
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
