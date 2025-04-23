CC = cc
CFLAGS =  -I./ -I./ecewo -I./src
LDFLAGS = -lws2_32

SRC = \
	ecewo/server.c \
	ecewo/router.c \
	ecewo/lib/sqlite3.c \
	ecewo/lib/cjson.c \
	ecewo/utils/params.c \
	ecewo/utils/query.c \
	ecewo/utils/headers.c \
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

.PHONY: all run clean clean-db nuke build rebuild compile migrate
