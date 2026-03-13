# Makefile for Post Office Server (PO)

PROG=	po
SRCS=	po.c

DPADD+= ${LIBSYS} ${LIBTIMERS} ${LIBC}
LDADD+= -lsys -ltimers -lc

MAN=

BINDIR?= /usr/sbin

BINDIR?= /usr/sbin
FILES=po.conf
FILESNAME=po
FILESDIR= /etc/system.conf.d

.include <minix.service.mk>
