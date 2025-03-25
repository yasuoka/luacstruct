Sample program: getaddrinfo
===========================

```lua
local socket = require("socket");

hints = socket.addrinfo()

hints.ai_family = socket.AF_UNSPEC
hints.ai_flags = socket.AI_NUMERICSERV
hints.ai_socktype = socket.SOCK_STREAM
hints.ai_protocol = socket.IPPROTO_TCP

local ai0 = socket.getaddrinfo("www.google.com", "443", hints)
local ai = ai0
while ai ~= nil do
    if ai.ai_addr.sa_family == socket.AF_INET then
        print(ai.ai_addr.sin4.sin_addr, ai.ai_addr.sin4.sin_port)
    elseif ai.ai_addr.sa_family == socket.AF_INET6 then
        print(ai.ai_addr.sin6.sin6_addr, ai.ai_addr.sin6.sin6_port)
    end
    ai = ai.ai_next
end
socket.freeaddrinfo(ai0)
ai0 = nil
```

```shell
$ lua51 test.lua
142.250.207.36  443
2404:6800:4004:818::2004        443
$
```
