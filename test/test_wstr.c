
#include <locale.h>
#include <wchar.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include "luacstruct.h"

#include "test_subr.h"

struct test_wstr {
	wchar_t		 name[128];
	wchar_t		*nameptr;
};

static int	 test_wstr_types(lua_State *);
static int	 test_wstr_new(lua_State *);
static int	 test_wstr_test(lua_State *);

static volatile int	test_wstr_initialized = 0;

EXPORT
int
luaopen_test_wstr(lua_State *L)
{
	if (test_wstr_initialized++ == 0) {
		setlocale(LC_ALL, "");
		test_wstr_types(L);
	}
	lua_newtable(L);

	lua_pushcfunction(L, test_wstr_new);
	lua_setfield(L, -2, "test_wstr");

	lua_pushcfunction(L, test_wstr_test);
	lua_setfield(L, -2, "test");

	return (1);
}

int
test_wstr_new(lua_State *L)
{
	struct test_wstr	*ptr;

	luacs_newobject(L, "test_wstr", NULL);
	ptr = luacs_object_pointer(L, -1, "test_wstr");
	if (ptr != NULL)
		ptr->nameptr = ptr->name;

	return (1);
}

int
test_wstr_types(lua_State *L)
{
	luacs_newstruct(L, test_wstr);

	luacs_wstring_field(L, test_wstr, name, 0);
	luacs_wstrptr_field(L, test_wstr, nameptr, 0);

	lua_pop(L, 1);

	return (0);
}

int
test_wstr_test(lua_State *L)
{
	struct luacobject	*obj;
	struct test_wstr	*wstr;

	lua_settop(L, 1);
	wstr = luacs_object_pointer(L, 1, "test_wstr");
	lua_pushvalue(L, 1);
	lua_pushstring(L, "こんにちわ世界");
	lua_setfield(L, 1, "name");

	ASSERT(L, wcscmp(wstr->name, L"こんにちわ世界") == 0);

	return (0);
}
