CC=gcc
CFLAGS=-Wall -Wextra
LIBS=-lcurl -lcjson

all:
	$(CC) $(CFLAGS) main.c -o speedtest $(LIBS)

clean:
	rm -f speedtest
