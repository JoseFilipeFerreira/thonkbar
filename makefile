ifndef PREFIX
	PREFIX = /usr/local
endif
ifndef MANPREFIX
	MANPREFIX = $(PREFIX)/share/man
endif

CC= cc
CFLAGS= -lpthread -std=c2x -Wall -Wextra -Wdouble-promotion -Werror=pedantic -Werror=vla -pedantic-errors -Wfatal-errors -flto -march=native -mtune=native

EXEC= thonkbar
DAEMON = thonkbar_daemon

build: thonkbar.c
	$(CC) thonkbar.c -O2 -o $(EXEC) $(CFLAGS)

debug: thonkbar.c
	$(CC) -g -DNDEBUG thonkbar.c -o $(EXEC) $(CFLAGS)

clean:
	rm $(EXEC)

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(EXEC) $(DESTDIR)$(PREFIX)/bin/
	cp -f $(DAEMON) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXEC)
	rm -f $(DESTDIR)$(PREFIX)/bin/$(DAEMON)
