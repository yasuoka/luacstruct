CC?=		cc
CFLAGS=		-O0 -g -Wall -Wextra
LUA?=		lua51
LUA_CFLAGS!=	pkg-config --cflags ${LUA}
LUA_LDADD!=	pkg-config --libs ${LUA}
LUACS_SRCS=	luacstruct.c

CFLAGS+=	--shared -fPIC -DLUACS_DEBUG
CFLAGS+=	${LUA_CFLAGS}

all: extra.so

extra.so: luacstruct.c luacstruct.h extra.c
	${CC} ${CFLAGS} -o $@ luacstruct.c extra.c

CLEANFILES+=	extra.so

clean:
	rm -f ${CLEANFILES}

test: all
	${LUA} extra_test.lua
