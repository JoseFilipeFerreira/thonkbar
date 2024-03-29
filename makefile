ifndef PREFIX
	PREFIX = /usr/local
endif
ifndef MANPREFIX
	MANPREFIX = $(PREFIX)/share/man
endif

CC= gcc
CFLAGS= -std=c2x -Wall -Wextra -Wdouble-promotion -Werror=pedantic -Werror=vla -pedantic-errors -Wfatal-errors -flto -march=native -mtune=native
CLIBS= -lpthread -liniparser

EXEC= thonkbar

$(EXEC): thonkbar.c
	$(CC) thonkbar.c -O2 -o $(EXEC) $(CFLAGS) $(CLIBS)

debug: thonkbar.c
	$(CC) -g -O0 -DNDEBUG thonkbar.c -o $(EXEC) $(CFLAGS) $(CLIBS)

clean:
	rm $(EXEC)

install: $(EXEC)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(EXEC) $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXEC)
