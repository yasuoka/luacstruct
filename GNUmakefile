CC?=		cc
CFLAGS=		-O0 -g -Wall -Wextra
LUA?=		luajit
LUA_CFLAGS:=	$(shell pkg-config --cflags ${LUA})
LUA_LDADD:=	$(shell pkg-config --libs ${LUA})
LUACS_SRCS=	luacstruct.c

CFLAGS+=	-DLIBBSD_OVERLAY -I/usr/include/bsd ${LUA_CFLAGS}

luacstruct.o:
	${CC} -c ${CFLAGS} luacstruct.c

CLEANFILES+=	luacstruct.o

clean:
	rm -f ${CLEANFILES}
