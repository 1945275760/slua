// Copyright (c) 2015  Phil Leblanc  -- see LICENSE file
// ---------------------------------------------------------------------
/* 

mtcp - A minimal Lua socket library for tcp connections

functions:
  bind       -- include listen
  accept
  connect
  write
  read       -- with a timeout
  close
  getpeername

the sockaddr structure, returned as a string by getpeername, 
accept and connect (second return value), can easily be parsed with
string.unpack:  eg. for a IPv4 address:
  family, port, ip1, ip2, ip3, ip4 = string.unpack("<H>HBBBB", addr)
  ipaddr = table.concat({ip1, ip2, ip3, ip4}, '.')

*/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define MTCP_VERSION "0.1"
#define BUFSIZE 1024
#define BACKLOG 32

// default timeout: 10 seconds
#define DEFAULT_TIMEOUT 10000


int mtcp_bind(lua_State *L) {
	// create a server socket, bind, then listen 
	// Lua args: host, service (as strings)
	// returns server socket file descriptor (as integer) or nil, errmsg
	const char *host, *service;
	struct addrinfo hints;
	struct addrinfo *result, *rp;	
	int sfd;
	int n;
	
	host = luaL_checkstring(L, 1);
	service = luaL_checkstring(L, 2);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;	 /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; 
	n = getaddrinfo(host, service, &hints, &result);
	if (n) {luaL_error(L, "getaddrinfo (%s) %s", host, gai_strerror(n));}
	
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) { continue; }
		n = bind(sfd, rp->ai_addr, rp->ai_addrlen);
		if (n == 0) break;
		close(sfd);
	}	
	if (rp == NULL) {  /* No address succeeded */
		lua_pushnil (L);
		lua_pushfstring (L, "bind error: %d  %d", n, errno);
		return 2;      
	}
	n = listen(sfd, BACKLOG);
	if (n) {
		lua_pushnil (L);
		lua_pushfstring (L, "listen error: %d  %d", n, errno);
		return 2;      
	}
	// success, return server socket fd
	lua_pushinteger (L, sfd);
	lua_pushlstring(L, (const char *)rp->ai_addr, rp->ai_addrlen);
	return 2;
} //mtcp_bind

int mtcp_accept(lua_State *L) {
	// accept incoming connections on a server socket 
	// Lua args: server socket file descriptor (as integer)
	// returns client socket file descriptor (as integer) and
	// the raw client address as a string,  or nil, errmsg
	int cfd, sfd;
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr); //enough for ip4&6 addr

	sfd = luaL_checkinteger(L, 1); // get server socket fd

	cfd = accept(sfd, (struct sockaddr *)&addr, &len);
	if (cfd == -1) {
		lua_pushnil (L);
		lua_pushfstring (L, "accept error: %d", errno);
		return 2;
	}
	//success, return client socket fd
	lua_pushinteger (L, cfd);
	lua_pushlstring(L, (const char *)&addr, len);
	return 2;
} //mtcp_accept


int mtcp_connect(lua_State *L) {
	// connect to a host
	// Lua args: host, service or port (as strings)
	// returns connection socket fd (as integer) and host raw address 
	// as a string, or nil, errmsg
	const char *host, *service;
	struct addrinfo hints;
	struct addrinfo *result, *rp;	
	int cfd;
	int n;
	
	host = luaL_checkstring(L, 1);
	service = luaL_checkstring(L, 2);

	/* Obtain address(es) matching host/port */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;       /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; 
	n = getaddrinfo(host, service, &hints, &result);
	if (n) {
		luaL_error(L, "getaddrinfo (%s) %s", host, gai_strerror(n));
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		cfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (cfd == -1) { continue; }
		n = connect(cfd, rp->ai_addr, rp->ai_addrlen);
		if (n == 0) break;
		close(cfd);
	}	
	if (rp == NULL) {  /* No address succeeded */
		lua_pushnil (L);
		lua_pushfstring (L, "connect error: %d  %d", n, errno);
		return 2;      
	}
	//success, return connection socket fd
	lua_pushinteger (L, cfd);
	lua_pushlstring(L, (const char *)rp->ai_addr, rp->ai_addrlen);
	return 2;
} //mtcp_connect

int mtcp_write(lua_State *L) {
	// Lua args:
	//   fd: integer - socket descriptor
	//   s: string - string to send
	//   idx: integer - 
	//   sbytes: integer - number of bytes to send 
	//		(adjusted to idx and string length if too large)
	int fd;
	int n;
	const char *s;
	size_t slen;
	int idx, sbytes;

	fd = luaL_checkinteger(L, 1); // get socket fd
	s = luaL_checklstring(L, 2, &slen); // string to write
	idx = luaL_optinteger(L, 3, 1); // starting index in string
	sbytes = luaL_optinteger(L, 4, 0); // number of bytes to write
	
	if (idx > slen) {  
		luaL_error(L, "write: idx (%d) too large", idx);
	}
	if (sbytes == 0) { sbytes = slen; }
	if (idx + sbytes -1 > slen) { sbytes = slen - idx + 1; }
	n = write(fd, s+idx-1, sbytes);
	if (n < 0) {  // write error
		lua_pushnil (L);
		lua_pushfstring (L, "write error: %d  %d", n, errno);
		return 2;      
	}
	//success, return number of bytes sent
	lua_pushinteger (L, n);
	return 1;
} //mtcp_write

int mtcp_read(lua_State *L) {
	// read bytes from a socket file descriptor
	// Lua args:  
	//    socket fd: integer
	//    nbytes: integer - max number of bytes to read 
	//              (optional - defaults to BUFSIZE)
	//    timeout: integer (in milliseconds)
	//				(optional - defaults to DEFAULT_TIMEOUT)
	// reading stops on error, on timeout, when at least nbytes bytes 
	// have been read, or when last read() returned less than BUFSIZE.
	//
	int fd;
	int n;
	luaL_Buffer b;
	char *buf;
	int bufln;
	struct pollfd pfd;
	int timeout;
	int nbytes, rbytes;

	fd = luaL_checkinteger(L, 1);
	nbytes = luaL_optinteger(L, 2, BUFSIZE); 
	timeout = luaL_optinteger(L, 3, DEFAULT_TIMEOUT); 

	bufln = BUFSIZE;
	buf = malloc(bufln);
	luaL_buffinit(L,&b);
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	rbytes = 0; // total number of bytes read
	while(1) {
		n = poll(&pfd, (nfds_t) 1, timeout);
		if (n < 0) {  // poll error
			free(buf);
			lua_pushnil (L);
			lua_pushfstring (L, "poll error: %d  %d", n, errno);
			return 2;      
		}
		if (n == 0) { // poll timeout
			free(buf);
			lua_pushnil (L);
			lua_pushfstring (L, "poll timeout");
			return 2;      
		}
		n = read(fd, buf, BUFSIZE);
		if (n == 0) { // nothing read
			break;
		}
		if (n < 0) {  // read error
			free(buf);
			lua_pushnil (L);
			lua_pushfstring (L, "read error: %d  %d", n, errno);
			return 2;      
		}		
		luaL_addlstring(&b, buf, n);
		rbytes += n;
		if (n < BUFSIZE) break;
		if (rbytes >= nbytes) break;
	}
	//success, return bytes read
	free(buf);
	luaL_pushresult(&b);
	return 1;
} //mtcp_read

int mtcp_close(lua_State *L) {
	// close a socket 
	// Lua args: socket file descriptor (as integer)
	// returns true on success or nil, errmsg
	int fd;
	int n;

	fd = luaL_checkinteger(L, 1); // get socket fd

	n = close(fd);
	if (n == -1) {
		lua_pushnil (L);
		lua_pushfstring (L, "close error: %d", errno);
		return 2;
	}
	//success, return true
	lua_pushboolean (L, 1);
	return 1;
} //mtcp_close

int mtcp_getpeername(lua_State *L) {
	
	int fd;
	int n;
	struct sockaddr addr;
	socklen_t len = sizeof(addr); //enough for ip4&6 addr
	
	fd = luaL_checkinteger(L, 1); // get socket fd

	n = getpeername(fd, &addr, &len);
	if (n == -1) {
		lua_pushnil (L);
		lua_pushfstring (L, "close error: %d", errno);
		return 2;
	}
	//success, return peer socket addr
	lua_pushlstring (L, (const char *)&addr, len);
	return 1;
} //mtcp_getpeername


static const struct luaL_Reg mtcplib[] = {
	{"bind", mtcp_bind},
	{"accept", mtcp_accept},
	{"connect", mtcp_connect},
	{"write", mtcp_write},
	{"read", mtcp_read},
	{"close", mtcp_close},
	{"getpeername", mtcp_getpeername},
	
	{NULL, NULL},
};


int luaopen_mtcp (lua_State *L) {
	luaL_newlib (L, mtcplib);
    // 
    lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, MTCP_VERSION); 
	lua_settable (L, -3);
	return 1;
}

