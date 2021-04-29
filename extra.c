#include <stdlib.h>
#include <lua.h>
#include "luacstruct.h"

static int l_test_ref(lua_State *);
static int l_test_nest(lua_State *);
static int l_test_ext(lua_State *);
static int l_test_enum(lua_State *);
static int l_test_copy(lua_State *);
static int l_test_fini(lua_State *);

int
luaopen_extra(lua_State *L)
{
	lua_register(L, "test_ref", l_test_ref);
	lua_register(L, "test_nest", l_test_nest);
	lua_register(L, "test_ext", l_test_ext);
	lua_register(L, "test_enum", l_test_enum);
	lua_register(L, "test_fini", l_test_fini);
	lua_register(L, "test_copy", l_test_copy);

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
l_test_fini(lua_State *L)
{
	luacs_delenum(L, "COLOR");
	return (0);
}
