#include <stdlib.h>
#include <stdio.h>
#include <lua.h>
#include "luacstruct.h"

#ifndef static_assert
#define static_assert(_cond, _msg) ((void)0)
#endif

static int l_test_ref(lua_State *);
static int l_test_nest(lua_State *);
static int l_test_ext(lua_State *);
static int l_test_enum(lua_State *);
static int l_test_copy(lua_State *);
static int l_test_array(lua_State *);
static int l_test_tostring(lua_State *);

int
luaopen_extra(lua_State *L)
{
	lua_register(L, "test_ref", l_test_ref);
	lua_register(L, "test_nest", l_test_nest);
	lua_register(L, "test_ext", l_test_ext);
	lua_register(L, "test_enum", l_test_enum);
	lua_register(L, "test_copy", l_test_copy);
	lua_register(L, "test_array", l_test_array);
	lua_register(L, "test_tostring", l_test_tostring);
	lua_register(L, "typename", luacs_object_typename);

	return (0);
}

int
l_test_ref(lua_State *L)
{
	struct ref_sub {
		int	x;
		int	y;
	} *s;
	struct ref_main {
		int		 a;
		int		 b;
		struct ref_sub	*ref;
	} *m;

	luacs_newstruct(L, ref_sub);
	luacs_int_field(L, ref_sub, x, 0);
	luacs_int_field(L, ref_sub, y, 0);

	luacs_newstruct(L, ref_main);
	luacs_int_field(L, ref_main, a, 0);
	luacs_int_field(L, ref_main, b, 0);
	luacs_objref_field(L, ref_main, ref_sub, ref, 0);
	lua_pop(L, 2);

	m = calloc(1, sizeof(struct ref_main));
	s = calloc(1, sizeof(struct ref_sub));
	m->a = 1;
	m->b = 2;
	m->ref = s;
	s->x = 3;
	s->y = 4;

	luacs_newobject(L, "ref_main", m);

	return (1);
}

int
l_test_nest(lua_State *L)
{
	struct nest_sub {
		int	x;
		int	y;
	};
	struct nest_main {
		int		 a;
		int		 b;
		struct nest_sub	 nest;
	} *m;

	luacs_newstruct(L, nest_sub);
	luacs_int_field(L, nest_sub, x, 0);
	luacs_int_field(L, nest_sub, y, 0);

	luacs_newstruct(L, nest_main);
	luacs_int_field(L, nest_main, a, 0);
	luacs_int_field(L, nest_main, b, 0);
	luacs_nested_field(L, nest_main, nest_sub, nest, 0);
	lua_pop(L, 2);

	m = calloc(1, sizeof(struct nest_main));
	m->a = 1;
	m->b = 2;
	m->nest.x = 3;
	m->nest.y = 4;

	luacs_newobject(L, "nest_main", m);

	return (1);
}

int
l_test_ext(lua_State *L)
{
	struct ext_main {
		int		 a;
		int		 b;
		void		*ref;
	} *m;

	luacs_newstruct(L, ext_main);
	luacs_int_field(L, ext_main, a, 0);
	luacs_int_field(L, ext_main, b, 0);
	luacs_extref_field(L, ext_main, ref, 0);
	luacs_pseudo_field(L, ext_main, extras, 0);
	lua_pop(L, 2);

	m = calloc(1, sizeof(struct ext_main));
	m->a = 1;
	m->b = 2;

	luacs_newobject(L, "ext_main", m);

	return (1);
}

int
l_test_enum(lua_State *L)
{
	enum COLOR {
		RED,
		GREEN,
		BLUE = 0x100000000ULL
	};
	struct enum_main {
		int	x;
		int	y;
		int64_t	z;
		enum COLOR
			color;
		enum COLOR
			invalid_color;
	} *m;

	luacs_newenum(L, COLOR);
	luacs_enum_declare_value(L, "RED", RED);
	luacs_enum_declare_value(L, "GREEN", GREEN);
	luacs_enum_declare_value(L, "BLUE", BLUE);

	luacs_newstruct(L, enum_main);
	luacs_int_field(L, enum_main, x, 0);
	luacs_int_field(L, enum_main, y, 0);
	luacs_int_field(L, enum_main, z, 0);
	luacs_enum_field(L, enum_main, COLOR, color, 0);
	luacs_enum_field(L, enum_main, COLOR, invalid_color, 0);
	lua_pop(L, 1);

	m = calloc(1, sizeof(struct enum_main));
	m->x = 100;
	m->y = 200;
	m->z = BLUE;
	m->color = BLUE;
	m->invalid_color = 99;
	luacs_newobject(L, "enum_main", m);

	return (2);
}

#include <string.h>

int
l_test_copy(lua_State *L)
{
	struct copy_fuga {
		int			 id;
		char			*name;
	} *f1, *f2;
	struct copy_main {
		int			 id;
		char			*name;
		struct copy_fuga	 fuga1;
		struct copy_fuga	*fuga2;
	} *m1, *m2;

	luacs_newstruct(L, copy_fuga);
	luacs_int_field(L, copy_fuga, id, 0);
	luacs_strptr_field(L, copy_fuga, name, 0);
	luacs_pseudo_field(L, copy_fuga, pseudo, 0);
	luacs_newstruct(L, copy_main);
	luacs_int_field(L, copy_main, id, 0);
	luacs_strptr_field(L, copy_main, name, 0);
	luacs_nested_field(L, copy_main, copy_fuga, fuga1, 0);
	luacs_objref_field(L, copy_main, copy_fuga, fuga2, 0);
	lua_pop(L, 2);

	f1 = calloc(1, sizeof(struct copy_fuga));
	f2 = calloc(1, sizeof(struct copy_fuga));
	m1 = calloc(1, sizeof(struct copy_main));
	m2 = calloc(1, sizeof(struct copy_main));

	int seq = 1;
	f1->id = seq++;
	f2->id = seq++;
	m1->id = seq++;
	m1->fuga1.id = seq;
	m2->id = seq++;
	m2->fuga1.id = seq;

	f1->name = strdup("f1");
	f2->name = strdup("f2");
	m1->name = strdup("m1");
	m1->fuga1.name = strdup("m1/fuga1");
	m2->name = strdup("m2");
	m2->fuga1.name = strdup("m2/fuga1");

	luacs_newobject(L, "copy_fuga", f1);
	luacs_newobject(L, "copy_fuga", f2);
	luacs_newobject(L, "copy_main", m1);
	luacs_newobject(L, "copy_main", m2);

	return (4);
}

int
l_test_array(lua_State *L)
{
	struct array_sub {
		int	x;
		int	y;
		int	z;
	};
	struct array_main {
		int			 int4[4];
		struct array_sub	*sub2[2];
		struct array_sub	 sub3[3];
		void			*ext2[2];
		int			 intxy[3][3];
	} *m1, *m2;

	luacs_newstruct(L, array_sub);
	luacs_int_field(L, array_sub, x, 0);
	luacs_int_field(L, array_sub, y, 0);
	luacs_int_field(L, array_sub, z, 0);

	luacs_newarraytype(L, "int3", LUACS_TINT32, NULL, sizeof(int32_t), 3,
	    0);
	luacs_newstruct(L, array_main);
	luacs_int_array_field(L, array_main, int4, 0);
	luacs_objref_array_field(L, array_main, array_sub, sub2, 0);
	luacs_nested_array_field(L, array_main, array_sub, sub3, 0);
	luacs_extref_array_field(L, array_main, ext2, 0);
	luacs_array_array_field(L, array_main, int3, intxy, 0);
	lua_remove(L, -2);

	m1 = calloc(1, sizeof(struct array_main));
	m1->int4[0] = 1;
	m1->int4[1] = 2;
	m1->int4[2] = 3;
	m1->int4[3] = 4;
	m1->sub2[0] = calloc(1, sizeof(struct array_sub));
	m1->sub2[1] = calloc(1, sizeof(struct array_sub));
	m1->sub2[0]->x = 1;
	m1->sub2[0]->y = 2;
	m1->sub2[0]->z = 3;
	m1->sub2[1]->x = 11;
	m1->sub2[1]->y = 12;
	m1->sub2[1]->z = 13;
	m1->sub3[0].x = 21;
	m1->sub3[0].y = 22;
	m1->sub3[0].z = 23;
	m1->sub3[1].x = 31;
	m1->sub3[1].y = 32;
	m1->sub3[1].z = 33;
	m1->sub3[2].x = 41;
	m1->sub3[2].y = 42;
	m1->sub3[2].z = 43;

	luacs_newobject(L, "array_main", m1);

	m2 = calloc(1, sizeof(struct array_main));

	luacs_newobject(L, "array_main", m2);

	luacs_newarray(L, LUACS_TINT32, NULL, sizeof(int32_t), 8, 0,
	    calloc(8, sizeof(int32_t)));
	luacs_newarray(L, LUACS_TINT32, NULL, sizeof(int32_t), 8, 0,
	    calloc(8, sizeof(int32_t)));


	return (4);
}

struct person {
	const char	*name;
	int		 height;	/* in cm */
	int		 weight;	/* in kg */
};

static int
person_bmi(lua_State *L)
{
	struct person *self;

	self = luacs_object_pointer(L, 1);
	lua_pushnumber(L, ((double)self->weight / (self->height *
	    self->height)) * 10000.0);

	return (1);
}

static int
person_tostring(lua_State *L)
{
	struct person *self;
	char	*buf = NULL;

	self = luacs_object_pointer(L, 1);
	asprintf(&buf, "%s(%d,%d)", self->name, self->height, self->weight);
	lua_pushstring(L, buf);
	free(buf);

	return (1);
}

int
l_test_tostring(lua_State *L)
{
	luacs_newstruct(L, person);
	luacs_strptr_field(L, person, name, 0);
	luacs_int_field(L, person, height, 0);
	luacs_int_field(L, person, weight, 0);

	luacs_declare_method(L, "bmi", person_bmi);
	luacs_declare_method(L, "__tostring", person_tostring);

	lua_pop(L, 1);

	static struct person aperson = { .name = "yamada", 168, 63 };

	luacs_newobject(L, "person", &aperson);

	return (1);
}
