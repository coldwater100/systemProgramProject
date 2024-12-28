# 컴파일러
CC = gcc

# 컴파일 플래그
CFLAGS = -g -Wall -D_XOPEN_SOURCE=600 -D_DEFAULT_SOURCE

# ncursesw 라이브러리
LDFLAGS = -lncursesw

# 파일들
SRCS = main.c display.c execute.c
OBJS = $(SRCS:.c=.o)
HEADERS = project_macro.h

# 실행 파일 이름
TARGET = program

# 기본 빌드 목표
all: $(TARGET)

# 실행 파일 생성
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# 개별 파일 컴파일
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# 클린
clean:
	rm -f $(OBJS) $(TARGET)

# 다시 빌드
rebuild: clean all
