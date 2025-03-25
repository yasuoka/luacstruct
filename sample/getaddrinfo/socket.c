/*
 * Copyright (c) 2025 YASUOKA Masahiko <yasuoka@yasuoka.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <lua.h>
#include <lauxlib.h>

#include "luacstruct.h"

#define EXPORT	__attribute__((visibility("default")))

static int socket_types(lua_State *);
static int lua_getaddrinfo(lua_State *);
static int lua_freeaddrinfo(lua_State *);
static int lua_addrinfo(lua_State *);
static int lua_in_addr__tostring(lua_State *);
static int lua_in6_addr__tostring(lua_State *);

static volatile int socket_initialized = 0;

/* map types to Lua by luacstruct */
int
socket_types(lua_State *L)
{
	/*
	 * sa_family
	 */
	luacs_newenum0(L, "sa_family", sizeof(sa_family_t));
	luacs_enum_declare_value(L, "AF_UNSPEC", AF_UNSPEC);
	luacs_enum_declare_value(L, "AF_INET", AF_INET);
	luacs_enum_declare_value(L, "AF_INET6", AF_INET6);
	luacs_enum_declare_value(L, "AF_UNIX", AF_UNIX);
	lua_pop(L, 1);

	/*
	 * sockaddr
	 */
	luacs_newstruct(L, sockaddr);
#ifdef HAVE_SA_LEN
	luacs_unsigned_field(L, sockaddr, sa_len, 0);
#endif
	luacs_enum_field(L, sockaddr, sa_family, sa_family, 0);
	luacs_bytearray_field(L, sockaddr, sa_data, 0);
	lua_pop(L, 1);

	/*
	 * in_addr
	 */
	luacs_newstruct(L, in_addr);
	luacs_unsigned_field(L, in_addr, s_addr, 0);
	luacs_declare_method(L, "__tostring", lua_in_addr__tostring);
	lua_pop(L, 1);

	/*
	 * sockaddr_in
	 */
	luacs_newstruct(L, sockaddr_in);
	luacs_enum_field(L, sockaddr_in, sa_family, sin_family, 0);
#ifdef HAVE_SA_LEN
	luacs_unsigned_field(L, sockaddr_in, sin_len, 0);
#endif
	luacs_unsigned_field(L, sockaddr_in, sin_port, LUACS_FENDIANBIG);
	luacs_nested_field(L, sockaddr_in, in_addr, sin_addr, 0);
	lua_pop(L, 1);

	/*
	 * in6_addr
	 */
	luacs_newstruct(L, in6_addr);
	luacs_unsigned_array_field(L, in6_addr, s6_addr, 0);
	luacs_declare_method(L, "__tostring", lua_in6_addr__tostring);
	lua_pop(L, 1);

	/*
	 * sockaddr_in6
	 */
	luacs_newstruct(L, sockaddr_in6);
	luacs_enum_field(L, sockaddr_in6, sa_family, sin6_family, 0);
#ifdef HAVE_SA_LEN
	luacs_unsigned_field(L, sockaddr_in6, sin6_len, 0);
#endif
	luacs_unsigned_field(L, sockaddr_in6, sin6_port, LUACS_FENDIANBIG);
	luacs_nested_field(L, sockaddr_in6, in6_addr, sin6_addr, 0);
	luacs_unsigned_field(L, sockaddr_in6, sin6_scope_id, 0);
	lua_pop(L, 1);

	/*
	 * put sin4 and sin6 nested field to sockaddr
	 */
	luacs_newstruct(L, sockaddr);
	luacs_declare_field(L, LUACS_TOBJENT, "sockaddr_in", "sin4",
	    sizeof(struct sockaddr_in), 0, 0, 0);
	luacs_declare_field(L, LUACS_TOBJENT, "sockaddr_in6", "sin6",
	    sizeof(struct sockaddr_in6), 0, 0, 0);
	lua_pop(L, 1);

	/*
	 * addrinfo
	 */
	luacs_newstruct(L, addrinfo);
	luacs_int_field(L, addrinfo, ai_flags, 0);
	luacs_int_field(L, addrinfo, ai_family, 0);
	luacs_int_field(L, addrinfo, ai_socktype, 0);
	luacs_int_field(L, addrinfo, ai_protocol, 0);
	luacs_unsigned_field(L, addrinfo, ai_addrlen, 0);
	luacs_objref_field(L, addrinfo, sockaddr, ai_addr, 0);
	luacs_strptr_field(L, addrinfo, ai_canonname, 0);
	luacs_objref_field(L, addrinfo, addrinfo, ai_next, 0);
	lua_pop(L, 1);

	return (0);
}

EXPORT
int
luaopen_socket(lua_State *L)
{
	if (socket_initialized++ == 0)
		socket_types(L);

	lua_newtable(L);
#define DECL_CONST(_L, _const) 			\
	lua_pushinteger(L, _const);		\
	lua_setfield(L, -2, #_const);
#define DECL_ENUM(_L, _name, _val)		\
	luacs_newenumval(L, _name, _val);	\
	lua_setfield(L, -2, #_val);

	DECL_ENUM(L, "sa_family", AF_UNSPEC);
	DECL_ENUM(L, "sa_family", AF_INET);
	DECL_ENUM(L, "sa_family", AF_INET6);
	DECL_ENUM(L, "sa_family", AF_UNIX);

	DECL_CONST(L, SOCK_STREAM);
	DECL_CONST(L, SOCK_DGRAM);
	DECL_CONST(L, SOCK_RDM);
	DECL_CONST(L, SOCK_SEQPACKET);

	DECL_CONST(L, IPPROTO_UDP);
	DECL_CONST(L, IPPROTO_TCP);

	DECL_CONST(L, AI_ADDRCONFIG);
	DECL_CONST(L, AI_CANONNAME);
#ifdef AI_FQDN
	DECL_CONST(L, AI_FQDN);
#endif
	DECL_CONST(L, AI_NUMERICHOST);
	DECL_CONST(L, AI_NUMERICSERV);
	DECL_CONST(L, AI_PASSIVE);

	lua_pushcfunction(L, lua_getaddrinfo);
	lua_setfield(L, -2, "getaddrinfo");
	lua_pushcfunction(L, lua_freeaddrinfo);
	lua_setfield(L, -2, "freeaddrinfo");
	lua_pushcfunction(L, lua_addrinfo);
	lua_setfield(L, -2, "addrinfo");

	return (1);
}

int
lua_getaddrinfo(lua_State *L)
{
	const char	*host = NULL, *serv = NULL;
	struct addrinfo	*hints, *ai0;
	int		 ret;

	lua_settop(L, 3);
	if (!lua_isnil(L, 1))
		host = luaL_checkstring(L, 1);
	if (!lua_isnil(L, 2))
		serv = luaL_checkstring(L, 2);
	hints = luacs_checkobject(L, 3, "addrinfo");

	if ((ret = getaddrinfo(host, serv, hints, &ai0)))
		luaL_error(L, "%s", gai_strerror(ret));

	luacs_newobject(L, "addrinfo", ai0);

	return (1);
}

int
lua_freeaddrinfo(lua_State *L)
{
	struct addrinfo	*ai;

	lua_settop(L, 1);
	ai = luacs_checkobject(L, 1, "addrinfo");
	freeaddrinfo(ai);

	return (0);
}

int
lua_addrinfo(lua_State *L)
{
	luacs_newobject(L, "addrinfo", NULL);

	return (1);
}

int
lua_in_addr__tostring(lua_State *L)
{
	struct in_addr	*ina;
	char		 buf[128];

	lua_settop(L, 1);
	ina = luacs_checkobject(L, 1, "in_addr");
	if (inet_ntop(AF_INET, ina, buf, sizeof(buf)) == NULL)
		luaL_error(L, "failed to convert a presentation format");
	lua_pushstring(L, buf);

	return (1);
}

int
lua_in6_addr__tostring(lua_State *L)
{
	struct in6_addr	*in6a;
	char		 buf[128];

	lua_settop(L, 1);
	in6a = luacs_checkobject(L, 1, "in6_addr");
	if (inet_ntop(AF_INET6, in6a, buf, sizeof(buf)) == NULL)
		luaL_error(L, "failed to convert a presentation format");
	lua_pushstring(L, buf);

	return (1);
}
