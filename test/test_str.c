#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include "luacstruct.h"

#include "test_subr.h"

struct test_str {
	char		 name[128];
	char		*nameptr;
};

static int	 test_str_types(lua_State *);
static int	 test_str_new(lua_State *);
static int	 test_str_test(lua_State *);

static volatile int	test_str_initialized = 0;

int
luaopen_test_str(lua_State *L)
{
	if (test_str_initialized++ == 0)
		test_str_types(L);
	lua_newtable(L);

	lua_pushcfunction(L, test_str_new);
	lua_setfield(L, -2, "test_str");

	lua_pushcfunction(L, test_str_test);
	lua_setfield(L, -2, "test");

	return (1);
}

int
test_str_new(lua_State *L)
{
	struct test_str	*ptr;

	luacs_newobject(L, "test_str", NULL);
	ptr = luacs_object_pointer(L, -1, "test_str");
	if (ptr != NULL)
		ptr->nameptr = ptr->name;

	return (1);
}

int
test_str_types(lua_State *L)
{
	luacs_newstruct(L, test_str);

	luacs_string_field(L, test_str, name, 0);
	luacs_strptr_field(L, test_str, nameptr, 0);

	lua_pop(L, 1);

	return (0);
}

int
test_str_test(lua_State *L)
{
	struct test_str		*str;

	str = luacs_object_pointer(L, 1, "test_str");
	lua_pushvalue(L, 1);
	lua_pushstring(L, "Hello world");
	lua_setfield(L, 1, "name");

	ASSERT(L, strcmp(str->name, "Hello world") == 0);

	return (0);
}
