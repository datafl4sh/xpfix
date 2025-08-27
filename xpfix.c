/* X-Plane 11 fix for newer Linux kernels.
 *
 * ------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * datafl4sh@toxicnet.eu wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and
 * you think this stuff is worth it, you can buy me a beer in return.
 * Matteo Cicuttin
 * ------------------------------------------------------------------------
 *
 * X-Plane 11 gets stuck on "Will init net" on Linux kernels >= 6.9 and
 * this apparently happens because of netlink modifications.
 *
 * A quick `strace` shows that it is on a recvfrom on a netlink socket
 * where X-Plane gets stuck:
 *
 *   socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE) = 90
 *   getpid()                                = 1923236
 *   sendto(90, [{nlmsg_len=24, nlmsg_type=0x16  [cut]
 *   recvfrom(90, [[{nlmsg_len=72, nlmsg_type=RTM_NEWADDR, [cut]
 *   recvfrom(90,    <--gets stuck here
 *
 * The idea is to track all the socket() calls requesting an AF_NETLINK
 * socket and to make the subsequent sendto() fail if nlmsg_type is
 * equal to 0x16. This code does this in an absolutely not thread safe
 * way, therefore it could break at any time. I noticed that X-Plane
 * apparently keeps working correctly if we just return -1 to all the
 * socket(AF_NETLINK, ...) calls, so this could be an harsher but safer
 * way to work around the hang. Good luck.
 *
 * To compile:
 *
 *      gcc -O3 -shared -o xpfix.so xpfix.c
 *
 * To use: drop xpfix.so in the same directory of the X-Plane executable
 * and run with
 *
 *      LD_PRELOAD=./xpfix.so ./X-Plane-x86_64
 *
 * Changelog:
 * 20250528 - First version
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <asm/types.h>
#include <sys/socket.h>

#define MAX_SOCK 1024

/* Netlink header */
struct nlmsghdr {
      __u32   nlmsg_len;
      __u16   nlmsg_type;
      __u16   nlmsg_flags;
      __u32   nlmsg_seq;
      __u32   nlmsg_pid;
};

static int socks[MAX_SOCK];

void __attribute__((constructor)) init()
{
    // printf("[XPFIX] init\n");
    for (int i = 0; i < MAX_SOCK; i++)
        socks[i] = 0;
}

int socket(int domain, int type, int protocol)
{
    int (*real_socket)(int, int, int) = dlsym(RTLD_NEXT, "socket");

    int fd = real_socket(domain, type, protocol);

    if (domain != AF_NETLINK)
        return fd;

    printf("[XPFIX] fd %d is a netlink socket, monitoring\n", fd);

    if (fd >= 0 && fd < MAX_SOCK) {
        socks[fd] = 1;
    }

    return fd;
}

ssize_t sendto(int fd, const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen)
{
    int (*real_sendto)(int, const void *, size_t, int,
        const struct sockaddr *, socklen_t) = dlsym(RTLD_NEXT, "sendto");

    if ( len < sizeof(struct nlmsghdr) )
        return real_sendto(fd, buf, len, flags, dest_addr, addrlen);

    if (fd >= 0 && fd < MAX_SOCK && socks[fd]) {
        const struct nlmsghdr *hdr = buf;
        if (hdr->nlmsg_type == 0x16) {
            printf("[XPFIX] blocked type 0x16 netlink packet\n");
            return -1;
        }
    }

    return real_sendto(fd, buf, len, flags, dest_addr, addrlen);
}

ssize_t send(int fd, const void *buf, size_t len, int flags)
{
    return sendto(fd, buf, len, flags, 0, 0);
}

int close(int fd)
{
    int (*real_close)(int) = dlsym(RTLD_NEXT, "close");

    if (fd >= 0 && fd < MAX_SOCK) {
        if (socks[fd]) {
            printf("[XPFIX] fd %d closed\n", fd);
            socks[fd] = 0;
        }
    }

    return real_close(fd);
}
