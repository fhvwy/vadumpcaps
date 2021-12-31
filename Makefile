
PREFIX := /usr/local
CFLAGS := -Wall -Wundef -g

vadumpcaps: vadumpcaps.c
	$(CC) -o $@ $(CFLAGS) $< $(shell pkg-config --libs --cflags libva libva-drm)

clean:
	rm -f vadumpcaps

install: vadumpcaps
	install -t $(PREFIX)/bin $<
