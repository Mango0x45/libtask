.POSIX:

CFLAGS = -ansi -pipe -O3 -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -pedantic
PREFIX = /usr

sources = libtask.o task.h
outputs = libtask.so libtask.a

all: ${outputs}

libtask.o: libtask.c

libtask.a: ${sources}
	${AR} ${ARFLAGS} $@ libtask.o

libtask.so: ${sources}
	${CC} ${CFLAGS} -o $@ -fPIC -shared libtask.o

install:
	mkdir -p ${PREFIX}/include ${PREFIX}/lib
	cp ${outputs} ${PREFIX}/lib
	cp task.h ${PREFIX}/include

clean:
	rm -f ${outputs} libtask.o
