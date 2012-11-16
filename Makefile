# Makefile for the hello driver.
PROG=	secret
SRCS=	secret.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

test: test.c
	gcc -g -Wall test.c  -o test

grant: grant.c
	gcc -g -Wall grant.c  -o grant


MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
