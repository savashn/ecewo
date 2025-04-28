# ecewo v0.11.0
# 2025 © Savas Sahin <savashn>

CC = gcc
CFLAGS =  -I./ -I./ecewo -I./ecewo/lib -I./src
LDFLAGS = -lws2_32

SRC = \
	ecewo/server.c \
    ecewo/router.c \
	ecewo/routes.c \
    ecewo/request.c \
    ecewo/lib/session.c \
    ecewo/lib/cjson.c \

OUT = server.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: all
	./${OUT}

clean:
	rm -f $(OUT)

build: clean all run

.PHONY: all run clean build
