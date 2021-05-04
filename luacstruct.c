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
#include <sys/queue.h>
#include <sys/tree.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <lua.h>
#include <lauxlib.h>

#include "luacstruct.h"

#define	METANAME_LUACSTRUCT	"luacstruct"
#define	METANAME_LUACSENUM	"luacenum"
#define	METANAME_LUACSTYPE	"luactype."	/* type registry prefix */
#define	METANAME_LUACSTRUCTOBJ	"luacstructobj"
#define	METANAME_LUACSENUMVAL	"luacenumval"
#define	LUACS_REGISTRY_NAME	"luacstruct_registry"

#if LUA_VERSION_NUM == 501
#define	lua_rawlen(_x, _i)	lua_objlen((_x), (_i))
#endif

#ifdef LUACS_DEBUG
#define LUACS_DBG(...)				\
	do {					\
		fprintf(stdout, __VA_ARGS__);	\
		fputs("\n", stdout);		\
	} while (0/*CONSTCOND*/)
#else
#define LUACS_DBG(...)	((void)0)
#endif

#ifndef MINIMUM
#define MINIMUM(_a, _b)		((_a) < (_b)? (_a) : (_b))
#endif

struct luacstruct {
	const char			*typename;
	char				 metaname[80];
	SPLAY_HEAD(luacstruct_fields, luacstruct_field)
					 fields;
	TAILQ_HEAD(,luacstruct_field)	 sorted;
};

struct luacstruct_field {
	enum luacstruct_type		 fieldtype;
	const char			*fieldname;
	int				 fieldoff;
	size_t				 fieldsiz;
	unsigned			 flags;
	int				 extref;
	SPLAY_ENTRY(luacstruct_field)	 tree;
	TAILQ_ENTRY(luacstruct_field)	 queue;
};

struct luacstruct_obj {
	caddr_t				 obj;
	struct luacstruct		*cs;
	int				 csref;
	int				 tblref;
};

struct luacenum {
	const char			*enumname;
	char				 metaname[80];
	size_t				 valwidth;
	SPLAY_HEAD(luacenum_labels, luacenum_value)
					 labels;
	SPLAY_HEAD(luacenum_values, luacenum_value)
					 values;
};

struct luacenum_value {
	intmax_t			 value;
	int				 ref;
	const char			*label;
	SPLAY_ENTRY(luacenum_value)	 treel;
	SPLAY_ENTRY(luacenum_value)	 treev;
};

static struct luacstruct
		*luacs_checkstruct(lua_State *, int);
static int	 luacs_struct__gc(lua_State *);
static int	 luacstruct_field_cmp(struct luacstruct_field *,
		    struct luacstruct_field *);
static int	 luacs_newobject0(lua_State *, void *);
static int	 luacs_object__index(lua_State *);
static int	 luacs_object__get(lua_State *, struct luacstruct_obj *,
		    struct luacstruct_field *);
static int	 luacs_object__newindex(lua_State *);
static int	 luacs_object_copy(lua_State *);
static int	 luacs_object__next(lua_State *);
static int	 luacs_object__pairs(lua_State *);
static int	 luacs_object__gc(lua_State *);
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
static int	 luacs_enumvalue__tostring(lua_State *);
static int	 luacs_enumvalue__gc(lua_State *);
static int	 luacenum_label_cmp(struct luacenum_value *,
		    struct luacenum_value *);
static int	 luacenum_value_cmp(struct luacenum_value *,
		    struct luacenum_value *);
static int	 luacs_ref(lua_State *);
static int	 luacs_getref(lua_State *, int);
static int	 luacs_unref(lua_State *, int);

SPLAY_PROTOTYPE(luacstruct_fields, luacstruct_field, tree,
    luacstruct_field_cmp);
SPLAY_PROTOTYPE(luacenum_labels, luacenum_value, treel, luacenum_label_cmp);
SPLAY_PROTOTYPE(luacenum_values, luacenum_value, treev, luacenum_value_cmp);

/* struct */
int
luacs_newstruct0(lua_State *L, const char *tname)
{
	int			 ret;
	struct luacstruct	*cs;
	char			 metaname[80];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACSTYPE, tname);
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

	cs->typename = index(cs->metaname, '.') + 1;
	SPLAY_INIT(&cs->fields);
	TAILQ_INIT(&cs->sorted);
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
	char			 metaname[80];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACSTYPE, tname);
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
luacs_struct__gc(lua_State *L)
{
	struct luacstruct	*cs;
	struct luacstruct_field	*field;

	cs = luacs_checkstruct(L, 1);
	if (cs) {
		while ((field = SPLAY_MIN(luacstruct_fields, &cs->fields)) !=
		    NULL) {
			if (field->extref > 0)
				luacs_unref(L, field->extref);
			SPLAY_REMOVE(luacstruct_fields, &cs->fields, field);
			free((char *)field->fieldname);
			free(field);
		}
	}

	return (0);
}

int
luacs_declare_field(lua_State *L, enum luacstruct_type _type,
    const char *tname, const char *name, size_t siz, int off, unsigned flags)
{
	struct luacstruct_field	*field, *field0;
	struct luacstruct	*cs;
	char			 buf[80];

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
	field->fieldtype = _type;
	field->fieldoff = off;
	field->fieldsiz = siz;
	field->flags = flags;
	switch (_type) {
	case LUACS_TOBJREF:
	case LUACS_TENUM:
		snprintf(buf, sizeof(buf), "%s%s", METANAME_LUACSTYPE, tname);
		lua_getfield(L, LUA_REGISTRYINDEX, buf);
		if (lua_isnil(L, -1)) {
			lua_pushfstring(L, "`%s %s' is not registered.",
			    (_type == LUACS_TOBJREF)? "struct" : tname);
			lua_error(L);
		}
		field->extref = luacs_ref(L);
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
	SPLAY_INSERT(luacstruct_fields, &cs->fields, field);
	TAILQ_FOREACH(field0, &cs->sorted, queue) {
		if (field->fieldoff < field0->fieldoff)
			break;
	}
	if (field0 == NULL)
		TAILQ_INSERT_TAIL(&cs->sorted, field, queue);
	else
		TAILQ_INSERT_BEFORE(field0, field, queue);

	return (0);
}

int
luacstruct_field_cmp(struct luacstruct_field *a, struct luacstruct_field *b)
{
	return (strcmp(a->fieldname, b->fieldname));
}

/* object */
int
luacs_newobject(lua_State *L, const char *tname, void *ptr)
{
	int			 ret;
	char			 metaname[80];

	if (ptr == NULL) {
		abort();
	}

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACSTYPE, tname);
	lua_getfield(L, LUA_REGISTRYINDEX, metaname);

	ret = luacs_newobject0(L, ptr);
	lua_remove(L, -2);

	return (ret);
}

int
luacs_newobject0(lua_State *L, void *ptr)
{
	struct luacstruct_obj	*obj;
	struct luacstruct	*cs;
	int			 ret;

	cs = luacs_checkstruct(L, -1);
	obj = lua_newuserdata(L, sizeof(struct luacstruct_obj));
	obj->obj = ptr;
	obj->cs = cs;
	lua_pushvalue(L, -2);
	obj->csref = luacs_ref(L);
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
	}
	lua_setmetatable(L, -2);
	lua_newtable(L);
	obj->tblref = luacs_ref(L);

	return (1);
}

int
luacs_object__index(lua_State *L)
{
	struct luacstruct_obj	*obj;
	struct luacstruct_field	 fkey, *field;

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
luacs_object__get(lua_State *L, struct luacstruct_obj *obj,
    struct luacstruct_field *field)
{
	void	*ptr;

	switch (field->fieldtype) {
	case LUACS_TINT8:
		lua_pushinteger(L, *(int8_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TUINT8:
		lua_pushinteger(L, *(uint8_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TINT16:
		lua_pushinteger(L, *(int16_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TUINT16:
		lua_pushinteger(L, *(uint16_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TINT32:
		lua_pushinteger(L, *(int32_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TUINT32:
		lua_pushinteger(L, *(uint32_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TINT64:
		lua_pushinteger(L, *(int64_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TUINT64:
		lua_pushinteger(L, *(uint64_t *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TENUM:
	    {
		intmax_t		 value;
		struct luacenum_value	*val;
		struct luacenum		*ce;
		switch (field->fieldsiz) {
		case 1:	value = *(int8_t  *)(obj->obj + field->fieldoff); break;
		case 2:	value = *(int16_t *)(obj->obj + field->fieldoff); break;
		case 4:	value = *(int32_t *)(obj->obj + field->fieldoff); break;
		case 8:	value = *(int64_t *)(obj->obj + field->fieldoff); break;
		}
		luacs_getref(L, field->extref);
		ce = luacs_checkenum(L, -1);
		lua_pop(L, 1);
		val = luacs_enum_get0(ce, value);
		if (val == NULL)
			lua_pushinteger(L, value);
		else
			luacs_getref(L, val->ref);
		break;
	    }
	case LUACS_TBOOL:
		lua_pushboolean(L, *(bool *)(obj->obj + field->fieldoff));
		break;
	case LUACS_TSTRING:
		lua_pushstring(L, (const char *) (obj->obj + field->fieldoff));
		break;
	case LUACS_TSTRPTR:
		lua_pushstring(L, *(const char **)(obj->obj + field->fieldoff));
		break;
	case LUACS_TBYTEARRAY:
		lua_pushlstring(L, (char *)obj->obj + field->fieldoff,
		    field->fieldsiz);
		break;
	case LUACS_TOBJREF:
		if (field->fieldsiz == 0) /* nested */
			ptr = obj->obj + field->fieldoff;
		else
			ptr = *(void **)(obj->obj + field->fieldoff);
		if (ptr == NULL)
			lua_pushnil(L);
		else {
			luacs_getref(L, obj->tblref);
			lua_getfield(L, -1, field->fieldname);
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1);
				luacs_getref(L, field->extref);
				luacs_newobject0(L, ptr);
				lua_pushvalue(L, -1);
				lua_setfield(L, -4, field->fieldname);
				lua_remove(L, -2);
			}
			lua_remove(L, -2);
		}
		break;
	case LUACS_TEXTREF:
		luacs_getref(L, obj->tblref);
		lua_getfield(L, -1, field->fieldname);
		lua_remove(L, -2);
		break;
	default:
		lua_pushnil(L);
		break;
	}

	return (1);
}

int
luacs_object__newindex(lua_State *L)
{
	struct luacstruct	*cs0;
	struct luacstruct_obj	*obj, *ano;
	struct luacstruct_field	 fkey, *field;
	size_t			 siz;

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
		switch (field->fieldtype) {
		case LUACS_TINT8:
			*(int8_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TUINT8:
			*(uint8_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TINT16:
			*(int16_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TUINT16:
			*(uint16_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TINT32:
			*(int32_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TUINT32:
			*(uint32_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TINT64:
			*(int64_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TUINT64:
			*(uint16_t *)(obj->obj + field->fieldoff) =
			    lua_tointeger(L, 3);
			break;
		case LUACS_TBOOL:
			*(bool *)(obj->obj + field->fieldoff) =
			    lua_toboolean(L, 3);
			break;
		case LUACS_TENUM:
		    {
			struct luacenum		*ce;
			struct luacenum_value	*val;
			void			*ptr;
			intmax_t		 value;

			luacs_getref(L, field->extref);
			ce = luacs_checkenum(L, -1);
			if (lua_type(L, 3) == LUA_TNUMBER) {
				value = lua_tointeger(L, 3);
				if (luacs_enum_get0(ce, value) == NULL) {
					lua_pushfstring(L,
					    "`%s' field must be a valid "
					    "integer for `enum %s'",
					    field->fieldname, ce->enumname);
					lua_error(L);
				}
			} else if (lua_type(L, 3) == LUA_TUSERDATA) {
				lua_pushcfunction(L, luacs_enum_memberof);
				lua_pushvalue(L, -2);
				lua_pushvalue(L, 3);
				lua_call(L, 2, 1);
				if (!lua_toboolean(L, -1)) {
					lua_pushfstring(L,
					    "`%s' field must be a member of "
					    "`enum %s", field->fieldname,
					    ce->enumname);
					lua_error(L);
				}
				val = lua_touserdata(L, 3);
				value = val->value;
			}
			ptr = obj->obj + field->fieldoff;
			switch (field->fieldsiz) {
			case 1: *(int8_t  *)(ptr) = value; break;
			case 2: *(int16_t *)(ptr) = value; break;
			case 4: *(int32_t *)(ptr) = value; break;
			case 8: *(int64_t *)(ptr) = value; break;
			}
		}
			break;
		case LUACS_TSTRING:
		case LUACS_TBYTEARRAY:
			luaL_checkstring(L, 3);
			siz = lua_rawlen(L, 3);
			luaL_argcheck(L, siz < field->fieldsiz, 3, "too long");
			memset(obj->obj + field->fieldoff, 0,
			    field->fieldsiz);
			memcpy(obj->obj + field->fieldoff,
			    lua_tostring(L, 3), MINIMUM(siz, field->fieldsiz));
			if (field->fieldtype == LUACS_TSTRING)
				*(char *)(obj->obj + field->fieldoff +
				    field->fieldsiz -1) = '\0';
			break;
		case LUACS_TSTRPTR:
			goto readonly;
		case LUACS_TOBJREF:
			/* get c struct of the field */
			luacs_getref(L, field->extref);
			cs0 = luacs_checkstruct(L, -1);
			lua_pop(L, 1);
			/* given instance of struct */
			ano = luaL_checkudata(L, 3, METANAME_LUACSTRUCTOBJ);
			if (cs0 != ano->cs) {
				lua_pushfstring(L,
				    "`%s' field must be an instance of "
				    "`struct %s'", field->fieldname,
				    cs0->typename);
				lua_error(L);
			}
			if (field->fieldsiz == 0) {	/* nested */
				lua_pushcfunction(L, luacs_object_copy);
				lua_getfield(L, 1, field->fieldname);
				lua_pushvalue(L, 3);
				lua_call(L, 2, 0);
			} else {
				*(void **)(obj->obj + field->fieldoff) =
				    ano->obj;
				/* use the same object */
				luacs_getref(L, obj->tblref);
				lua_pushvalue(L, 3);
				lua_setfield(L, -2, field->fieldname);
				lua_pop(L, 1);
			}
			break;
		case LUACS_TEXTREF:
			luacs_getref(L, obj->tblref);
			lua_pushvalue(L, 3);
			lua_setfield(L, -2, field->fieldname);
			lua_pop(L, 1);
			break;
		default:
			lua_pushnil(L);
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
	struct luacstruct_obj	*l, *r;
	struct luacstruct_field	*field;

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
		if (field->fieldsiz > 0)
			memcpy((caddr_t)l->obj + field->fieldoff,
			    (caddr_t)r->obj + field->fieldoff, field->fieldsiz);
		else if (field->fieldtype == LUACS_TOBJREF ||
		    field->fieldtype == LUACS_TEXTREF) {
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
	struct luacstruct_obj	*obj;
	struct luacstruct_field	*field, fkey;

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
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 1);
	lua_pushnil(L);

	return (3);
}

int
luacs_object__gc(lua_State *L)
{
	struct luacstruct_obj	*obj;

	obj = luaL_checkudata(L, 1, METANAME_LUACSTRUCTOBJ);
	luacs_unref(L, obj->csref);
	luacs_unref(L, obj->tblref);

	return (0);
}

/* enum */
int
luacs_newenum0(lua_State *L, const char *ename, size_t valwidth)
{
	int		 ret;
	struct luacenum	*ce;
	char		 metaname[80];

	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACSTYPE, ename);
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
	ce->enumname = index(ce->metaname, '.') + 1;
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
		lua_pushcfunction(L, luacs_enum_get);
		lua_setfield(L, -2, "get");
		lua_pushcfunction(L, luacs_enum_memberof);
		lua_setfield(L, -2, "memberof");
	}
	lua_setmetatable(L, -2);

	return (1);
}

int
luacs_delenum(lua_State *L, const char *ename)
{
	char		 metaname[80];

	lua_pushnil(L);
	snprintf(metaname, sizeof(metaname), "%s%s", METANAME_LUACSTYPE, ename);
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

	ce = luacs_checkenum(L, 1);
	vkey.value = luaL_checkinteger(L, 2);

	if ((val = SPLAY_FIND(luacenum_values, &ce->values, &vkey)) == NULL)
		lua_pushnil(L);
	else
		luacs_getref(L, val->ref);

	return (1);
}

int
luacs_enum_memberof(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, *val1;

	ce = luacs_checkenum(L, 1);
	val = luaL_checkudata(L, 2, METANAME_LUACSENUMVAL);
	val1 = luacs_enum_get0(ce, val->value);
	lua_pushboolean(L, val1 != NULL && val1 == val);

	return (1);
}

int
luacs_enum__index(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, vkey;

	ce = luacs_checkenum(L, 1);
	vkey.label = luaL_checkstring(L, 2);
	if ((val = SPLAY_FIND(luacenum_labels, &ce->labels, &vkey)) == NULL)
		lua_pushnil(L);
	else
		luacs_getref(L, val->ref);

	return (1);
}

int
luacs_enum__next(lua_State *L)
{
	struct luacenum		*ce;
	struct luacenum_value	*val, vkey;

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

	ce = luacs_checkenum(L, 1);
	val = SPLAY_MIN(luacenum_labels, &ce->labels);
	while (val != NULL) {
		valtmp = SPLAY_NEXT(luacenum_labels, &ce->labels, val);
		SPLAY_REMOVE(luacenum_labels, &ce->labels, val);
		SPLAY_REMOVE(luacenum_values, &ce->values, val);
		luacs_unref(L, val->ref);
		val = valtmp;
	}

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
	val->ref = luacs_ref(L);	/* as for a ref from luacenum */
	if ((ret = luaL_newmetatable(L, METANAME_LUACSENUMVAL)) != 0) {
		lua_pushcfunction(L, luacs_enumvalue__gc);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, luacs_enumvalue__tostring);
		lua_setfield(L, -2, "__tostring");
		lua_pushcfunction(L, luacs_enumvalue_tointeger);
		lua_setfield(L, -2, "tointeger");
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

	val = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	lua_pushinteger(L, val->value);

	return (1);
}

int
luacs_enumvalue__tostring(lua_State *L)
{
	struct luacenum_value	*val;
	char			 buf[80];

	val = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);
	/* Lua supports limitted int width for number2tstr */
	snprintf(buf, sizeof(buf), "%jd", val->value);
	lua_pushfstring(L, "%s(%s)", val->label, buf);

	return (1);
}

int
luacs_enumvalue__gc(lua_State *L)
{
	struct luacenum_value	*val;
	val = luaL_checkudata(L, 1, METANAME_LUACSENUMVAL);

	return (0);
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

SPLAY_GENERATE(luacstruct_fields, luacstruct_field, tree, luacstruct_field_cmp);
SPLAY_GENERATE(luacenum_labels, luacenum_value, treel, luacenum_label_cmp);
SPLAY_GENERATE(luacenum_values, luacenum_value, treev, luacenum_value_cmp);
