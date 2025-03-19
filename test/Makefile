LUA?=		lua51
LUA_CFLAGS!=	pkg-config --cflags ${LUA}
LUA_LDADD!=	pkg-config --libs ${LUA}

CFLAGS=		--shared -fPIC -DLUACS_DEBUG
CFLAGS+=	-I${.CURDIR}/.. ${LUA_CFLAGS}

all: test

MODULES=	

test_wstr.so: test_wstr.c
	${CC} ${CFLAGS} -o $@ ${.CURDIR}/../luacstruct.c \
	    ${.CURDIR}/test_wstr.c ${LUA_LDADD}
MODULES+=	test_wstr.so

test_str.so: test_str.c
	${CC} ${CFLAGS} -o $@ ${.CURDIR}/../luacstruct.c \
	    ${.CURDIR}/test_str.c ${LUA_LDADD}
MODULES+=	test_str.so

test:	${MODULES}
	${LUA} test.lua

clean:
	rm -f ${MODULES}
