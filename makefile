kwm: main.c; gcc -Wall -Wextra -O2 -o $@ $< -lX11
install: kwn; install -m 755 kwm /usr/bin/
run: kwm; env DISPLAY=:1 kwm
x:; Xephyr -ac -br -noreset -screen 1024x768 :1
