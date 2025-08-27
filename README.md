X-Plane 11 gets stuck on "Will init net" on Linux kernels >= 6.9 and
this apparently happens because of netlink API modifications (don't break the user space they say...).

Normally this is worked around by starting X-Plane with `--disable_networking`,
but in that way you lose all the flight model data output to the network (for example).
With this workaround, X-Plane network data output capabilities are preserved.

A quick `strace` shows that it is on a `recvfrom()` on a netlink socket
where X-Plane gets stuck:

```
  socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE) = 90
  getpid()                                = 1923236
  sendto(90, [{nlmsg_len=24, nlmsg_type=0x16  [cut]
  recvfrom(90, [[{nlmsg_len=72, nlmsg_type=RTM_NEWADDR, [cut] 
  recvfrom(90,    <--gets stuck here
```

The idea is to track all the `socket()` calls requesting an AF_NETLINK
socket and to make the subsequent `sendto()` fail if `nlmsg_type` is
equal to 0x16. This code does this in an absolutely not thread safe
way, therefore it could break at any time. I noticed that X-Plane
apparently keeps working correctly if we just return -1 to all the
`socket(AF_NETLINK, ...)` calls, so this could be an harsher but safer
way to work around the hang. Good luck.

To compile:

```
     gcc -O3 -shared -o xpfix.so xpfix.c
```

To use: drop xpfix.so in the same directory of the X-Plane executable
and run with

```
     LD_PRELOAD=./xpfix.so ./X-Plane-x86_64
```

If you own XPlane via Steam, set the launch option to:
(it doesn't work with a space in the path)
```
    LD_PRELOAD="/path/to/xpfix.so" %command%
```
