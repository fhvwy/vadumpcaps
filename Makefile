
PREFIX := /usr/local
CFLAGS := -Wall -g

vadumpcaps: vadumpcaps.c
	$(CC) -o $@ $(CFLAGS) $< $(shell pkg-config --libs --cflags libva libva-drm)

install: vadumpcaps
	install -t $(PREFIX)/bin $<
