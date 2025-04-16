CC = cc
CFLAGS = -I./src -I./chttp -I./chttp/lib/sqlite3 -I./chttp/lib/cjson
LDFLAGS = -lws2_32

SRC = \
	chttp/main.c \
	chttp/router.c \
	chttp/utils.c \
	chttp/lib/sqlite3/sqlite3.c \
	chttp/lib/cjson/cjson.c \
	src/handlers.c \

OUT = server.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)

.PHONY: all clean
