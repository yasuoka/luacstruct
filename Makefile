CC?=		cc
CFLAGS=		-O0 -g -Wall -Wextra
LUA?=		lua51
LUA_CFLAGS!!=	pkg-config --cflags ${LUA}
LUA_LDADD!!=	pkg-config --libs ${LUA}
LUACS_SRCS=	luacstruct.c

CFLAGS+=	${LUA_CFLAGS}

luacstruct.o:
	${CC} -c ${CFLAGS} luacstruct.c

CLEANFILES+=	luacstruct.o

clean:
	rm -f ${CLEANFILES}
