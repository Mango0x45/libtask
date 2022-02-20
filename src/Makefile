.POSIX:

CFLAGS = -ansi -pipe -O3 -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -pedantic
PREFIX = /usr

all: libtask.so

libtask.so: libtask.c task.h
	${CC} ${CFLAGS} -o $@ -fPIC -shared libtask.c

install:
	mkdir -p ${PREFIX}/{include,lib}
	cp libtask.so ${PREFIX}/lib
	cp task.h ${PREFIX}/include

clean:
	rm libtask.so
