LUA?=		luajit
LUA_CFLAGS:=	$(shell pkg-config --cflags ${LUA})
LUA_LDADD:=	$(shell pkg-config --libs ${LUA})

CFLAGS=		--shared -fPIC -DLUACS_DEBUG -fvisibility=hidden
CFLAGS+=	-DLIBBSD_OVERLAY -I/usr/include/bsd -I${CURDIR}/.. ${LUA_CFLAGS}

all: test

MODULES=	

test_wstr.so: test_wstr.c
	${CC} ${CFLAGS} -DLUACS_VARIANT=\"t1\" -o $@ \
	    ${CURDIR}/../luacstruct.c ${CURDIR}/test_wstr.c ${LUA_LDADD}
MODULES+=	test_wstr.so

test_str.so: test_str.c
	${CC} ${CFLAGS} -DLUACS_VARIANT=\"t2\" -o $@ \
	    ${CURDIR}/../luacstruct.c ${CURDIR}/test_str.c ${LUA_LDADD}
MODULES+=	test_str.so

test_extra.so: test_extra.c
	${CC} ${CFLAGS} -DLUACS_VARIANT=\"t3\" -o $@ \
	    ${CURDIR}/../luacstruct.c ${CURDIR}/test_extra.c ${LUA_LDADD}
MODULES+=	test_extra.so

test:	${MODULES}
	${LUA} test.lua

clean:
	rm -f ${MODULES}
