CC = clang
CSTYLE = -Wno-gnu-offsetof-extensions
CFLAGS = -Wall -g -O1 -Wextra -pedantic -std=c99 -L/usr/local/lib -lcglm #-fsanitize=address
LDFLAGS = -lSDL2 -lm

SRC = main.c engine.c pipeline.c
OUT = app

all:
	$(CC) $(CSTYLE) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
