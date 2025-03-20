/*
 * Copyright (c) 2021 YASUOKA Masahiko <yasuoka@yasuoka.net>
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
#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <errno.h>
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "luacstruct.h"

/*
 * When this is used by multiple modules, meta names in Lua will be conflicted,
 * which might cause problems.  Define LUACS_VARIANT uniquely per a module to
 * avoid the problems.
 */
#ifndef LUACS_VARIANT
#define LUACS_VARIANT		"3"
#endif

#define	METANAMELEN		80
#define	METANAME_LUACSTRUCT	"luacstruct" LUACS_VARIANT
#define	METANAME_LUACSENUM	"luacenum" LUACS_VARIANT
#define	METANAME_LUACTYPE	"luactype" LUACS_VARIANT "."
#define	METANAME_LUACARRAY	"luacarray" LUACS_VARIANT
#define	METANAME_LUACARRAYTYPE	"luacarraytype" LUACS_VARIANT
#define	METANAME_LUACSTRUCTOBJ	"luacstructobj" LUACS_VARIANT
#define	METANAME_LUACSENUMVAL	"luacenumval" LUACS_VARIANT
#define	METANAME_LUACSUSERTABLE	"luacusertable" LUACS_VARIANT

#define	LUACS_REGISTRY_NAME	"luacstruct_registry"

#if LUA_VERSION_NUM == 501
#define	lua_rawlen(_x, _i)	lua_objlen((_x), (_i))
#define lua_absindex(_L, _i)	(((_i) > 0 || (_i) <= LUA_REGISTRYINDEX) \
				    ? (_i) : lua_gettop(_L) + (_i) + 1)
#endif

#ifdef LUACS_DEBUG
#define LUACS_DBG(...)				\
	do {					\
		fprintf(stdout, __VA_ARGS__);	\
		fputs("\n", stdout);		\
	} while (0/*CONSTCOND*/)
#define LUACS_ASSERT(_L, _cond)						\
	do {								\
		if (!(_cond)) {						\
			lua_pushfstring((_L), "ASSERT(%s) failed "	\
			    "in %s() at %s:%d", #_cond, __func__,	\
			    __FILE__, __LINE__);			\
			lua_error((_L));				\
		}							\
	} while (0/*CONSTCOND*/)
#else
#define LUACS_DBG(...)		((void)0)
#define LUACS_ASSERT(_L, _cond)	((void)0)
#endif

#ifndef MINIMUM
#define MINIMUM(_a, _b)		((_a) < (_b)? (_a) : (_b))
#endif
#ifndef MAXIMUM
#define MAXIMUM(_a, _b)		((_a) > (_b)? (_a) : (_b))
#endif

#ifndef	nitems
#define	nitems(_x)		(sizeof(_x) / sizeof((_x)[0]))
#endif

#ifndef __unused
#define __unused       __attribute__((__unused__))
#endif

#ifdef _WIN32
#if BYTE_ORDER == LITTLE_ENDIAN
#define htobe16(x)	_byteswap_ushort(x)
#define htole16(x)	(x)
#define htobe32(x)	_byteswap_ulong(x)
#define htole32(x)	(x)
#define htobe64(x)	_byteswap_uint64(x)
#define htole64(x)	(x)
#define be16toh(x)	_byteswap_ushort(x)
#define le16toh(x)	(x)
#define be32toh(x)	_byteswap_ulong(x)
#define le32toh(x)	(x)
#define be64toh(x)	_byteswap_uint64(x)
#define le64toh(x)	(x)
#else
#define htobe16(x)	(x)
#define htole16(x)	_byteswap_ushort(x)
#define htobe32(x)	(x)
#define htole32(x)	_byteswap_ulong(x)
#define htobe64(x)	(x)
#define htole64(x)	_byteswap_uint64(x)
#define be16toh(x)	(x)
#define le16toh(x)	_byteswap_ushort(x)
#define be32toh(x)	(x)
#define le32toh(x)	_byteswap_ulong(x)
#define be64toh(x)	(x)
#define le64toh(x)	_byteswap_uint64(x)
#endif
#endif

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe16(x)	OSSwapHostToBigInt16(x)
#define htole16(x)	OSSwapHostToLittleInt16(x)
#define be16toh(x)	OSSwapBigToHostInt16(x)
#define le16toh(x)	OSSwapLittleToHostInt16(x)
#define htobe32(x)	OSSwapHostToBigInt32(x)
#define htole32(x)	OSSwapHostToLittleInt32(x)
#define be32toh(x)	OSSwapBigToHostInt32(x)
#define le32toh(x)	OSSwapLittleToHostInt32(x)
#define htobe64(x)	OSSwapHostToBigInt64(x)
#define htole64(x)	OSSwapHostToLittleInt64(x)
#define be64toh(x)	OSSwapBigToHostInt64(x)
#define le64toh(x)	OSSwapLittleToHostInt64(x)
#endif

struct luacstruct {
	const char			*typename;
	char				 metaname[METANAMELEN];
	SPLAY_HEAD(luacstruct_fields, luacstruct_field)
					 fields;
	TAILQ_HEAD(,luacstruct_field)	 sorted;
};

struct luacarraytype {
	const char			*typename;
	char				 metaname[METANAMELEN];
	enum luacstruct_type		 type;
	size_t				 size;
	int				 nmemb;
	int				 typref;
	unsigned			 flags;
};

struct luacregeon {
	enum luacstruct_type		 type;
	int				 off;
	size_t				 size;
	int				 typref;
	unsigned			 flags;
};

struct luacstruct_field {
	enum luacstruct_type		 type;
	const char			*fieldname;
	struct luacregeon		 regeon;
	int				 constval;
	int				 nmemb;
	unsigned			 flags;
	int				 ref;
	SPLAY_ENTRY(luacstruct_field)	 tree;
	TAILQ_ENTRY(luacstruct_field)	 queue;
};

struct luacobject {
	enum luacstruct_type		 type;
	struct luacstruct		*cs;
	caddr_t				 ptr;
	size_t				 size;
	int				 nmemb;
	int				 typref;
	unsigned			 flags;
};

struct luacenum {
	const char			*enumname;
	char				 metaname[METANAMELEN];
	size_t				 valwidth;
	SPLAY_HEAD(luacenum_labels, luacenum_value)
					 labels;
	SPLAY_HEAD(luacenum_values, luacenum_value)
					 values;
	int				 func_get;
	int				 func_memberof;
};

struct luacenum_value {
	struct luacenum			*ce;
	intmax_t			 value;
	int				 ref;
	const char			*label;
	SPLAY_ENTRY(luacenum_value)	 treel;
	SPLAY_ENTRY(luacenum_value)	 treev;
};

static struct luacstruct
		*luacs_checkstruct(lua_State *, int);
static int	 luacs_usertable(lua_State *, int);
static int	 luacs_struct__gc(lua_State *);
static struct luacstruct_field
		*luacs_declare(lua_State *, enum luacstruct_type, const char *,
		    const char *, size_t, int, int, unsigned);
static int	 luacstruct_field_cmp(struct luacstruct_field *,
		    struct luacstruct_field *);
static struct luacstruct_field
		*luacsfield_copy(lua_State *, struct luacstruct_field *);
static void	 luacstruct_field_free(lua_State *, struct luacstruct *,
		    struct luacstruct_field *);
static int	 luacs_pushctype(lua_State *, enum luacstruct_type,
		    const char *);
static int	 luacs_newarray0(lua_State *, enum luacstruct_type, int, size_t,
		    int, unsigned, void *);
static int	 luacs_array__len(lua_State *);
static int	 luacs_array__index(lua_State *);
static int	 luacs_array__newindex(lua_State *);
static int	 luacs_array_copy(lua_State *);
static int	 luacs_array__next(lua_State *);
static int	 luacs_array__pairs(lua_State *);
static int	 luacs_array__gc(lua_State *);
static int	 luacs_newobject0(lua_State *, void *);
static int	 luacs_object__luacstructdump(struct lua_State *);
struct luacobj_compat;
static void	 luacs_object_compat(lua_State *, int, struct luacobj_compat *);
static int	 luacs_object__eq(lua_State *);
static int	 luacs_object__tostring(lua_State *);
static int	 luacs_object__index(lua_State *);
static int	 luacs_object__get(lua_State *, struct luacobject *,
		    struct luacstruct_field *);
static int	 luacs_object__newindex(lua_State *);
static int	 luacs_object_copy(lua_State *);
static int	 luacs_object__next(lua_State *);
static int	 luacs_object__pairs(lua_State *);
static int	 luacs_object__gc(lua_State *);
static int	 luacs_pushregeon(lua_State *, struct luacobject *,
		    struct luacregeon *);
static void	 luacs_pullregeon(lua_State *, struct luacobject *,
		    struct luacregeon *, int);
struct luacenum	*luacs_checkenum(lua_State*, int);
struct luacenum_value
		*luacs_enum_get0(struct luacenum *, intmax_t);
static int	 luacs_enum_get(lua_State *);
static int	 luacs_enum_memberof(lua_State *);
static int	 luacs_enum__index(lua_State *);
static int	 luacs_enum__pairs(lua_State *);
static int	 luacs_enum__next(lua_State *);
static int	 luacs_enum__gc(lua_State *);
static int	 luacs_enumvalue_tointeger(lua_State *);
static int	 luacs_enumvalue_label(lua_State *);
static int	 luacs_enumvalue__tostring(lua_State *);
static int	 luacs_enumvalue__gc(lua_State *);
static int	 luacs_enumvalue__eq(lua_State *);
static int	 luacs_enumvalue__lt(lua_State *);
static int	 luacenum_label_cmp(struct luacenum_value *,
		    struct luacenum_value *);
static int	 luacenum_value_cmp(struct luacenum_value *,
		    struct luacenum_value *);
static int	 luacs_ref(lua_State *);
static int	 luacs_getref(lua_State *, int);
static int	 luacs_unref(lua_State *, int);
static int	 luacs_pushwstring(lua_State *, const wchar_t *);

SPLAY_PROTOTYPE(luacstruct_fields, luacstruct_field, tree,
    luacstruct_field_cmp);
SPLAY_PROTOTYPE(luacenum_labels, luacenum_value, treel, luacenum_label_cmp);
SPLAY_PROTOTYPE(luacenum_values, luacenum_value, treev, luacenum_value_cmp);


/* struct */
int
luacs_newstruct0(lua_State *L, const char *tname, const char *supertname)
{
	int			 ret;
	struct luacstruct	*cs, *supercs = NULL;
	char			 metaname[METANAMELEN], buf[128];
	struct luacstruct_field	*fieldf, *fieldt;

	if (supertname != NULL) {
		snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE,
		    supertname);
		lua_getfield(L, LUA_REGISTRYINDEX, metaname);
		if (lua_isnil(L, -1)) {
			snprintf(buf, sizeof(buf), "`%s' is not regisiterd",
			    supertname);
			lua_pushstring(L, buf);
			lua_error(L);
			return (0);	/* not reached */
		}
		supercs = luacs_checkstruct(L, -1);
	}

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, tname);
	lua_getfield(L, LUA_REGISTRYINDEX, metaname);
	if (!lua_isnil(L, -1)) {
		cs = luacs_checkstruct(L, -1);
		return (1);
	}
	lua_pop(L, 1);

	cs = lua_newuserdata(L, sizeof(struct luacstruct));
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, metaname);
	memcpy(cs->metaname, metaname, MINIMUM(sizeof(metaname),
	    sizeof(cs->metaname)));

	cs->typename = strchr(cs->metaname, '.') + 1;
	SPLAY_INIT(&cs->fields);
	TAILQ_INIT(&cs->sorted);

	/* Inherit from super struct if specified */
	if (supercs != NULL) {
		TAILQ_FOREACH(fieldf, &supercs->sorted, queue) {
			if ((fieldt = luacsfield_copy(L, fieldf)) == NULL) {
				strerror_r(errno, buf, sizeof(buf));
				lua_pushstring(L, buf);
				lua_error(L);
				return (0);	/* not reached */
			}
			TAILQ_INSERT_TAIL(&cs->sorted, fieldt, queue);
			SPLAY_INSERT(luacstruct_fields, &cs->fields, fieldt);
		}
	}

	if ((ret = luaL_newmetatable(L, METANAME_LUACSTRUCT)) != 0) {
		lua_pushcfunction(L, luacs_struct__gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	return (1);
}

int
luacs_delstruct(lua_State *L, const char *tname)
{
	char			 metaname[METANAMELEN];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, tname);
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, metaname);

	return (0);
}

struct luacstruct *
luacs_checkstruct(lua_State *L, int csidx)
{
	return (luaL_checkudata(L, csidx, METANAME_LUACSTRUCT));
}

int
luacs_usertable(lua_State *L, int idx)
{
	int	 absidx;

	absidx = lua_absindex(L, idx);

	lua_getfield(L, LUA_REGISTRYINDEX, METANAME_LUACSUSERTABLE);
	if (lua_isnil(L, -1)) {
		/* Create a weak table */
		lua_pop(L, 1);
		lua_newtable(L);
		lua_newtable(L);
		lua_pushstring(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
		lua_setfield(L, LUA_REGISTRYINDEX, METANAME_LUACSUSERTABLE);
		lua_getfield(L, LUA_REGISTRYINDEX, METANAME_LUACSUSERTABLE);
	}
	lua_pushvalue(L, absidx);
	lua_gettable(L, -2);
	if (lua_isnil(L, -1)) {
		/* Create a new table which is weakly refered by the userdata */
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, absidx);
		lua_pushvalue(L, -2);
		lua_settable(L, -4);
	}
	lua_remove(L, -2);

	return (1);
}

int
luacs_struct__gc(lua_State *L)
{
	struct luacstruct	*cs;
	struct luacstruct_field	*field;

	lua_settop(L, 1);
	cs = luacs_checkstruct(L, 1);
	if (cs) {
		while ((field = SPLAY_MIN(luacstruct_fields, &cs->fields)) !=
		    NULL)
			luacstruct_field_free(L, cs, field);
	}

	return (0);
}

int
luacs_declare_field(lua_State *L, enum luacstruct_type _type,
    const char *tname, const char *name, size_t siz, int off, int nmemb,
    unsigned flags)
{
	luacs_declare(L, _type, tname, name, siz, off, nmemb, flags);
	return (0);
}

struct luacstruct_field	*
luacs_declare(lua_State *L, enum luacstruct_type _type,
    const char *tname, const char *name, size_t siz, int off, int nmemb,
    unsigned flags)
{
	struct luacstruct_field	*field, *field0;
	struct luacstruct	*cs;
	char			 buf[BUFSIZ];

	cs = luacs_checkstruct(L, -1);
	if ((field = calloc(1, sizeof(struct luacstruct_field))) == NULL) {
		strerror_r(errno, buf, sizeof(buf));
		lua_pushstring(L, buf);
		lua_error(L);
	}
	if ((field->fieldname = strdup(name)) == NULL) {
		free(field);
		strerror_r(errno, buf, sizeof(buf));
		lua_pushstring(L, buf);
		lua_error(L);
	}
	while ((field0 = SPLAY_FIND(luacstruct_fields, &cs->fields, field))
	    != NULL)
		luacstruct_field_free(L, cs, field0);
	field->regeon.type = _type;
	field->regeon.off = off;
	field->regeon.size = siz;
	field->regeon.flags = flags;
	field->nmemb = nmemb;
	field->flags = flags;
	switch (_type) {
	case LUACS_TOBJREF:
	case LUACS_TOBJENT:
	case LUACS_TENUM:
	case LUACS_TARRAY:
		luacs_pushctype(L, _type, tname);
		field->regeon.typref = luacs_ref(L);
		break;
	case LUACS_TINT64:
	case LUACS_TUINT64:
		if (sizeof(lua_Integer) < 8) {
			lua_pushliteral(L,
			    "Lua runtime doesn't support 64bit integer");
			lua_error(L);
		}
	default:
		break;
	}
	field->type = (field->nmemb > 0)? LUACS_TARRAY : _type;

	SPLAY_INSERT(luacstruct_fields, &cs->fields, field);
	TAILQ_FOREACH(field0, &cs->sorted, queue) {
		if (field->regeon.off < field0->regeon.off)
			break;
	}
	if (field0 == NULL)
		TAILQ_INSERT_TAIL(&cs->sorted, field, queue);
	else
		TAILQ_INSERT_BEFORE(field0, field, queue);

	return (field);
}

int
luacs_declare_method(lua_State *L, const char *name, int (*func)(lua_State *))
{
	struct luacstruct_field	 *field;

	field = luacs_declare(L, LUACS_TMETHOD, NULL, name, 0, 0, 0,
	    LUACS_FREADONLY);
	lua_pushcfunction(L, func);
	field->ref = luacs_ref(L);

	return (0);
}

int
luacs_declare_const(lua_State *L, const char *name, int constval)
{
	struct luacstruct_field	 *field;

	field = luacs_declare(L, LUACS_TCONST, NULL, name, 0, 0, 0,
	    LUACS_FREADONLY);
	field->constval = constval;

	return (0);
}

int
luacstruct_field_cmp(struct luacstruct_field *a, struct luacstruct_field *b)
{
	return (strcmp(a->fieldname, b->fieldname));
}

struct luacstruct_field *
luacsfield_copy(lua_State *L, struct luacstruct_field *from)
{
	struct luacstruct_field	*to;

	to = calloc(1, sizeof(struct luacstruct_field));
	if (to == NULL)
		return (NULL);
	if ((to->fieldname = strdup(from->fieldname)) == NULL) {
		free(to);
		return (NULL);
	}
	to->type = from->type;
	to->regeon = from->regeon;
	to->constval = from->constval;
	to->nmemb = from->nmemb;
	to->flags = from->flags;
	/* Update refs */
	if (from->regeon.typref != 0) {
		luacs_getref(L, from->regeon.typref);
		to->regeon.typref = luacs_ref(L);
	}
	if (from->ref != 0) {
		luacs_getref(L, from->ref);
		to->ref = luacs_ref(L);
	}

	return (to);
}

void
luacstruct_field_free(lua_State *L, struct luacstruct *cs,
    struct luacstruct_field *field)
{
	if (field) {
		if (field->regeon.typref > 0)
			luacs_unref(L, field->regeon.typref);
		if (field->ref != 0)
			luacs_unref(L, field->ref);
		TAILQ_REMOVE(&cs->sorted, field, queue);
		SPLAY_REMOVE(luacstruct_fields, &cs->fields, field);
		free((char *)field->fieldname);
	}
	free(field);
}

int
luacs_pushctype(lua_State *L, enum luacstruct_type _type, const char *tname)
{
	char	metaname[METANAMELEN];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, tname);
	lua_getfield(L, LUA_REGISTRYINDEX, metaname);
	if (lua_isnil(L, -1)) {
		if (_type == LUACS_TARRAY)
			lua_pushfstring(L, "array `%s' is not registered",
			    tname);
		else
			lua_pushfstring(L, "`%s %s' is not registered",
			    _type == LUACS_TENUM? "enum" : "struct", tname);
		lua_error(L);
	}
	if (_type == LUACS_TARRAY)
		luaL_checkudata(L, -1, METANAME_LUACARRAYTYPE);
	else if (_type == LUACS_TENUM)
		luacs_checkenum(L, -1);
	else
		luacs_checkstruct(L, -1);

	return (1);
}

/* array */
int
luacs_arraytype__gc(lua_State *L)
{
	luaL_checkudata(L, -1, METANAME_LUACARRAYTYPE);

	return (0);
}

int
luacs_newarraytype(lua_State *L, const char *tname, enum luacstruct_type _type,
    const char *membtname, size_t size, int nmemb, unsigned flags)
{
	int			 ret;
	struct luacarraytype	*cat;
	char			 metaname[METANAMELEN];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, tname);
	lua_getfield(L, LUA_REGISTRYINDEX, metaname);
	if (!lua_isnil(L, -1)) {
		cat = luaL_checkudata(L, -1, METANAME_LUACARRAYTYPE);
		return (1);
	}
	lua_pop(L, 1);

	cat = lua_newuserdata(L, sizeof(struct luacarraytype));

	cat->type = _type;
	cat->size = size;
	cat->nmemb = nmemb;
	cat->flags = flags;

	switch (_type) {
	case LUACS_TOBJREF:
	case LUACS_TOBJENT:
	case LUACS_TARRAY:
		if (membtname == NULL) {
			lua_pushfstring(L, "`membtname' argument must be "
			    "specified when creating an array of %s",
			    _type == LUACS_TENUM?  "LUACS_TENUM" :
			    _type == LUACS_TOBJREF ?  "LUACS_TOBJREF" :
			    "LUACS_TOBJENT");
			lua_error(L);
		}
		luacs_pushctype(L, _type, membtname);
		cat->typref = luacs_ref(L);
		break;
	default:
		break;
	}

	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, metaname);
	memcpy(cat->metaname, metaname, MINIMUM(sizeof(metaname),
	    sizeof(cat->metaname)));

	cat->typename = strchr(cat->metaname, '.') + 1;
	if ((ret = luaL_newmetatable(L, METANAME_LUACARRAYTYPE)) != 0) {
		lua_pushcfunction(L, luacs_arraytype__gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	return (1);
}

int
luacs_newarray(lua_State *L, enum luacstruct_type _type, const char *membtname,
    size_t size, int nmemb, unsigned flags, void *ptr)
{
	int	typref = 0;

	switch (_type) {
	case LUACS_TENUM:
	case LUACS_TOBJREF:
	case LUACS_TOBJENT:
	case LUACS_TARRAY:
		if (membtname == NULL) {
			lua_pushfstring(L, "`membtname' argument must be "
			    "specified when creating an array of %s",
			    _type == LUACS_TENUM?  "LUACS_TENUM" :
			    _type == LUACS_TOBJREF ?  "LUACS_TOBJREF" :
			    "LUACS_TOBJENT");
			lua_error(L);
		}
		luacs_pushctype(L, _type, membtname);
		typref = luacs_ref(L);
		break;
	default:
		break;
	}
	return (luacs_newarray0(L, _type, typref, size, nmemb, flags, ptr));
}

int
luacs_newarray0(lua_State *L, enum luacstruct_type _type, int typidx,
    size_t size, int nmemb, unsigned flags, void *ptr)
{
	struct luacobject	*obj;
	int			 ret, absidx;

	absidx = lua_absindex(L, typidx);

	if (ptr != NULL) {
		obj = lua_newuserdata(L, sizeof(struct luacobject));
		obj->ptr = ptr;
	} else {
		obj = lua_newuserdata(L, sizeof(struct luacobject) +
		    size * nmemb);
		obj->ptr = (caddr_t)(obj + 1);
	}
	obj->type =_type;
	obj->size = size;
	obj->nmemb = nmemb;
	obj->flags = flags;

	if (typidx != 0) {
		lua_pushvalue(L, absidx);
		obj->typref = luacs_ref(L);
	}

	if ((ret = luaL_newmetatable(L, METANAME_LUACARRAY)) != 0) {
		lua_pushcfunction(L, luacs_array__len);
		lua_setfield(L, -2, "__len");
		lua_pushcfunction(L, luacs_array__index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, luacs_array__newindex);
		lua_setfield(L, -2, "__newindex");
		lua_pushcfunction(L, luacs_array__next);
		lua_pushcclosure(L, luacs_array__pairs, 1);
		lua_setfield(L, -2, "__pairs");
		lua_pushcfunction(L, luacs_array__gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);

	return (1);
}

int
luacs_array__len(lua_State *L)
{
	struct luacobject	*obj;

	lua_settop(L, 1);
	obj = luaL_checkudata(L, 1, METANAME_LUACARRAY);
	lua_pushinteger(L, obj->nmemb);

	return (1);
}

int
luacs_array__index(lua_State *L)
{
	struct luacobject	*obj;
	struct luacarraytype	*cat;
	int			 idx;
	struct luacregeon	 regeon;
	void			*ptr;

	lua_settop(L, 2);
	obj = luaL_checkudata(L, 1, METANAME_LUACARRAY);
	idx = luaL_checkinteger(L, 2);
	if (idx < 1 || obj->nmemb < idx) {
		lua_pushnil(L);
		return (1);
	}
	memset(&regeon, 0, sizeof(regeon));

	regeon.type = obj->type;
	regeon.off = (idx - 1) * obj->size;
	regeon.size = obj->size;
	regeon.typref = obj->typref;

	switch (obj->type)  {
	default:
		return (luacs_pushregeon(L, obj, &regeon));
		break;
	case LUACS_TOBJREF:
	case LUACS_TOBJENT:
		if (obj->type == LUACS_TOBJENT)
			ptr = obj->ptr + regeon.off;
		else
			ptr = *(void **)(obj->ptr + regeon.off);
		if (ptr == NULL)
			lua_pushnil(L);
		else {
			luacs_usertable(L, 1);
			lua_rawgeti(L, -1, idx);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				luacs_getref(L, obj->typref);
				luacs_newobject0(L, ptr);
				lua_pushvalue(L, -1);
				lua_rawseti(L, -4, idx);
				lua_remove(L, -2);
			}
			lua_remove(L, -2);
		}
		break;
	case LUACS_TEXTREF:
		luacs_usertable(L, 1);
		lua_rawgeti(L, -1, idx);
		lua_remove(L, -2);
		break;
	case LUACS_TARRAY:
		ptr = obj->ptr + regeon.off;
		if (ptr == NULL)
			lua_pushnil(L);
		else {
			luacs_usertable(L, 1);
			lua_rawgeti(L, -1, idx);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				luacs_getref(L, obj->typref);
				cat = luaL_checkudata(L, -1,
				    METANAME_LUACARRAYTYPE);
				lua_pop(L, 1);
				if (cat->typref != 0)
					luacs_getref(L, cat->typref);
				luacs_newarray0(L, cat->type, cat->typref,
				    (cat->typref != 0)? -1 : 0, cat->nmemb,
				    cat->flags, obj->ptr + regeon.off);
				if (cat->typref != 0)
					lua_remove(L, -2);
				lua_pushvalue(L, -1);
				lua_rawseti(L, -3, idx);
			}
			lua_remove(L, -2);
		}
		break;
	}

	return (1);
}

int
luacs_array__newindex(lua_State *L)
{
	struct luacobject	*obj, *ano = NULL;
	int			 idx;
	struct luacregeon	 regeon;
	struct luacstruct	*cs0;

	lua_settop(L, 3);
	obj = luaL_checkudata(L, 1, METANAME_LUACARRAY);
	idx = luaL_checkinteger(L, 2);
	if (idx < 1 || obj->nmemb < idx) { /* out of the range */
		lua_pushfstring(L, "array index %d out of the range 1:%d",
		    idx, obj->nmemb);
		lua_error(L);
	}
	regeon.type = obj->type;
	regeon.off = (idx - 1) * obj->size;
	regeon.size = obj->size;
	regeon.typref = obj->typref;
	regeon.flags = obj->flags;

	if ((obj->flags & LUACS_FREADONLY) != 0) {
readonly:
		lua_pushliteral(L, "array is readonly");
		lua_error(L);
	}
	switch (regeon.type) {
	default:
		luacs_pullregeon(L, obj, &regeon, 3);
		break;
	case LUACS_TSTRPTR:
	case LUACS_TWSTRPTR:
		goto readonly;
	case LUACS_TOBJREF:
	case LUACS_TOBJENT:
		/* get c struct of the field */
		luacs_getref(L, regeon.typref);
		cs0 = luacs_checkstruct(L, -1);
		lua_pop(L, 1);
		/* given instance of struct */
		if (regeon.type == LUACS_TOBJENT || !lua_isnil(L, 3))
			ano = luaL_checkudata(L, 3, METANAME_LUACSTRUCTOBJ);
		if (ano != NULL && cs0 != ano->cs) {
			lua_pushfstring(L,
			    "must be an instance of `struct %s'",
			    cs0->typename);
			lua_error(L);
		}
		if (regeon.type == LUACS_TOBJENT) {
			lua_pushcfunction(L, luacs_object_copy);

			/* ca't assume the object is cached */
			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 1);
			lua_pushinteger(L, 2);
			lua_call(L, 2, 1);

			lua_pushvalue(L, 3);
			lua_call(L, 2, 0);
		} else {
			*(void **)(obj->ptr + regeon.off) = ano? ano->ptr :
			    NULL;
			/* use the same object */
			luacs_usertable(L, 1);
			lua_pushvalue(L, 3);
			lua_rawseti(L, -2, idx);
			lua_pop(L, 1);
		}
		break;
	case LUACS_TEXTREF:
		luacs_usertable(L, 1);
		lua_pushvalue(L, 3);
		lua_rawseti(L, -2, idx);
		lua_pop(L, 1);
		break;
	case LUACS_TARRAY:
		lua_pushcfunction(L, luacs_array_copy);

		lua_pushcfunction(L, luacs_array__index);
		lua_pushvalue(L, 1);
		lua_pushinteger(L, 2);
		lua_call(L, 2, 1);

		lua_pushvalue(L, 3);
		lua_call(L, 2, 0);
		break;
	}

	return (0);
}

int
luacs_array_copy(lua_State *L)
{
	struct luacobject	*l, *r;
	struct luacregeon	 regeon;
	int			 idx;

	lua_settop(L, 2);
	l = luaL_checkudata(L, 1, METANAME_LUACARRAY);
	r = luaL_checkudata(L, 2, METANAME_LUACARRAY);

	if (l->type != r->type) {
		lua_pushliteral(L,
		    "can't copy between arrays of a different type");
		lua_error(L);
	}
	if (l->nmemb != r->nmemb) {
		lua_pushliteral(L,
		    "can't copy between arrays which size are different");
		lua_error(L);
	}
	switch (l->type) {
	default:
		for (idx = 1; idx <= l->nmemb; idx++) {
			regeon.type = l->type;
			regeon.off = (idx - 1) * l->size;
			regeon.size = l->size;
			regeon.typref = l->typref;

			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 2);
			lua_pushinteger(L, idx);
			lua_call(L, 2, 1);

			luacs_pullregeon(L, l, &regeon, -1);
			lua_pop(L, 1);
		}
		break;
	case LUACS_TOBJREF:
		luacs_usertable(L, 1);
		for (idx = 1; idx <= l->nmemb; idx++) {
			/* use the same pointer */
			*(void **)(l->ptr + (idx - 1) * l->size) =
			    *(void **)(r->ptr + (idx - 1) * r->size);

			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 2);
			lua_pushinteger(L, idx);
			lua_call(L, 2, 1);

			lua_rawseti(L, -2, idx);
		}
		lua_pop(L, 1);
		break;
	case LUACS_TOBJENT:
		for (idx = 1; idx <= l->nmemb; idx++) {
			lua_pushcfunction(L, luacs_object_copy);

			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 1);
			lua_pushinteger(L, idx);
			lua_call(L, 2, 1);

			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 2);
			lua_pushinteger(L, idx);
			lua_call(L, 2, 1);

			lua_call(L, 2, 0);
		}
		break;
	case LUACS_TEXTREF:
		luacs_usertable(L, 1);
		for (idx = 1; idx <= l->nmemb; idx++) {
			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 2);
			lua_pushinteger(L, idx);
			lua_call(L, 2, 1);

			lua_rawseti(L, -2, idx);
		}
		lua_pop(L, 1);
		break;
	case LUACS_TARRAY:
		for (idx = 1; idx <= l->nmemb; idx++) {
			lua_pushcfunction(L, luacs_array_copy);

			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 1);
			lua_pushinteger(L, idx);
			lua_call(L, 2, 1);

			lua_pushcfunction(L, luacs_array__index);
			lua_pushvalue(L, 2);
			lua_pushinteger(L, 2);
			lua_call(L, 2, 1);

			lua_call(L, 2, 0);
		}
		break;
	}

	return (0);
}

int
luacs_array__next(lua_State *L)
{
	struct luacobject	*obj;
	int			 idx = 0;

	lua_settop(L, 2);
	obj = luaL_checkudata(L, 1, METANAME_LUACARRAY);
	if (!lua_isnil(L, 2))
		idx = luaL_checkinteger(L, 2);

	idx++;
	if (idx < 1 || obj->nmemb < idx) {
		lua_pushnil(L);
		return (1);
	}
	lua_pushinteger(L, idx);
	lua_pushcfunction(L, luacs_array__index);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, idx);
	lua_call(L, 2, 1);
	return (2);
}

int
luacs_array__pairs(lua_State *L)
{
	lua_settop(L, 1);
	luaL_checkudata(L, 1, METANAME_LUACARRAY);

	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 1);
	lua_pushnil(L);

	return (3);
}

int
luacs_array__gc(lua_State *L)
{
	struct luacobject	*obj;

	lua_settop(L, 1);
	obj = luaL_checkudata(L, 1, METANAME_LUACARRAY);
	if (obj->typref != 0)
		luacs_unref(L, obj->typref);

	return (0);
}

/* object */
int
luacs_newobject(lua_State *L, const char *tname, void *ptr)
{
	int			 ret;
	char			 metaname[METANAMELEN];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, tname);
	lua_getfield(L, LUA_REGISTRYINDEX, metaname);

	ret = luacs_newobject0(L, ptr);
	lua_remove(L, -2);

	return (ret);
}

int
luacs_newobject0(lua_State *L, void *ptr)
{
	struct luacobject	*obj;
	struct luacstruct	*cs;
	struct luacstruct_field	*field;
	int			 ret;
	size_t			 objsiz = 0;

	cs = luacs_checkstruct(L, -1);
	if (ptr != NULL) {
		obj = lua_newuserdata(L, sizeof(struct luacobject));
		obj->ptr = ptr;
	} else {
		TAILQ_FOREACH(field, &cs->sorted, queue) {
			objsiz = MAXIMUM(objsiz, field->regeon.off +
			    (field->nmemb == 0? 1 : field->nmemb) *
			    field->regeon.size);
		}
		obj = lua_newuserdata(L, sizeof(struct luacobject) + objsiz);
		obj->ptr = (caddr_t)(obj + 1);
	}
	obj->cs = cs;
	lua_pushvalue(L, -2);
	obj->typref = luacs_ref(L);
	if ((ret = luaL_newmetatable(L, METANAME_LUACSTRUCTOBJ)) != 0) {
		lua_pushcfunction(L, luacs_object__index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, luacs_object__newindex);
		lua_setfield(L, -2, "__newindex");
		lua_pushcfunction(L, luacs_object__next);
		lua_pushcclosure(L, luacs_object__pairs, 1);
		lua_setfield(L, -2, "__pairs");
		lua_pushcfunction(L, luacs_object__gc);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, luacs_object__tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, luacs_object__eq);
		lua_setfield(L, -2, "__eq");
		lua_pushcfunction(L, luacs_object__luacstructdump);
		lua_setfield(L, -2, "__luacstructdump");
	}
	lua_setmetatable(L, -2);

	return (1);
}

int
luacs_object__luacstructdump(struct lua_State *L)
{
	struct luacobject	*obj;

	lua_settop(L, 1);
	obj = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	lua_pushlightuserdata(L, obj->ptr);
	lua_pushstring(L, obj->cs->typename);

	return (2);
}

struct luacobj_compat {
	void		*ptr;
	const char	*typ;
};

void *
luacs_object_pointer(lua_State *L, int ref, const char *typename)
{
	struct luacobj_compat	compat;

	memset(&compat, 0, sizeof(compat));
	luacs_object_compat(L, ref, &compat);
	if (typename == NULL || (compat.typ != NULL && strcmp(compat.typ,
	    typename) == 0))
		return (compat.ptr);

	return (NULL);
}

/*
 * Explicitly clear the weak reference from the internal user table.  In Lua
 * 5.2 and later, no need to use this function since the reference is
 * automatically calculated.  However, it doesn't work in Lua 5.1 when the
 * pseudo value references the parent object.  To avoid this problem,
 * call this function when the object is no longer needed.
 */
void
luacs_object_clear(lua_State *L, int idx)
{
	int	 absidx;

	absidx = lua_absindex(L, idx);
	lua_getfield(L, LUA_REGISTRYINDEX, METANAME_LUACSUSERTABLE);
	if (lua_isnil(L, -1))
		return;
	lua_pushvalue(L, absidx);
	lua_newtable(L);
	lua_settable(L, -3);
	lua_pop(L, 1);
}

void
luacs_object_compat(lua_State *L, int ref, struct luacobj_compat *compat)
{
	lua_pushvalue(L, ref);
	if (lua_getmetatable(L, -1)) {
		lua_getfield(L, -1, "__luacstructdump");
		if (!lua_isnil(L, -1)) {
			lua_pushvalue(L, -3);
			lua_call(L, 1, 2);
			compat->typ = lua_tostring(L, -1);
			compat->ptr = lua_touserdata(L, -2);
			lua_pop(L, 2);
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

int
luacs_object__eq(lua_State *L)
{
	struct luacobject	*obja, *objb;
	void			*ptra, *ptrb;

	lua_settop(L, 2);

	ptra = luacs_object_pointer(L, 1, NULL);
	ptrb = luacs_object_pointer(L, 2, NULL);
	if (ptra == NULL || ptrb == NULL) {
		lua_pushboolean(L, false);
		return (1);
	}
	obja = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	objb = luaL_checkudata(L, 2, METANAME_LUACSTRUCTOBJ);
	if (ptra == ptrb) {		/* the same pointer */
		if (obja == objb ||	/* the same type */
		    strcmp(obja->cs->typename, objb->cs->typename) == 0) {
			lua_pushboolean(L, true);
			return (1);
		}
	}
	lua_getfield(L, 1, "__eq");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_pushboolean(L, false);
		return (1);
	}
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_call(L, 2, 1);

	return (1);
}

int
luacs_object__tostring(lua_State *L)
{
	struct luacobject	*obj;
	char			 buf[BUFSIZ];

	lua_settop(L, 1);
	obj = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);

	lua_getfield(L, 1, "__tostring");
	if (!lua_isnil(L, -1)) {
		lua_pushvalue(L, 1);
		lua_call(L, 1, 1);
	} else {
		snprintf(buf, sizeof(buf), "struct %s: %p", obj->cs->typename,
		    obj->ptr);
		lua_pushstring(L, buf);
	}

	return (1);
}

int
luacs_object_typename(lua_State *L)
{
	struct luacobj_compat	 compat;

	memset(&compat, 0, sizeof(compat));
	lua_settop(L, 1);
	luacs_object_compat(L, 1, &compat);
	if (compat.typ != NULL)
		lua_pushstring(L, compat.typ);
	else
		lua_pushnil(L);

	return (1);
}

int
luacs_object__index(lua_State *L)
{
	struct luacobject	*obj;
	struct luacstruct_field	 fkey, *field;

	lua_settop(L, 2);
	obj = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	fkey.fieldname = luaL_checkstring(L, 2);
	if ((field = SPLAY_FIND(luacstruct_fields, &obj->cs->fields, &fkey))
	    != NULL)
		return (luacs_object__get(L, obj, field));
	else
		lua_pushnil(L);
	return (1);
}

int
luacs_object__get(lua_State *L, struct luacobject *obj,
    struct luacstruct_field *field)
{
	void	*ptr;

	switch (field->type) {
	default:
		return (luacs_pushregeon(L, obj, &field->regeon));
	case LUACS_TOBJREF:
	case LUACS_TOBJENT:
		if (field->type == LUACS_TOBJENT)
			ptr = obj->ptr + field->regeon.off;
		else
			ptr = *(void **)(obj->ptr + field->regeon.off);
		if (ptr == NULL)
			lua_pushnil(L);
		else {
			struct luacobject *cache = NULL;

			luacs_usertable(L, 1);
			lua_getfield(L, -1, field->fieldname);
			if (lua_isnil(L, -1))
				lua_pop(L, 1);
			else {	/* has a cache */
				cache = luaL_checkudata(L, -1,
				    METANAME_LUACSTRUCTOBJ);
				/* cached reference may be staled */
				if (field->type == LUACS_TOBJREF &&
				    cache->ptr !=
				    *(void **)(obj->ptr + field->regeon.off)) {
					lua_pop(L, 1);
					lua_pushnil(L);
					lua_setfield(L, -2, field->fieldname);
					cache = NULL;
				}
			}
			if (cache == NULL) {
				luacs_getref(L, field->regeon.typref);
				luacs_newobject0(L, ptr);
				lua_pushvalue(L, -1);
				lua_setfield(L, -4, field->fieldname);
				lua_remove(L, -2);
			}
			lua_remove(L, -2);
		}
		break;
	case LUACS_TEXTREF:
		luacs_usertable(L, 1);
		lua_getfield(L, -1, field->fieldname);
		lua_remove(L, -2);
		break;
	case LUACS_TARRAY:
		/* use the cache if any */
		luacs_usertable(L, 1);
		lua_getfield(L, -1, field->fieldname);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			if (field->regeon.typref != 0)
				luacs_getref(L, field->regeon.typref);
			luacs_newarray0(L, field->regeon.type,
			    (field->regeon.typref != 0)? -1 : 0,
			    field->regeon.size, field->nmemb, field->flags,
			    obj->ptr + field->regeon.off);
			if (field->regeon.typref != 0)
				lua_remove(L, -2);
			lua_pushvalue(L, -1);
			lua_setfield(L, -3, field->fieldname);
		}
		lua_remove(L, -2);
		break;
	case LUACS_TMETHOD:
		luacs_getref(L, field->ref);
		break;
	case LUACS_TCONST:
		lua_pushinteger(L, field->constval);
		break;
	}

	return (1);
}

int
luacs_object__newindex(lua_State *L)
{
	struct luacstruct	*cs0;
	struct luacobject	*obj, *ano = NULL;
	struct luacstruct_field	 fkey, *field;

	lua_settop(L, 3);
	obj = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	fkey.fieldname = luaL_checkstring(L, 2);
	if ((field = SPLAY_FIND(luacstruct_fields, &obj->cs->fields, &fkey))
	    != NULL) {
		if ((field->flags & LUACS_FREADONLY) != 0) {
readonly:
			lua_pushfstring(L, "field `%s' is readonly",
			    field->fieldname);
			lua_error(L);
		}
		switch (field->type) {
		default:
			luacs_pullregeon(L, obj, &field->regeon, 3);
			break;
		case LUACS_TSTRPTR:
		case LUACS_TWSTRPTR:
			goto readonly;
		case LUACS_TOBJREF:
		case LUACS_TOBJENT:
			/* get c struct of the field */
			luacs_getref(L, field->regeon.typref);
			cs0 = luacs_checkstruct(L, -1);
			lua_pop(L, 1);
			if (field->regeon.type == LUACS_TOBJENT ||
			    !lua_isnil(L, 3))
				/* given instance of struct */
				ano = luaL_checkudata(L, 3,
				    METANAME_LUACSTRUCTOBJ);
			/* given instance of struct */
			if (ano != NULL && cs0 != ano->cs) {
				lua_pushfstring(L,
				    "`%s' field must be an instance of "
				    "`struct %s'", field->fieldname,
				    cs0->typename);
				lua_error(L);
			}
			if (field->regeon.type == LUACS_TOBJENT) {
				lua_pushcfunction(L, luacs_object_copy);
				lua_getfield(L, 1, field->fieldname);
				lua_pushvalue(L, 3);
				lua_call(L, 2, 0);
			} else {
				*(void **)(obj->ptr + field->regeon.off) =
				    ano != NULL? ano->ptr : NULL;
				/* use the same object */
				luacs_usertable(L, 1);
				lua_pushvalue(L, 3);
				lua_setfield(L, -2, field->fieldname);
				lua_pop(L, 1);
			}
			break;
		case LUACS_TEXTREF:
			luacs_usertable(L, 1);
			lua_pushvalue(L, 3);
			lua_setfield(L, -2, field->fieldname);
			lua_pop(L, 1);
			break;
		case LUACS_TARRAY:
			lua_pushcfunction(L, luacs_array_copy);
			lua_getfield(L, 1, field->fieldname);
			lua_pushvalue(L, 3);
			lua_call(L, 2, 0);
			break;
		}
	} else {
		lua_pushfstring(L, "`struct %s' doesn't have field `%s'",
		    obj->cs->typename, fkey.fieldname);
		lua_error(L);
	}

	return (0);
}

int
luacs_object_copy(lua_State *L)
{
	struct luacobject	*l, *r;
	struct luacstruct_field	*field;

	lua_settop(L, 2);
	l = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	r = luaL_checkudata(L, 2, METANAME_LUACSTRUCTOBJ);
	if (l->cs != r->cs) {
		lua_pushfstring(L,
		    "copying from `struct %s' instance to `struct %s' "
		    "instance is not supported", l->cs->typename,
		    r->cs->typename);
		lua_error(L);
	}

	TAILQ_FOREACH(field, &l->cs->sorted, queue) {
		if (field->regeon.size > 0)
			memcpy((caddr_t)l->ptr + field->regeon.off,
			    (caddr_t)r->ptr + field->regeon.off,
			    field->regeon.size);
		else if (field->regeon.type == LUACS_TOBJREF ||
		    field->regeon.type == LUACS_TOBJENT ||
		    field->regeon.type == LUACS_TEXTREF) {
			/* l[fieldname] = r[fieldname] */
			lua_getfield(L, 2, field->fieldname);
			lua_setfield(L, 1, field->fieldname);
		}
	}

	return (0);
}

int
luacs_object__next(lua_State *L)
{
	struct luacobject	*obj;
	struct luacstruct_field	*field, fkey;

	lua_settop(L, 2);
	obj = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	if (lua_isnil(L, 2))
		field = TAILQ_FIRST(&obj->cs->sorted);
	else {
		fkey.fieldname = luaL_checkstring(L, 2);
		field = SPLAY_FIND(luacstruct_fields, &obj->cs->fields, &fkey);
		if (field != NULL)
			field = TAILQ_NEXT(field, queue);
	}
	if (field == NULL) {
		lua_pushnil(L);
		return (1);
	}
	lua_pushstring(L, field->fieldname);
	luacs_object__get(L, obj, field);

	return (2);
}

int
luacs_object__pairs(lua_State *L)
{
	lua_settop(L, 1);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 1);
	lua_pushnil(L);

	return (3);
}

int
luacs_object__gc(lua_State *L)
{
	struct luacobject	*obj;
	struct luacstruct_field	 fkey, *field;

	lua_settop(L, 1);
	obj = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	fkey.fieldname = "__gc";
	if ((field = SPLAY_FIND(luacstruct_fields, &obj->cs->fields, &fkey))
	    != NULL && field->type == LUACS_TMETHOD) {
		luacs_getref(L, field->ref);
		lua_pushvalue(L, 1);
		lua_pcall(L, 1, 0, 0);
	}
	luacs_unref(L, obj->typref);

	return (0);
}

void *
luacs_checkobject(lua_State *L, int idx, const char *typename)
{
	struct luacobj_compat	 compat;

	memset(&compat, 0, sizeof(compat));
	luacs_object_compat(L, idx, &compat);

	if (compat.typ != NULL && strcmp(compat.typ, typename) == 0)
		return (compat.ptr);

	luaL_error(L, "%s expected, got %s",
	    typename, (compat.typ != NULL)? compat.typ : luaL_typename(L, idx));
	/* NOTREACHED */
	LUACS_ASSERT(L, 0);
	abort();
}

/* regeon */
int
luacs_pushregeon(lua_State *L, struct luacobject *obj,
    struct luacregeon *regeon)
{
	intmax_t	 ival;
	uintmax_t	 uval;

	switch (regeon->type) {
	case LUACS_TINT8:
		lua_pushinteger(L, *(int8_t *)(obj->ptr + regeon->off));
		break;
	case LUACS_TINT16:
		ival = *(int16_t *)(obj->ptr + regeon->off);
		if ((regeon->flags & LUACS_FENDIAN) == 0)
			lua_pushinteger(L, ival);
		else if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			lua_pushinteger(L, be16toh(ival));
		else
			lua_pushinteger(L, le16toh(ival));
		break;
	case LUACS_TINT32:
		ival = *(int32_t *)(obj->ptr + regeon->off);
		if ((regeon->flags & LUACS_FENDIAN) == 0)
			lua_pushinteger(L, ival);
		else if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			lua_pushinteger(L, be32toh(ival));
		else
			lua_pushinteger(L, le32toh(ival));
		break;
	case LUACS_TINT64:
		ival = *(int64_t *)(obj->ptr + regeon->off);
		if ((regeon->flags & LUACS_FENDIAN) == 0)
			lua_pushinteger(L, ival);
		else if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			lua_pushinteger(L, be64toh(ival));
		else
			lua_pushinteger(L, le64toh(ival));
		break;
	case LUACS_TUINT8:
		lua_pushinteger(L, *(uint8_t *)(obj->ptr + regeon->off));
		break;
	case LUACS_TUINT16:
		uval = *(uint16_t *)(obj->ptr + regeon->off);
		if ((regeon->flags & LUACS_FENDIAN) == 0)
			lua_pushinteger(L, uval);
		else if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			lua_pushinteger(L, be16toh(uval));
		else
			lua_pushinteger(L, le16toh(uval));
		break;
	case LUACS_TUINT32:
		uval = *(uint32_t *)(obj->ptr + regeon->off);
		if ((regeon->flags & LUACS_FENDIAN) == 0)
			lua_pushinteger(L, uval);
		else if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			lua_pushinteger(L, be32toh(uval));
		else
			lua_pushinteger(L, le32toh(uval));
		break;
	case LUACS_TUINT64:
		uval = *(uint64_t *)(obj->ptr + regeon->off);
		if ((regeon->flags & LUACS_FENDIAN) == 0)
			lua_pushinteger(L, uval);
		else if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			lua_pushinteger(L, be64toh(uval));
		else
			lua_pushinteger(L, le64toh(uval));
		break;
	case LUACS_TBOOL:
		lua_pushboolean(L, *(bool *)(obj->ptr + regeon->off));
		break;
	case LUACS_TSTRING:
	    {
		int		 len;
		const char	*ch;
		char		*str, buf[128];

		for (len = 0, ch = (const char *)(obj->ptr + regeon->off);
		    ch < (const char *)(obj->ptr + regeon->off +
		    regeon->size) && *ch != '\0'; ch++, len++)
			;
		if (*ch != '\0') {
			if ((str = calloc(1, len + 1)) == NULL) {
				strerror_r(errno, buf, sizeof(buf));
				lua_pushstring(L, buf);
				abort();
			}
			memcpy(str, obj->ptr + regeon->off, len);
			str[len] = '\0';
			lua_pushstring(L, str);
			free(str);
		} else
			lua_pushstring(L,
			    (const char *)(obj->ptr + regeon->off));
		break;
	    }
	case LUACS_TSTRPTR:
		lua_pushstring(L, *(const char **)(obj->ptr + regeon->off));
		break;
	case LUACS_TWSTRING:
	    {
		int		 len;
		const wchar_t	*ch;
		wchar_t		*wstr;
		char		 buf[128];

		for (len = 0, ch = (const wchar_t *)obj->ptr;
		    ch < (const wchar_t *)(obj->ptr + regeon->off +
		    regeon->size) && *ch != L'\0'; ch++, len++)
			;
		if (*ch != L'\0') {
			if ((wstr = calloc(sizeof(wchar_t), len + 1)) == NULL) {
				strerror_r(errno, buf, sizeof(buf));
				lua_pushstring(L, buf);
				abort();
			}
			memcpy(wstr, obj->ptr + regeon->off,
			    sizeof(wchar_t) * len);
			wstr[len] = L'\0';
			luacs_pushwstring(L, wstr);
			free(wstr);
		}
		luacs_pushwstring(L, (const wchar_t *)(obj->ptr + regeon->off));
		break;
	    }
	case LUACS_TWSTRPTR:
		luacs_pushwstring(L,
		    *(const wchar_t **)(obj->ptr + regeon->off));
		break;
	case LUACS_TENUM:
	    {
		intmax_t		 value;
		struct luacenum_value	*val;
		struct luacenum		*ce;
		switch (regeon->size) {
		case 1:	value = *(int8_t  *)(obj->ptr + regeon->off); break;
		case 2:	value = *(int16_t *)(obj->ptr + regeon->off); break;
		case 4:	value = *(int32_t *)(obj->ptr + regeon->off); break;
		case 8:	value = *(int64_t *)(obj->ptr + regeon->off); break;
		default:
			luaL_error(L, "%s: obj is broken", __func__);
			abort();
		}
		luacs_getref(L, regeon->typref);
		ce = luacs_checkenum(L, -1);
		lua_pop(L, 1);
		val = luacs_enum_get0(ce, value);
		if (val == NULL)
			lua_pushinteger(L, value);
		else
			luacs_getref(L, val->ref);
		break;
	    }
	case LUACS_TBYTEARRAY:
		lua_pushlstring(L, (char *)obj->ptr + regeon->off,
		    regeon->size);
		break;
	default:
		lua_pushnil(L);
		break;
	}

	return (1);
}

void
luacs_pullregeon(lua_State *L, struct luacobject *obj,
    struct luacregeon *regeon, int idx)
{
	size_t		 siz;
	int		 absidx;
	intmax_t	 ival;
	uintmax_t	 uval;

	absidx = lua_absindex(L, idx);

	switch (regeon->type) {
	case LUACS_TINT8:
		*(int8_t *)(obj->ptr + regeon->off) = lua_tointeger(L, absidx);
		break;
	case LUACS_TUINT8:
		*(uint8_t *)(obj->ptr + regeon->off) = lua_tointeger(L, absidx);
		break;
	case LUACS_TINT16:
		ival = lua_tointeger(L, absidx);
		if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			ival = htobe16(ival);
		else if ((regeon->flags & LUACS_FENDIANLITTLE) != 0)
			ival = htole16(ival);
		*(int16_t *)(obj->ptr + regeon->off) = ival;
		break;
	case LUACS_TUINT16:
		uval = lua_tointeger(L, absidx);
		if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			uval = htobe16(uval);
		else if ((regeon->flags & LUACS_FENDIANLITTLE) != 0)
			uval = htole16(uval);
		*(uint16_t *)(obj->ptr + regeon->off) = uval;
		break;
	case LUACS_TINT32:
		ival = lua_tointeger(L, absidx);
		if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			ival = htobe32(ival);
		else if ((regeon->flags & LUACS_FENDIANLITTLE) != 0)
			ival = htole32(ival);
		*(int32_t *)(obj->ptr + regeon->off) = ival;
		break;
	case LUACS_TUINT32:
		uval = lua_tointeger(L, absidx);
		if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			uval = htobe32(uval);
		else if ((regeon->flags & LUACS_FENDIANLITTLE) != 0)
			uval = htole32(uval);
		*(uint32_t *)(obj->ptr + regeon->off) = uval;
		break;
	case LUACS_TINT64:
		ival = lua_tointeger(L, absidx);
		if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			ival = htobe64(ival);
		else if ((regeon->flags & LUACS_FENDIANLITTLE) != 0)
			ival = htole64(ival);
		*(int64_t *)(obj->ptr + regeon->off) = ival;
		break;
	case LUACS_TUINT64:
		uval = lua_tointeger(L, absidx);
		if ((regeon->flags & LUACS_FENDIANBIG) != 0)
			uval = htobe64(uval);
		else if ((regeon->flags & LUACS_FENDIANLITTLE) != 0)
			uval = htole64(uval);
		*(uint64_t *)(obj->ptr + regeon->off) = uval;
		break;
	case LUACS_TBOOL:
		*(bool *)(obj->ptr + regeon->off) = lua_toboolean(L, absidx);
		break;
	case LUACS_TENUM:
	    {
		struct luacenum		*ce;
		struct luacenum_value	*val;
		void			*ptr;
		intmax_t		 value;

		luacs_getref(L, regeon->typref);
		ce = luacs_checkenum(L, -1);
		if (lua_type(L, absidx) == LUA_TNUMBER) {
			value = lua_tointeger(L, absidx);
			if (luacs_enum_get0(ce, value) == NULL) {
				lua_pushfstring(L,
				    "must be a valid integer for `enum %s'",
				    ce->enumname);
				lua_error(L);
			}
		} else if (lua_type(L, absidx) == LUA_TUSERDATA) {
			lua_pushcfunction(L, luacs_enum_memberof);
			lua_pushvalue(L, -2);
			lua_pushvalue(L, absidx);
			lua_call(L, 2, 1);
			if (!lua_toboolean(L, -1))
				luaL_error(L,
				    "must be a member of `enum %s",
				    ce->enumname);
			val = lua_touserdata(L, absidx);
			value = val->value;
		} else {
			luaL_error(L, "must be a member of `enum %s",
			    ce->enumname);
			/* NOTREACHED */
			abort();
		}

		ptr = obj->ptr + regeon->off;
		switch (regeon->size) {
		case 1: *(int8_t  *)(ptr) = value; break;
		case 2: *(int16_t *)(ptr) = value; break;
		case 4: *(int32_t *)(ptr) = value; break;
		case 8: *(int64_t *)(ptr) = value; break;
		}
	}
		break;
	case LUACS_TSTRING:
	case LUACS_TBYTEARRAY:
		luaL_checkstring(L, absidx);
		siz = lua_rawlen(L, absidx);
		luaL_argcheck(L, siz <= regeon->size, absidx, "too long");
		siz = MINIMUM(siz, regeon->size);
		memcpy(obj->ptr + regeon->off, lua_tostring(L, absidx), siz);
		if (regeon->type == LUACS_TSTRING && siz < regeon->size)
			*(char *)(obj->ptr + regeon->off + siz) = '\0';
		break;
	case LUACS_TWSTRING:
	    {
		size_t	wstrsiz;

		luaL_checkstring(L, absidx);
		if ((wstrsiz = mbstowcs(NULL, lua_tostring(L, absidx), 0)) ==
		    (size_t)-1) {
			luaL_error(L,
			    "the string contains an invalid character");
			abort();
		}
		wstrsiz *= sizeof(wchar_t);
		luaL_argcheck(L, wstrsiz <= regeon->size, absidx, "too long");
		if (mbstowcs((wchar_t *)(obj->ptr + regeon->off),
		    lua_tostring(L, absidx), wstrsiz) == (size_t)-1) {
			luaL_error(L,
			    "the string contains an invalid character");
			abort();
		}
		if (wstrsiz + sizeof(wchar_t) <= regeon->size)
			*(wchar_t *)(obj->ptr + regeon->off + wstrsiz) = L'\0';
		break;
	    }
	case LUACS_TOBJREF:
	case LUACS_TOBJENT:
	case LUACS_TEXTREF:
		LUACS_ASSERT(L, 0);
		break;
	default:
		lua_pushnil(L);
		break;
	}
}

/* enum */
int
luacs_newenum0(lua_State *L, const char *ename, size_t valwidth)
{
	int		 ret;
	struct luacenum	*ce;
	char		 metaname[METANAMELEN];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, ename);
	lua_getfield(L, LUA_REGISTRYINDEX, metaname);
	if (!lua_isnil(L, -1)) {
		ce = luacs_checkenum(L, -1);
		return (1);
	}
	lua_pop(L, 1);

	ce = lua_newuserdata(L, sizeof(struct luacenum));
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, metaname);
	memcpy(ce->metaname, metaname, MINIMUM(sizeof(metaname),
	    sizeof(ce->metaname)));

	ce->valwidth = valwidth;
	ce->enumname = strchr(ce->metaname, '.') + 1;
	SPLAY_INIT(&ce->labels);
	SPLAY_INIT(&ce->values);
	if ((ret = luaL_newmetatable(L, METANAME_LUACSENUM)) != 0) {
		lua_pushcfunction(L, luacs_enum__gc);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, luacs_enum__index);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, luacs_enum__next);
		lua_pushcclosure(L, luacs_enum__pairs, 1);
		lua_setfield(L, -2, "__pairs");
	}
	lua_setmetatable(L, -2);

	lua_pushvalue(L, -1);
	lua_pushcclosure(L, luacs_enum_get, 1);
	ce->func_get = luacs_ref(L);

	lua_pushvalue(L, -1);
	lua_pushcclosure(L, luacs_enum_memberof, 1);
	ce->func_memberof = luacs_ref(L);

	return (1);
}

int
luacs_delenum(lua_State *L, const char *ename)
{
	char		 metaname[METANAMELEN];

	lua_pushnil(L);
	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, ename);
	lua_setfield(L, LUA_REGISTRYINDEX, metaname);
	return (0);
}

struct luacenum *
luacs_checkenum(lua_State *L, int ceidx)
{
	return (luaL_checkudata(L, ceidx, METANAME_LUACSENUM));
}


struct luacenum_value *
luacs_enum_get0(struct luacenum *ce, intmax_t value)
{
	struct luacenum_value	 vkey;

	vkey.value = value;
	return (SPLAY_FIND(luacenum_values, &ce->values, &vkey));
}

int
luacs_enum_get(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, vkey;

	lua_settop(L, 1);
	ce = luacs_checkenum(L, lua_upvalueindex(1));
	vkey.value = luaL_checkinteger(L, 1);

	if ((val = SPLAY_FIND(luacenum_values, &ce->values, &vkey)) == NULL)
		lua_pushnil(L);
	else
		luacs_getref(L, val->ref);

	return (1);
}

int
luacs_newenumval(lua_State *L, const char *ename, intmax_t ival)
{
	struct luacenum	*ce;
	struct luacenum_value
			*cv;
	char		 metaname[METANAMELEN];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACTYPE, ename);
	lua_getfield(L, LUA_REGISTRYINDEX, metaname);
	if (!lua_isnil(L, -1)) {
		ce = luacs_checkenum(L, -1);
		lua_pop(L, 1);
		cv = luacs_enum_get0(ce, ival);
		if (cv != NULL) {
			luacs_getref(L, cv->ref);
			return (1);
		}
		lua_pushnil(L);
	}
	return (1);
}

int
luacs_enum_memberof(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, *val1;

	lua_settop(L, 1);
	ce = luacs_checkenum(L, lua_upvalueindex(1));
	val = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	val1 = luacs_enum_get0(ce, val->value);
	lua_pushboolean(L, val1 != NULL && val1 == val);

	return (1);
}

int
luacs_enum__index(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, vkey;

	lua_settop(L, 2);
	ce = luacs_checkenum(L, 1);
	vkey.label = luaL_checkstring(L, 2);
	if ((val = SPLAY_FIND(luacenum_labels, &ce->labels, &vkey)) == NULL) {
		if (strcmp(vkey.label, "get") == 0)
			luacs_getref(L, ce->func_get);
		else if (strcmp(vkey.label, "memberof") == 0)
			luacs_getref(L, ce->func_memberof);
		else
			lua_pushnil(L);
	} else
		luacs_getref(L, val->ref);

	return (1);
}

int
luacs_enum__next(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, vkey;

	lua_settop(L, 2);
	ce = luacs_checkenum(L, 1);
	if (lua_isnil(L, 2))
		val= SPLAY_MIN(luacenum_values, &ce->values);
	else {
		vkey.label = luaL_checkstring(L, 2);
		/* don't confuse.  key is label, sort by value */
		val = SPLAY_FIND(luacenum_labels, &ce->labels, &vkey);
		if (val != NULL)
			val = SPLAY_NEXT(luacenum_values, &ce->values, val);
	}
	if (val == NULL) {
		lua_pushnil(L);
		return (1);
	}
	lua_pushstring(L, val->label);
	luacs_getref(L, val->ref);

	return (2);
}

int
luacs_enum__pairs(lua_State *L)
{
	lua_settop(L, 1);
	luacs_checkenum(L, 1);
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 1);
	lua_pushnil(L);

	return (3);
}

int
luacs_enum__gc(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, *valtmp;

	lua_settop(L, 1);
	ce = luacs_checkenum(L, 1);
	val = SPLAY_MIN(luacenum_labels, &ce->labels);
	while (val != NULL) {
		valtmp = SPLAY_NEXT(luacenum_labels, &ce->labels, val);
		SPLAY_REMOVE(luacenum_labels, &ce->labels, val);
		SPLAY_REMOVE(luacenum_values, &ce->values, val);
		luacs_unref(L, val->ref);
		val = valtmp;
	}
	luacs_unref(L, ce->func_get);

	return (0);
}

int
luacs_enum_declare_value(lua_State *L, const char *label, intmax_t value)
{
	int			 ret, llabel;
	struct luacenum_value	*val;
	struct luacenum		*ce;

	ce = luacs_checkenum(L, -1);
	llabel = strlen(label) + 1;
	val = lua_newuserdata(L, sizeof(struct luacenum_value) + llabel);
	memcpy(val + 1, label, llabel);
	val->label = (char *)(val + 1);
	val->value = value;
	SPLAY_INSERT(luacenum_labels, &ce->labels, val);
	SPLAY_INSERT(luacenum_values, &ce->values, val);

	lua_pushvalue(L, -1);
	val->ce = ce;
	val->ref = luacs_ref(L);	/* as for a ref from luacenum */
	if ((ret = luaL_newmetatable(L, METANAME_LUACSENUMVAL)) != 0) {
		lua_pushcfunction(L, luacs_enumvalue__gc);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, luacs_enumvalue__lt);
		lua_setfield(L, -2, "__lt");
		lua_pushcfunction(L, luacs_enumvalue__eq);
		lua_setfield(L, -2, "__eq");
		lua_pushcfunction(L, luacs_enumvalue__tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, luacs_enumvalue_tointeger);
		lua_setfield(L, -2, "tointeger");
		lua_pushcfunction(L, luacs_enumvalue_label);
		lua_setfield(L, -2, "label");
	}
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
	lua_pop(L, 1);

	return (0);
}

int
luacs_enumvalue_tointeger(lua_State *L)
{
	struct luacenum_value	*val;

	lua_settop(L, 1);
	val = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	lua_pushinteger(L, val->value);

	return (1);
}

int
luacs_enumvalue_label(lua_State *L)
{
	struct luacenum_value	*val;

	lua_settop(L, 1);
	val = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	lua_pushstring(L, val->label);

	return (1);
}

int
luacs_enumvalue__tostring(lua_State *L)
{
	struct luacenum_value	*val;
	char			 buf[128];

	lua_settop(L, 1);
	val = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	/* Lua supports limitted int width for number2tstr */
	snprintf(buf, sizeof(buf), "%jd", val->value);
	lua_pushfstring(L, "%s(%s)", val->label, buf);

	return (1);
}

int
luacs_enumvalue__gc(lua_State *L)
{
	lua_settop(L, 1);
	luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);

	return (0);
}

int
luacs_enumvalue__eq(lua_State *L)
{
	struct luacenum_value	*val1, *val2;
	intmax_t		 ival1, ival2;

	lua_settop(L, 2);
	val1 = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	ival1 = val1->value;

	if (lua_isnumber(L, 2))
		ival2 = lua_tointeger(L, 2);
	else {
		val2 = luaL_checkudata(L, 2, METANAME_LUACSENUMVAL);
		ival2 = val2->value;
	}
	if (ival1 == ival2)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);

	return (1);
}

int
luacs_enumvalue__lt(lua_State *L)
{
	struct luacenum_value	*val1, *val2;
	intmax_t		 ival1, ival2;

	lua_settop(L, 2);
	val1 = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	ival1 = val1->value;

	if (lua_isnumber(L, 2))
		ival2 = lua_tointeger(L, 2);
	else {
		val2 = luaL_checkudata(L, 2, METANAME_LUACSENUMVAL);
		ival2 = val2->value;
	}
	if (ival1 < ival2)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);

	return (1);
}

int
luacenum_label_cmp(struct luacenum_value *a, struct luacenum_value *b)
{
	return (strcmp(a->label, b->label));
}

int
luacenum_value_cmp(struct luacenum_value *a, struct luacenum_value *b)
{
	intmax_t	cmp = a->value - b->value;
	return ((cmp == 0)? 0 : (cmp < 0)? -1 : 1);
}

int
luacs_checkenumval(lua_State *L, int idx, const char *enumname)
{
	struct luacenum_value	*val;

	val = luaL_checkudata(L, idx, METANAME_LUACSENUMVAL);
	if (strcmp(val->ce->enumname, enumname) != 0)
		luaL_error(L, "%s expected, got %s", enumname,
		    val->ce->enumname);

	return (val->value);
}

/* refs */
int
luacs_ref(lua_State *L)
{
	int	 ret;

	lua_getfield(L, LUA_REGISTRYINDEX, LUACS_REGISTRY_NAME);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_setfield(L, LUA_REGISTRYINDEX, LUACS_REGISTRY_NAME);
		lua_getfield(L, LUA_REGISTRYINDEX, LUACS_REGISTRY_NAME);
	}
	lua_pushvalue(L, -2);
	ret = luaL_ref(L, -2);
	lua_pop(L, 2);

	return (ret);
}

int
luacs_getref(lua_State *L, int ref)
{
	lua_getfield(L, LUA_REGISTRYINDEX, LUACS_REGISTRY_NAME);
	if (lua_isnil(L, -1))
		return (1);
	lua_rawgeti(L, -1, ref);
	lua_remove(L, -2);
	return (1);
}

int
luacs_unref(lua_State *L, int ref)
{
	lua_getfield(L, LUA_REGISTRYINDEX, LUACS_REGISTRY_NAME);
	luaL_unref(L, -1, ref);
	lua_pop(L, 1);

	return (0);
}

/* utilities */
int
luacs_pushwstring(lua_State *L, const wchar_t *wstr)
{
	char	*mbs = NULL;
	size_t	 mbssiz = 0;
	char	 buf[128];

	if ((mbssiz = wcstombs(mbs, wstr, 0)) == (size_t)-1) {
		luaL_error(L, "the string containing invalid wide character");
		abort();
	}
	mbssiz++;	/* for null char */
	if ((mbs = calloc(1, mbssiz)) == NULL) {
		strerror_r(errno, buf, sizeof(buf));
		lua_pushstring(L, buf);
		abort();
	}
	if ((wcstombs(mbs, wstr, mbssiz)) == (size_t)-1) {
		luaL_error(L, "the string containing invalid wide character");
		abort();
	}
	lua_pushstring(L, mbs);
	free(mbs);

	return (1);
}

SPLAY_GENERATE(luacstruct_fields, luacstruct_field, tree, luacstruct_field_cmp);
SPLAY_GENERATE(luacenum_labels, luacenum_value, treel, luacenum_label_cmp);
SPLAY_GENERATE(luacenum_values, luacenum_value, treev, luacenum_value_cmp);
