all:
	gcc -g -o triad *.c -lncurses -lreadline -lpthread

TAGS:
	ctags *.{c,h}
