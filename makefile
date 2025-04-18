CC = cc
CFLAGS = -I./src -I./chttp -I./chttp/lib -I./
LDFLAGS = -lws2_32

SRC = \
	chttp/server.c \
	chttp/router.c \
	chttp/lib/sqlite3.c \
	chttp/lib/cjson.c \
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

nuke:
	make clean && make clean-db

.PHONY: all run clean clean-db nuke
