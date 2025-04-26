# ecewo v0.10.0
# 2025 © Savas <savashn> Sahin

CC = cc
CFLAGS =  -I./ -I./ecewo -I./src
LDFLAGS = -lws2_32

SRC = \
	ecewo/server.c \
    ecewo/router.c \
    ecewo/request.c \
    ecewo/lib/session.c \
    ecewo/lib/sqlite3.c \
    ecewo/lib/cjson.c \
	src/main.c \
	src/handlers.c \
	src/db.c \

OUT = server.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: all
	./${OUT}

clean:
	rm -f $(OUT)

clean-db:
	rm -f sql.db

build: clean all run

build-all: clean clean-db all run

.PHONY: all run clean clean-db build build-all
