// Copyright (c) 2015  Phil Leblanc  -- see LICENSE file
// ---------------------------------------------------------------------
/* tweetnacl

A binding to the wonderful NaCl crypto library by Dan Bernstein,
Tanja Lange et al. -- http://nacl.cr.yp.to/

The version included here is the "Tweet" version ("NaCl in 100 tweets")
by Dan Bernstein et al. --  http://tweetnacl.cr.yp.to/index.html

160408 
- removed the ill-designed, "easier" functions - stick with the original api

150721
- split luazen and tweetnacl.  
- nacl lua interface is in this file (luatweetnacl.c)

TweetNaCl version 20140427 - loaded on 150630 from
http://tweetnacl.cr.yp.to/index.html
includes: tweetnacl.c, tweetnacl.h

150630
- addition of the tweetnacl binding to luazen
  based on tweetnacl version 20140427 - loaded on 150630 from
  http://tweetnacl.cr.yp.to/index.html
  includes: tweetnacl.c, tweetnacl.h

  randombytes()  not included in tweetnacl. Got it from
  https://hyperelliptic.org/nacl/nacl-20110221.tar.bz2
  (Tanja Lange site)

NaCl specs: http://nacl.cr.yp.to/

*/

#define TWEETNACL_VERSION "tweetnacl-0.2"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "tweetnacl.h"


//=========================================================
// compatibility with Lua 5.2  --and lua 5.3, added 150621
// (from roberto's lpeg 0.10.1 dated 101203)
//
#if (LUA_VERSION_NUM >= 502)

#undef lua_equal
#define lua_equal(L,idx1,idx2)  lua_compare(L,(idx1),(idx2),LUA_OPEQ)

#undef lua_getfenv
#define lua_getfenv	lua_getuservalue
#undef lua_setfenv
#define lua_setfenv	lua_setuservalue

#undef lua_objlen
#define lua_objlen	lua_rawlen

#undef luaL_register
#define luaL_register(L,n,f) \
	{ if ((n) == NULL) luaL_setfuncs(L,f,0); else luaL_newlib(L,f); }

#endif
//=========================================================

# define LERR(msg) return luaL_error(L, msg)

typedef unsigned char u8;
typedef unsigned long u32;
typedef unsigned long long u64;

//------------------------------------------------------------
// nacl functions (the "tweetnacl version")

extern void randombytes(unsigned char *x,unsigned long long xlen); 


static int tw_randombytes(lua_State *L) {
	
    size_t bufln; 
	lua_Integer li = luaL_checkinteger(L, 1);  // 1st arg
	bufln = (size_t) li;
    unsigned char *buf = malloc(bufln); 
	randombytes(buf, li);
    lua_pushlstring (L, buf, bufln); 
    free(buf);
	return 1;
}//randombytes()

static int tw_box_keypair(lua_State *L) {
	// generate and return a random key pair (pk, sk)
	unsigned char pk[crypto_box_PUBLICKEYBYTES];
	unsigned char sk[crypto_box_SECRETKEYBYTES];
	int r = crypto_box_keypair(pk, sk);
	lua_pushlstring (L, pk, crypto_box_PUBLICKEYBYTES); 
	lua_pushlstring (L, sk, crypto_box_SECRETKEYBYTES); 
	return 2;
}//box_keypair()

static int tw_box_getpk(lua_State *L) {
	// return the public key associated to a secret key
	size_t skln;
	unsigned char pk[crypto_box_PUBLICKEYBYTES];
	const char *sk = luaL_checklstring(L,1,&skln); // secret key
	if (skln != crypto_box_SECRETKEYBYTES) LERR("bad sk size");
	int r = crypto_scalarmult_base(pk, sk);
	lua_pushlstring (L, pk, crypto_box_PUBLICKEYBYTES); 
	return 1;
}//box_getpk()

static int tw_box(lua_State *L) {
	size_t mln, nln, pkln, skln;
	const char *m = luaL_checklstring(L,1,&mln);   // plaintext
	const char *n = luaL_checklstring(L,2,&nln);   // nonce
	const char *pk = luaL_checklstring(L,3,&pkln); // public key
	const char *sk = luaL_checklstring(L,4,&skln); // secret key
	if (mln <= crypto_box_ZEROBYTES) LERR("box_open: mln <= ZEROBYTES");
	if (nln != crypto_box_NONCEBYTES) LERR("box_open: bad nonce size");
	if (pkln != crypto_box_PUBLICKEYBYTES) LERR("box_open: bad pk size");
	if (skln != crypto_box_SECRETKEYBYTES) LERR("box_open: bad sk size");
	unsigned char * buf = malloc(mln);
	int r = crypto_box(buf, m, mln, n, pk, sk);
	lua_pushlstring(L, buf, mln); 
	free(buf);
	return 1;   
}// box()

static int tw_box_open(lua_State *L) {
	char * msg = "box_open: argument error";
	size_t cln, nln, pkln, skln;
	const char *c = luaL_checklstring(L,1,&cln);	
	const char *n = luaL_checklstring(L,2,&nln);	
	const char *pk = luaL_checklstring(L,3,&pkln);	
	const char *sk = luaL_checklstring(L,4,&skln);	
	if (cln <= crypto_box_ZEROBYTES) LERR("box_open: cln <= ZEROBYTES");
	if (nln != crypto_box_NONCEBYTES) LERR("box_open: bad nonce size");
	if (pkln != crypto_box_PUBLICKEYBYTES) LERR("box_open: bad pk size");
	if (skln != crypto_box_SECRETKEYBYTES) LERR("box_open: bad sk size");
	unsigned char * buf = malloc(cln);
	int r = crypto_box_open(buf, c, cln, n, pk, sk);
	if (r != 0) { 
		free(buf); 
		lua_pushnil (L);
		lua_pushfstring(L, "box_open error %d", r);
		return 2;         
	} 
	lua_pushlstring (L, buf, cln); 
	free(buf);
	return 1;
} // box_open()

static int tw_box_beforenm(lua_State *L) {
	int r;
	size_t pkln, skln;
	u8 k[32];
	const char *pk = luaL_checklstring(L,1,&pkln); // dest public key
	const char *sk = luaL_checklstring(L,2,&skln); // src secret key
	if (pkln != crypto_box_PUBLICKEYBYTES) LERR("box_beforenm: bad pk size");
	if (skln != crypto_box_SECRETKEYBYTES) LERR("box_beforenm: bad sk size");
	r = crypto_box_beforenm(k, pk, sk);
	lua_pushlstring(L, k, 32); 
	return 1;   
}// box()

static int tw_secretbox(lua_State *L) {
	int r;
	size_t mln, nln, kln;
	const char *m = luaL_checklstring(L,1,&mln);	
	const char *n = luaL_checklstring(L,2,&nln);	
	const char *k = luaL_checklstring(L,3,&kln);	
	if (mln <= crypto_box_ZEROBYTES) LERR("secretbox: mln <= ZEROBYTES");
	if (nln != crypto_box_NONCEBYTES) LERR("secretbox: bad nonce size");
	if (kln != crypto_secretbox_KEYBYTES) LERR("secretbox: bad key size");
	unsigned char * buf = malloc(mln);
	r = crypto_secretbox(buf, m, mln, n, k);
	lua_pushlstring (L, buf, mln); 
	free(buf);
	return 1;
} // secretbox()

static int tw_secretbox_open(lua_State *L) {
	int r = 0;
	size_t cln, nln, kln;
	const char *c = luaL_checklstring(L,1,&cln);	
	const char *n = luaL_checklstring(L,2,&nln);	
	const char *k = luaL_checklstring(L,3,&kln);	
	if (cln <= crypto_box_ZEROBYTES) LERR("secretbox_open: cln <= ZEROBYTES");
	if (nln != crypto_box_NONCEBYTES) LERR("secretbox_open: bad nonce size");
	if (kln != crypto_secretbox_KEYBYTES) LERR("secretbox_open: bad key size");
	unsigned char * buf = malloc(cln);
	r = crypto_secretbox_open(buf, c, cln, n, k);
	if (r != 0) { 
		free(buf); 
		lua_pushnil (L);
		lua_pushfstring(L, "secretbox_open error %d", r);
		return 2;         
	} 
	lua_pushlstring (L, buf, cln); 
	free(buf);
	return 1;
} // secretbox_open()

static int tw_stream(lua_State *L) {
	int r;
	size_t mln, nln, kln;
	mln = luaL_checkinteger(L,1);	
	const char *n = luaL_checklstring(L,2,&nln);	
	const char *k = luaL_checklstring(L,3,&kln);	
	// dont know if the zerobyte limit applies for stream()...?!?
	if (mln <= crypto_box_ZEROBYTES) LERR("msg length <= ZEROBYTES");
	if (nln != crypto_box_NONCEBYTES) LERR("bad nonce size");
	if (kln != crypto_secretbox_KEYBYTES) LERR("bad key size");
	unsigned char * buf = malloc(mln);
	r = crypto_stream(buf, mln, n, k);
	lua_pushlstring (L, buf, mln); 
	free(buf);
	return 1;
} // stream()

static int tw_stream_xor(lua_State *L) {
	int r;
	size_t mln, nln, kln;
	const char *m = luaL_checklstring(L,1,&mln);	
	const char *n = luaL_checklstring(L,2,&nln);	
	const char *k = luaL_checklstring(L,3,&kln);	
	if (mln <= crypto_box_ZEROBYTES) LERR("msg length <= ZEROBYTES");
	if (nln != crypto_box_NONCEBYTES) LERR("bad nonce size");
	if (kln != crypto_secretbox_KEYBYTES) LERR("bad key size");
	unsigned char * buf = malloc(mln);
	r = crypto_stream_xor(buf, m, mln, n, k);
	lua_pushlstring (L, buf, mln); 
	free(buf);
	return 1;
} // stream_xor()

static int tw_onetimeauth(lua_State *L) {
	// no leading zerobytes
	int r;
	u8 mac[16];
	size_t mln, kln;
	const char *m = luaL_checklstring(L,1,&mln);	
	const char *k = luaL_checklstring(L,2,&kln);	
	if (kln != crypto_secretbox_KEYBYTES) LERR("bad key size");
	r = crypto_onetimeauth(mac, m, mln, k);
    lua_pushlstring (L, mac, 16); 
    return 1;
}//onetimeauth()

// onetimeauth_verify - not implemented, very easy to do in Lua:
//      if onetimeauth(m, k) == mac then ...

static int tw_sha512(lua_State *L) {
    size_t sln; 
    const char *src = luaL_checklstring (L, 1, &sln);
    char digest[64];
	crypto_hash(digest, (const unsigned char *) src, (unsigned long long) sln);  
    lua_pushlstring (L, digest, 64); 
    return 1;
}

//-- sign functions (ed25519)
// sign_BYTES 64
// sign_PUBLICKEYBYTES 32
// sign_SECRETKEYBYTES 64

static int tw_sign_keypair(lua_State *L) {
	// generate and return a random key pair (pk, sk)
	unsigned char pk[32];
	unsigned char sk[64];
	int r = crypto_box_keypair(pk, sk);
	lua_pushlstring (L, pk, 32); 
	lua_pushlstring (L, sk, 64); 
	return 2;
}//sign_keypair()


static int tw_sign(lua_State *L) {
	int r;
	size_t mln, skln;
	const char *m = luaL_checklstring(L,1,&mln);   // text to sign
	const char *sk = luaL_checklstring(L,2,&skln); // secret key
	if (skln != 64) LERR("bad signature sk size");
	u64 usmln = mln + 64;
	unsigned char * buf = malloc(usmln);
	r = crypto_sign(buf, &usmln, m, mln, sk);
	lua_pushlstring(L, buf, usmln); 
	free(buf);
	return 1;   
}// sign()

static int tw_sign_open(lua_State *L) {
	int r;
	size_t smln, pkln;
	const char *sm = luaL_checklstring(L,1,&smln);   // signed text
	const char *pk = luaL_checklstring(L,2,&pkln);   // public key
	if (pkln != 32) LERR("bad signature pk size");
	unsigned char * buf = malloc(smln);
	u64 umln;
	r = crypto_sign(buf, &umln, sm, smln, pk);
	lua_pushlstring(L, buf, umln); 
	free(buf);
	return 1;   
}// sign_open()




//------------------------------------------------------------
// lua library declaration
//
static const struct luaL_Reg tweetnacllib[] = {
	// nacl functions
	{"randombytes", tw_randombytes},
	{"box", tw_box},
	{"box_open", tw_box_open},
	{"box_keypair", tw_box_keypair},
	{"box_getpk", tw_box_getpk},
	{"secretbox", tw_secretbox},
	{"secretbox_open", tw_secretbox_open},
	{"box_afternm", tw_secretbox},
	{"box_open_afternm", tw_secretbox_open},
	{"box_beforenm", tw_box_beforenm},
	{"box_stream_key", tw_box_beforenm}, // an alias for box_beforenm()
	{"onetimeauth", tw_onetimeauth},
	{"poly1305", tw_onetimeauth}, 
	{"hash", tw_sha512},
	{"sha512", tw_sha512}, 
	{"sign", tw_sign}, 
	{"sign_open", tw_sign_open}, 
	{"sign_keypair", tw_sign_keypair}, 
		
	{NULL, NULL},
};

int luaopen_tweetnacl (lua_State *L) {
	luaL_register (L, "tweetnacl", tweetnacllib);
    // 
    lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, TWEETNACL_VERSION); 
	lua_settable (L, -3);
	return 1;
}

