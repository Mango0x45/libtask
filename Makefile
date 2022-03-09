.POSIX:

PREFIX = /usr
MANDIR = ${PREFIX}/share/man

all:
install:
	cd src/; make && make install
	mkdir -p ${MANDIR}/man3 ${MANDIR}/man5
	cp man/*.3 ${MANDIR}/man3
	cp man/*.5 ${MANDIR}/man5
