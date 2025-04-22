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

nuke: clean clean-db

build: all run

rebuild: clean clean-db all run

compile: clean all run

migrate: clean-db run

.PHONY: all run clean clean-db nuke build rebuild compile migrate
