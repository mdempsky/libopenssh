#	$OpenBSD: Makefile,v 1.61 2013/07/02 13:32:38 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1 ssh_config.5
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c mux.c \
	roaming_common.c roaming_client.c

.include <bsd.own.mk>

# XXX leave on while refactoring to ensure we don't break it.
KERBEROS5=yes

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV -DGSSAPI
.endif # KERBEROS5

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
DPADD+=  ${LIBGSSAPI} ${LIBKRB5}
LDADD+=  -lgssapi -lkrb5 -lasn1
LDADD+=  -lwind -lroken -lcom_err -lpthread -lheimbase
.endif # KERBEROS5

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
