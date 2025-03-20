LUA?=		luajit
LUA_CFLAGS:=	$(shell pkg-config --cflags ${LUA})
LUA_LDADD:=	$(shell pkg-config --libs ${LUA})

CFLAGS=		--shared -fPIC -DLUACS_DEBUG -fvisibility=hidden ${LUA_CFLAGS}
CFLAGS+=	-DLIBBSD_OVERLAY -I/usr/include/bsd -I${CURDIR}/../..

all: socket.so

MODULES=	

socket.so: socket.c
	${CC} ${CFLAGS} -DLUACS_VARIANT=\"s1\" -o $@ \
	    ${CURDIR}/../../luacstruct.c ${CURDIR}/socket.c ${LUA_LDADD}
MODULES+=	socket.so

clean:
	rm -f ${MODULES}

