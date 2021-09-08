# LUACSTRUCT - Map C struct to Lua

A simple C source code which provides a lua metatable representing a C
struct, can be embedded within your application.  Also it enables
controlling access to the fields of the struct.

  * [How to use](#how-to-use)
    * [1. Prerequisite](#1-prerequisite)
    * [2. Prepare](#2-prepare)
    * [3. Declare a C struct in your application](#3-declare-a-c-struct-in-your-application)
    * [4. Map an instance of the struct](#4-map-an-instance-of-the-struct)
    * [5. Declare a enum in your application](#5-declare-a-enum-in-your-application)

## How to use

### 1. Prerequisite

This document assumes that Lua is embed already in your application.


### 2. Prepare

```c
#include "yourapp_types.h"          // 3.

#include <lua.h>                    // 1.
#include "luacstruct.h"             // 2.
```

0. copy `luacstruct.h` and `luacstruct.c` to your application, and add
   `luacstruct.c` in SRCS of Makefile of your application.
   - luacstruct.c is using `<sys/tree.h>` and `<sys/queue.h>`.  Copy
     them if your OS doesn't have any of them.
1. open a source file which is doing the Lua thing.
2. include "luacstruct.h".
3. make sure all headers which declare the structs or enums which you
   want to expose are included.

### 3. Declare a C struct in your application

```c
// create a new luacstruct for "struct yourapp_type"
luacs_newstruct(L, yourapp_type);	// don't surround "..."

// member fields
luacs_int_field(L, yourapp_type, field1, 0);  // `field1' field
luacs_int_field(L, yourapp_type, field2, 0);  // `field2' field
luacs_strptr_field(L, yourapp_type, name, 0); // `name' field

lua_pop(L, 1);		              // pop the luacstruct
```

To expose a C structure, create a new `luacstruct` object for a C struct
which you want to expose, and declare the fields of the struct to the
`luacstruct` object.

`luacs_newstruct()` is the macro function to create a new `luacstruct`
object.  If there is a struct object which has the same name, the
existing object is used instead of creating new one.  The function
pushes the object on the stack.  

`luacs_declare_field()` is the function to declare fields.  Usually this
function is not used directly, used through the helper macro functions
as follows:

```c
#define luacs_int_field(_L, _type, _field, _flags)              // int
#define luacs_unsigned_field(_L, _type, _field, _flags)         // unsigned int
#define luacs_enum_field(_L, _type, _etype, _field, _flags)     // enum
#define luacs_bool_field(_L, _type, _field, _flags)             // bool
#define luacs_bytearray_field(_L, _type, _field, _flags)        // byterray
#define luacs_string_field(_L, _type, _field, _flags)           // char[] represents a string
#define luacs_strptr_field(_L, _type, _field, _flags)           // a pointer to a string
#define luacs_objref_field(_L, _type, _tname, _field, _flags)   // a pointer to an instance of the
                                                                // _tname struct
#define luacs_nested_field(_L, _type, _tname, _field, _flags)   // nested  _tname struct
#define luacs_extref_field(_L, _type, _field, _flags)           // can be set an external lua
                                                                // object manually
#define luacs_pseudo_field(_L, _type, _field, _flags)           // for a pseudo field.  can be set
                                                                // any lua object manually
```

The meaning of the arguments is as follows:

|Argument |Description|
|---------|-----------|
|`_L`     |Specify a lua state by a pointer of `lua_State`|
|`_type`  |Specify a struct name|
|`_field` |Specify a field name in the struct|
|`_flags` |Bit flags.  Specify `LUACS_FREADONLY` if the field must be read only|
|`_tname` |Specify a type name of the field.  Only for `luacs_objref_field()` or `luacs_objref_field()`|
|`_etype` |Specify a enum type.  Only for `luacs_enum_field()`|


### 4. Map an instance of the struct

```c
int
youapp_do_script(struct youapp_type *self)
{
	lua_getglobal(L, "yourfunction");
	luacs_newobject(L, "yourapp_type", self);	// here
	lua_call(L, 1, 0);
}
```

To map an instance of the sctruct, the struct must be declared already
with a name.  Then call `luacs_newobject()` with the name to create a
mapped Lua object.

On Lua, you can use it naturally.

```lua
yourfunction = function(self)
	print(self.name, self.field1, self.field2)
	self.field1 = 5
	for k, v in pairs(self) do
		print(k, v)
	end
end
```

As for `pairs()`, `pairs()` will work as it is on Lua 5.2 and later
since `__pairs()` meta method is provided for objects created by
`luacs_newobject()`.  For Lua 5.1, you need to override the global
`pairs()` function like follows:

```lua
if _VERSION == "Lua 5.1" then
	_pairs = pairs
	pairs = function(t)
	    local mt = getmetatable(t)
	    if mt and mt.__pairs then
		return mt.__pairs(t)
	    end
	    return _pairs(t)
	end
end
```

### 5. Declare a enum in your application

```c
/*
 * Declare the following enum
 * enum yourapp_color {
 *     YOURAPP_RED,
 *     YOURAPP_GREEN,
 *     YOURAPP_BLUE
 * }
 */

// create a new luacstruct for "struct yourapp_type"
luacs_newenum(L, yourapp_color);	      // don't surround ".."

luacs_enum_declare_value(L, "RED",   YOURAPP_RED);
luacs_enum_declare_value(L, "GREEN", YOURAPP_GREEN);
luacs_enum_declare_value(L, "BLUE",  YOURAPP_BLUE);

lua_pop(L, 1);		              // pop the enum
```

To expose an enum, create a new `luacenum` object for an enum which you
want to expose, and declare the values of the enum for the `luacenum`
object.

`luacs_newenum()` is the function to create a new `luacenum` object.  If
there is an enum object which has the same name, the existing object is
used instead of creating new one.  The function pushes the object on the
stack.  

`luacs_enum_declare_value()` macro function is used to declare the enum
values.

After the enum is declared, it can be used when declaring a enum field
for a ``luacstruct`:

```c
/*
 * Use it in the following struct
 *
 * struct yourapp_type2 {
 *     enum yourapp_color color
 *     :
 * }
 */
luacs_newstruct(L, yourapp_type2);

luacs_enum_field(L, yourapp_type2, yourapp_color, color, 0);	
    :
lua_pop(L, 1);		              // pop the struct
```

Also the `luacenum` object can be expose as a set of constants:
```c
luacs_newenum(L, yourapp_color);
lua_setglobal(L, "COLOR");
```
You can use it in Lua:
```lua
local color = COLOR.RED		-- or COLOR.BLUE, COLOR.GREEN
```

Also it has some useful methods:
```lua
color = COLOR.get(1)		-- get COLOR object by an integer
COLOR.memberof(obj)		-- check whether an object is COLOR
local ival = color.tointeger()	-- get an integer value
print(color)			-- pretty output like "GREEN(1)"
```
