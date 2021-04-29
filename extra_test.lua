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
end
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
main()
