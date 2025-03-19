#define ASSERT(_L, _cond)		\
	if (!(_cond))			\
		luaL_error((_L), "ASSERT(%s) failed", #_cond);
