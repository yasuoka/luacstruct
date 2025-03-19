---
--- str
---
local test_str = require("test_str")

local p = test_str.test_str()
p.name = "Hello"
assert(p.name == "Hello")
assert(p.nameptr == "Hello")
-- set short string.
p.name = "Hi"
assert(p.name == "Hi")
assert(#p.name == 2)

test_str.test(p)	-- test in C

-- name field size is 128
ret, err = pcall(function() p.name = string.rep("A", 128) end)
assert(ret)
assert(#p.name == 128)
p = nil

---
--- wstr
---
local test_wstr = require("test_wstr")

local p = test_wstr.test_wstr()
p.name = "こんにちわ"
assert(p.name == "こんにちわ")
assert(p.nameptr == "こんにちわ")

test_wstr.test(p)	-- test in C

-- name field size is 128
ret, err = pcall(function() p.name = string.rep("A", 128) end)
assert(ret)
assert(#p.name == 128)
p = nil
