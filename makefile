CC= gcc
CFLAGS= -lpthread -std=c2x -Wall -Wextra -Wdouble-promotion -Werror=pedantic -Werror=vla -pedantic-errors -Wfatal-errors -flto -march=native -mtune=native

EXEC= thonkbar
DAEMON = thonkbar_daemon

$(EXEC): thonkbar.c
	$(CC) thonkbar.c -O2 -o $(EXEC) $(CFLAGS)

debug: thonkbar.c
	$(CC) -g -DNDEBUG thonkbar.c -o $(EXEC) $(CFLAGS)

clean:
	rm -f $(EXEC)

install: $(EXEC) $(DAEMON)
	cp -f $(EXEC) $(DAEMON) /usr/local/bin

uninstall:
	rm /usr/local/bin/{$(EXEC),$(DAEMON)}
