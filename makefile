CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11

kwm: kwm.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f kwm

run:
	env DISPLAY=:1 ./kwm

install:
	install -m 755 kwm /usr/bin/

xephyr:
	Xephyr -ac -br -noreset -screen 1024x768 :1

.PHONY: clean
