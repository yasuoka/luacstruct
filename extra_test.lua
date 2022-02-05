require("extra")

local main = function()
	--
	-- reference to another instance
	--
	ref = test_ref()
	assert(ref)
	assert(ref.a == 1)
	assert(ref.b == 2)
	assert(ref.ref)
	assert(ref.ref.x == 3)
	assert(ref.ref.y == 4)
	--
	-- checks whether pairs() works
	--
	local n = 0
	for k, v in pairs(ref) do
		assert(ref[k] == v)
		n = n + 1
	end
	assert(n == 3)
	--
	-- nested structure
	--
	nest = test_nest()
	assert(nest)
	assert(nest.a == 1)
	assert(nest.b == 2)
	assert(nest.nest)
	assert(nest.nest.x == 3)
	assert(nest.nest.y == 4)
	function try()
	    nest.nest = ref
	end
	local ok, err = pcall(try)
	assert(not ok)
	--
	-- ext
	--
	ext = test_ext()
	assert(ext)
	assert(ext.a == 1)
	assert(ext.b == 2)
	-- ext.ref can be written
	function try()
	    ext.ref = nest
	end
	local ok, err = pcall(try)
	assert(ok)
	assert(ext.ref.a == 1)
	assert(ext.ref.b == 2)
	ext.extras = {}
	ext.extras.counter = ext.extras.counter or 1
	ext.extras.counter = ext.extras.counter + 1
	assert(ext.extras)
	assert(ext.extras.counter == 2)

	color,m = test_enum()
	assert(m.color == color.BLUE)
	m.color = 1
	assert(m.color == color.GREEN)	-- 1 is GREEN
	-- assigning undefined integer is not allowed
	local r,e = pcall(function()
	    m.color = 101
	end)
	assert(not r)
	-- but C can assign an invalid value
	assert(m.invalid_color)
	-- we can use as a number
	assert(type(m.invalid_color) == "number")
	-- check iteratable
	iter = pairs(color)
	k, v = iter(color, nil)
	assert(k == "RED")
	k, v = iter(color, k)
	assert(k == "GREEN")
	k, v = iter(color, k)
	assert(k == "BLUE")
	k, v = iter(color, k)
	assert(k == nil)
	-- extra functions
	assert(color.get(1) == color.GREEN)
	assert(color.memberof(color.GREEN))
	assert(color.GREEN:tointeger() == 1)

	--print(color)
	--print(color.RED)
	--print(color.GREEN)
	--print(color.BLUE)
	--color = test_enum()

	f1,f2,m1,m2 = test_copy()

	f1.pseudo = 100
	f2.pseudo = 101

	-- copying of the same type is ok
	m1.fuga1 = f1
	assert(m1.fuga1.id == f1.id)
	-- since they are different instance
	assert(m1.fuga1 ~= f1)
	m1.fuga1.id = 100
	assert(m1.fuga1.id ~= f1.id)
	-- copying of a different type should be refused
	local r,e = pcall(function()
	    m1.fuga1 = m2
	end)
	-- copy pseudo field as well */
	assert(m1.fuga1.pseudo == f1.pseudo)
	m1.fuga1.pseudo = 200
	assert(m1.fuga1.pseudo ~= f1.pseudo)

	-- assigning of the same type is ok
	m1.fuga2 = f2
	assert(m1.fuga2.id == f2.id)
	-- this must be a reference
	assert(m1.fuga2 == f2)
	m1.fuga2.id = 100
	assert(f2.id == 100)
	-- assigning different type must be refused
	local r, e = pcall(function()
	    m1.fuga2 = m2
	end)
	assert(not r)
	-- pseudo is ok
	assert(m1.fuga2.pseudo == f2.pseudo)
	m1.fuga1.pseudo = 201
	-- the same since fuga is copy
	assert(m1.fuga2.pseudo == f2.pseudo)

	local m1,m2,int8a,int8b = test_array()
	-- check __len
	assert(#m1.int4 == 4)
	assert(#m1.sub2 == 2)
	assert(#m1.sub3 == 3)
	-- check __index
	assert(m1.int4[1] == 1)
	assert(m1.int4[2] == 2)
	assert(m1.int4[3] == 3)
	assert(m1.int4[4] == 4)
	assert(m1.sub2[1].x == 1)
	assert(m1.sub2[1].y == 2)
	assert(m1.sub2[1].z == 3)
	assert(m1.sub2[2].x == 11)
	assert(m1.sub2[2].y == 12)
	assert(m1.sub2[2].z == 13)
	assert(m1.sub3[1].x == 21)
	assert(m1.sub3[1].y == 22)
	assert(m1.sub3[1].z == 23)
	assert(m1.sub3[2].x == 31)
	assert(m1.sub3[2].y == 32)
	assert(m1.sub3[2].z == 33)
	assert(m1.sub3[3].x == 41)
	assert(m1.sub3[3].y == 42)
	assert(m1.sub3[3].z == 43)
	-- __newindex primitive
	m1.int4[1] = 9
	m1.int4[2] = 8
	m1.int4[3] = 7
	m1.int4[4] = 6
	assert(m1.int4[1] == 9)
	assert(m1.int4[2] == 8)
	assert(m1.int4[3] == 7)
	assert(m1.int4[4] == 6)
	-- __newindex objref / objent
	m1.sub2[1] = m1.sub3[1]
	assert(m1.sub2[1].x == 21)
	assert(m1.sub2[1].y == 22)
	assert(m1.sub2[1].z == 23)
	-- sub2 must be a reference
	m1.sub3[1].x = 999
	assert(m1.sub2[1].x == 999)
	assert(m1.sub2[1] ==  m1.sub3[1])
	-- sub3 must be an entity
	m1.sub3[2] = m1.sub3[3]
	assert(m1.sub3[2].x == 41)
	assert(m1.sub3[2].y == 42)
	assert(m1.sub3[2].z == 43)
	m1.sub3[2].x = 999
	assert(m1.sub3[2].x == 999)
	assert(m1.sub3[3].x == 41)

	-- assign array
	m2.int4 = m1.int4
	assert(m2.int4[1] == 9)
	assert(m2.int4[2] == 8)
	assert(m2.int4[3] == 7)
	assert(m2.int4[4] == 6)

	m2.sub2 = m1.sub2
	-- sub2 is refereence
	assert(m2.sub2 ~= m1.sub2)
	assert(m2.sub2[1] == m1.sub2[1])
	assert(m2.sub2[2] == m1.sub2[2])
	assert(m2.sub2[1].x == m1.sub2[1].x)
	assert(m2.sub2[1].y == m1.sub2[1].y)
	assert(m2.sub2[1].z == m1.sub2[1].z)
	assert(m2.sub2[2].x == m1.sub2[2].x)
	assert(m2.sub2[2].y == m1.sub2[2].y)
	assert(m2.sub2[2].z == m1.sub2[2].z)
	m1.sub2[1].x = 1234
	assert(m2.sub2[1].x == 1234)

	m2.sub3 = m1.sub3
	-- sub3 is an entity
	assert(m2.sub3[1] ~= m1.sub3[1])
	assert(m2.sub3[2] ~= m1.sub3[2])
	assert(m2.sub3[3] ~= m1.sub3[3])
	assert(m2.sub3[1].x == m1.sub3[1].x)
	assert(m2.sub3[1].y == m1.sub3[1].y)
	assert(m2.sub3[1].z == m1.sub3[1].z)
	m1.sub3[2].z = 100
	m2.sub3[2].z = 101
	assert(m1.sub3[2].z == 100)
	assert(m2.sub3[2].z == 101)

	m1.ext2[1] = "hello"
	m1.ext2[2] = "world"
	m2.ext2 = m1.ext2
	assert(m1.ext2[1] == m2.ext2[1])
	assert(m1.ext2[2] == m2.ext2[2])
	assert(m2.ext2[1] == "hello")
	assert(m2.ext2[2] == "world")

	assert(#int8a == 8);
	assert(#int8b == 8);
	for i=1,#int8a do
		int8b[i] = 2 * i
	end
	-- bare array objects
	int8a = int8b
	assert(int8a[1] == 2)
	assert(int8a[8] == 16)
	-- iteration
	local i = 1
	for k,v in pairs(int8a) do
		assert(k == i)
		assert(v == i * 2)
		i = i + 1
	end
	for i,v in ipairs(int8a) do
		assert(v == i * 2)
		i = i + 1
	end

	yamada = test_tostring_const()
	assert(tostring(yamada) == "yamada(168,63)")
	assert(22.0 <= yamada:bmi() and yamada:bmi() < 23.0)
	assert(typename(yamada) == "person")
	rv = pcall(function() yamada.bmi = function() return 21 end end)
	assert(not rv)
	-- test const
	yamada.country = yamada.JAPAN
	assert(yamada.country == 81)
	assert(yamada.country == yamada.JAPAN)
	assert(yamada.UNITED_STATES == 1)
	rv = pcall(function() yamada.UNITED_STATES = 81 end)
	assert(not rv)
end
if _VERSION == "Lua 5.1" then
	local _pairs = pairs
	pairs = function(t)
	    local mt = getmetatable(t)
	    if mt and mt.__pairs then
		return mt.__pairs(t)
	    end
	    return _pairs(t)
	end
end
if _VERSION == "Lua 5.1" or _VERSION == "Lua 5.2" then
	local _pairs = ipairs
	local _next = function(t,i)
		i = i or 0
		i = i + 1
		if i <= #t then
			return i, t[i]
		end
		return nil
	end
	ipairs = function(t)
		if type(t) ~= "table" then
			return _next, t, nil
		end
		return _pairs(t)
	end
end
main()
