.POSIX:

CFLAGS = -ansi -pipe -O3 -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -pedantic

all: libtask.so

libtask.so: libtask.c task.h
	${CC} ${CFLAGS} -o $@ -fPIC -shared libtask.c

clean:
	rm libtask.so
