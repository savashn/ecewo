CC = cc
CFLAGS = -I./src -I./chttp
LDFLAGS = -lws2_32
SRC = chttp/main.c chttp/router.c chttp/utils.c src/handlers.c
OUT = server.exe

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUTPUT)

.PHONY: all clean
