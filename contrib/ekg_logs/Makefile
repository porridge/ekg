all: parse

parse: parse.c
	gcc -O2 -o parse parse.c  `glib-config --cflags --libs` # -ggdb
	strip parse

clean:
	rm -f parse
