all: cli

cli: inet.c triad.c main.c
	gcc -lncurses -lreadline -lpthread -o cli inet.c triad.c main.c

clean:
	@rm cli

TAGS:
	ctags *.{c,h}
