--local bit = require("bit")
local socket = require("socket");

local hints, ai, ai0

hints = socket.addrinfo()

hints.ai_family = socket.AF_UNSPEC
hints.ai_flags = socket.AI_NUMERICSERV
hints.ai_socktype = socket.SOCK_STREAM
hints.ai_protocol = socket.IPPROTO_TCP

ai0 = socket.getaddrinfo("www.google.com", "443", hints)
ai = ai0
while ai ~= nil do
    if ai.ai_addr.sa_family == socket.AF_INET then
	print(ai.ai_addr.sin4.sin_addr, ai.ai_addr.sin4.sin_port)
    elseif ai.ai_addr.sa_family == socket.AF_INET6 then
	print(ai.ai_addr.sin6.sin6_addr, ai.ai_addr.sin6.sin6_port)
    end
    ai = ai.ai_next
end
socket.freeaddrinfo(ai0)
