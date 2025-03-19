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
#ifndef LUACSTRUCT_H
#define LUACSTRUCT_H 1

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <lua.h>

enum luacstruct_type {
	LUACS_TINT8,
	LUACS_TINT16,
	LUACS_TINT32,
	LUACS_TINT64,
	LUACS_TUINT8,
	LUACS_TUINT16,
	LUACS_TUINT32,
	LUACS_TUINT64,
	LUACS_TENUM,
	LUACS_TBOOL,
	LUACS_TSTRING,
	LUACS_TSTRPTR,
	LUACS_TWSTRING,
	LUACS_TWSTRPTR,
	LUACS_TBYTEARRAY,
	LUACS_TOBJREF,
	LUACS_TOBJENT,
	LUACS_TEXTREF,
	LUACS_TARRAY,
	LUACS_TMETHOD,
	LUACS_TCONST
};

#define LUACS_FREADONLY		0x01
#define LUACS_FENDIANBIG	0x02
#define LUACS_FENDIANLITTLE	0x04
#define LUACS_FENDIAN		(LUACS_FENDIANBIG | LUACS_FENDIANLITTLE)

#ifdef __cplusplus
extern "C" {
#endif

int	 luacs_newstruct0(lua_State *, const char *, const char *);
int	 luacs_declare_method(lua_State *, const char *, int (*)(lua_State *));
int	 luacs_declare_const(lua_State *, const char *, int);
int	 luacs_delstruct(lua_State *, const char *);
int	 luacs_declare_field(lua_State *, enum luacstruct_type,
	    const char *, const char *, size_t, int, int, unsigned);
int	 luacs_newobject(lua_State *, const char *, void *);
void	*luacs_object_pointer(lua_State *, int, const char *);
void	 luacs_object_clear(lua_State *, int);
int	 luacs_object_typename(lua_State *);
void	*luacs_checkobject(lua_State *, int, const char *);
int	 luacs_newenum0(lua_State *, const char *, size_t);
int	 luacs_newenumval(lua_State *, const char *, intmax_t);
int	 luacs_delenum(lua_State *, const char *);
int	 luacs_enum_declare_value(lua_State *, const char *, intmax_t);
int	 luacs_checkenumval(lua_State *, int, const char *);
int	 luacs_newarray(lua_State *, enum luacstruct_type, const char *,
	    size_t, int, unsigned, void *);
int	 luacs_newarraytype(lua_State *, const char *, enum luacstruct_type,
	    const char *, size_t, int, unsigned);

#ifdef __cplusplus
}
#endif

#define luacs_newstruct(_L, _typename)				\
	do {							\
		{ struct _typename; /* check valid for type */}	\
		luacs_newstruct0((_L), #_typename, NULL);	\
	} while(0/*CONSTCOND*/)
#define luacs_newenum(_L, _enumname)				\
	do {							\
		{ enum _enumname; /* check valid for enum */}	\
		luacs_newenum0((_L), #_enumname,		\
		    sizeof(enum _enumname));			\
	} while(0/*CONSTCOND*/)
#define validintwidth(_w)	\
	((_w) == 1 || (_w) == 2 || (_w) == 4 || (_w) == 8)
#define luacs_int_field(_L, _type, _field, _flags)		\
	do {							\
		static_assert(validintwidth(			\
		    sizeof((struct _type *)0)->_field),		\
		    "`"#_field"' is an unsupported int type");	\
		enum luacstruct_type _itype;			\
		switch(sizeof(((struct _type *)0)->_field)) {	\
		case 1:	_itype = LUACS_TINT8; break;		\
		case 2:	_itype = LUACS_TINT16; break;		\
		case 4:	_itype = LUACS_TINT32; break;		\
		case 8:	_itype = LUACS_TINT64; break;		\
		}						\
		luacs_declare_field((_L), _itype, NULL,	#_field,\
		    sizeof(((struct _type *)0)->_field),	\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_unsigned_field(_L, _type, _field, _flags)		\
	do {							\
		static_assert(validintwidth(			\
		    sizeof((struct _type *)0)->_field),		\
		    "`"#_field"' is an unsupported int type");	\
		enum luacstruct_type _itype;			\
		switch (sizeof(((struct _type *)0)->_field)) {	\
		case 1:	_itype = LUACS_TUINT8; break;		\
		case 2:	_itype = LUACS_TUINT16; break;		\
		case 4:	_itype = LUACS_TUINT32; break;		\
		case 8:	_itype = LUACS_TUINT64; break;		\
		}						\
		luacs_declare_field((_L), _itype, NULL,	#_field,\
		    sizeof(((struct _type *)0)->_field),	\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_enum_field(_L, _type, _etype, _field, _flags)	\
	do {							\
		static_assert(validintwidth(			\
		    sizeof((struct _type *)0)->_field),		\
		    "`"#_field"' is an unsupported int type");	\
		luacs_declare_field((_L), LUACS_TENUM, #_etype,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_bool_field(_L, _type, _field, _flags)		\
	do {							\
		static_assert(sizeof(bool) ==			\
		    sizeof(((struct _type *)0)->_field),	\
		    "`"#_field"' is not a bool value");		\
		luacs_declare_field((_L), LUACS_TBOOL, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_bytearray_field(_L, _type, _field, _flags)	\
	do {							\
		luacs_declare_field((_L), LUACS_TBYTEARRAY, NULL,\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_string_field(_L, _type, _field, _flags)		\
	do {							\
		luacs_declare_field((_L), LUACS_TSTRING, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_strptr_field(_L, _type, _field, _flags)		\
	do {							\
		static_assert(sizeof(char *) ==			\
		    sizeof(((struct _type *)0)->_field),	\
		    "`"#_field"' is not a pointer value");	\
		luacs_declare_field((_L), LUACS_TSTRPTR, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
/*
 * Declare the wide char(wchar_t) string field.  Because Lua doesn't use
 * wchar_t string, lua_cstruct convert the string into multi-byte chars
 * by wcstombs(3) when mapping to Lua.  This means the string is encoded
 * by the encoding specified by setlocale(3).
 */
#define luacs_wstring_field(_L, _type, _field, _flags)		\
	do {							\
		luacs_declare_field((_L), LUACS_TWSTRING, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_wstrptr_field(_L, _type, _field, _flags)		\
	do {							\
		static_assert(sizeof(wchar_t *) ==		\
		    sizeof(((struct _type *)0)->_field),	\
		    "`"#_field"' is not a pointer value");	\
		luacs_declare_field((_L), LUACS_TWSTRPTR, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_objref_field(_L, _type, _tname, _field, _flags)	\
	do {							\
		static_assert(sizeof(void *) ==			\
		    sizeof(((struct _type *)0)->_field),	\
		    "`"#_field"' is not a pointer value");	\
		luacs_declare_field((_L), LUACS_TOBJREF, #_tname,\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_nested_field(_L, _type, _tname, _field, _flags)	\
	do {							\
		luacs_declare_field((_L), LUACS_TOBJENT, #_tname,\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
#define luacs_extref_field(_L, _type, _field, _flags)		\
	do {							\
		luacs_declare_field((_L), LUACS_TEXTREF, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field), 0, _flags);	\
	} while (0/*CONSTCOND*/)
/*
 * When using Lua 5.1 and the pseudo value might reference the parent object,
 * you need to explicitly clear the value or call luacs_object_clear().
 * luacstruct uses a weak reference to manage the reference from the object to
 * the pseudo values, but since a weak table in Lua 5.1 is not an ephemeron
 * table, it doesn't work properly.
 */
#define luacs_pseudo_field(_L, _type, _field, _flags)		\
	do {							\
		luacs_declare_field((_L), LUACS_TEXTREF, NULL,	\
		    #_field, 0, 0, 0, _flags);		\
	} while (0/*CONSTCOND*/)

#define	_nitems(_x)		(sizeof(_x) / sizeof((_x)[0]))

#define luacs_int_array_field(_L, _type, _field, _flags)	\
	do {							\
		static_assert(validintwidth(			\
		    sizeof((struct _type *)0)->_field[0]),	\
		    "`"#_field"' is an unsupported int type");	\
		enum luacstruct_type _itype;			\
		switch(sizeof(((struct _type *)0)->_field[0])) {\
		case 1:	_itype = LUACS_TINT8; break;		\
		case 2:	_itype = LUACS_TINT16; break;		\
		case 4:	_itype = LUACS_TINT32; break;		\
		case 8:	_itype = LUACS_TINT64; break;		\
		}						\
		luacs_declare_field((_L), _itype, NULL,	#_field,\
		    sizeof(((struct _type *)0)->_field[0]),	\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_unsigned_array_field(_L, _type, _field, _flags)	\
	do {							\
		static_assert(validintwidth(			\
		    sizeof((struct _type *)0)->_field[0]),	\
		    "`"#_field"' is an unsupported int type");	\
		enum luacstruct_type _itype;			\
		switch (sizeof(((struct _type *)0)->_field[0])){\
		case 1:	_itype = LUACS_TUINT8; break;		\
		case 2:	_itype = LUACS_TUINT16; break;		\
		case 4:	_itype = LUACS_TUINT32; break;		\
		case 8:	_itype = LUACS_TUINT64; break;		\
		}						\
		luacs_declare_field((_L), _itype, NULL,	#_field,\
		    sizeof(((struct _type *)0)->_field[0]),	\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_enum_array_field(_L, _type, _etype, _field, _flags)\
	do {							\
		static_assert(validintwidth(			\
		    sizeof((struct _type *)0)->_field[0]),	\
		    "`"#_field"' is an unsupported int type");	\
		luacs_declare_field((_L), LUACS_TENUM, #_etype,	\
		    #_field, sizeof(((struct _type *)0)->_field),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_bool_array_field(_L, _type, _field, _flags)	\
	do {							\
		static_assert(sizeof(bool) ==			\
		    sizeof(((struct _type *)0)->_field[0]),	\
		    "`"#_field"' is not a bool value");		\
		luacs_declare_field((_L), LUACS_TBOOL, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_bytearray_array_field(_L, _type, _field, _flags)	\
	do {							\
		luacs_declare_field((_L), LUACS_TBYTEARRAY, NULL,\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_string_array_field(_L, _type, _field, _flags)	\
	do {							\
		luacs_declare_field((_L), LUACS_TSTRING, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_strptr_array_field(_L, _type, _field, _flags)	\
	do {							\
		static_assert(sizeof(char *) ==			\
		    sizeof(((struct _type *)0)->_field[0]),	\
		    "`"#_field"' is not a pointer value");	\
		luacs_declare_field((_L), LUACS_TSTRPTR, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)

#define luacs_objref_array_field(_L, _type, _tname, _field, _flags)\
	do {							\
		static_assert(sizeof(void *) ==			\
		    sizeof(((struct _type *)0)->_field[0]),	\
		    "`"#_field"' is not a pointer value");	\
		luacs_declare_field((_L), LUACS_TOBJREF, #_tname,\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_nested_array_field(_L, _type, _tname, _field, _flags)\
	do {							\
		luacs_declare_field((_L), LUACS_TOBJENT, #_tname,\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_extref_array_field(_L, _type, _field, _flags)	\
	do {							\
		luacs_declare_field((_L), LUACS_TEXTREF, NULL,	\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)
#define luacs_array_array_field(_L, _type, _tname, _field, _flags)\
	do {							\
		luacs_declare_field((_L), LUACS_TARRAY, #_tname,\
		    #_field, sizeof(((struct _type *)0)->_field[0]),\
		    offsetof(struct _type, _field),		\
		    _nitems(((struct _type *)0)->_field), _flags);\
	} while (0/*CONSTCOND*/)

#endif
