CC = cc
CFLAGS = -I./src -I./chttp -I./chttp/lib -I./
LDFLAGS = -lws2_32

SRC = \
	chttp/main.c \
	chttp/router.c \
	chttp/lib/sqlite3.c \
	chttp/lib/cjson.c \
	src/handlers.c \

OUT = server.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)

.PHONY: all clean
