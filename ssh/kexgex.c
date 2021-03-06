/* $OpenBSD: kexgex.c,v 1.27 2006/08/03 03:34:42 deraadt Exp $ */
/*
 * Copyright (c) 2000 Niels Provos.  All rights reserved.
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>

#include <openssl/evp.h>
#include <signal.h>

#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "ssh2.h"
#include "err.h"
#include "sshbuf.h"

int
kexgex_hash(
    const EVP_MD *evp_md,
    const char *client_version_string,
    const char *server_version_string,
    const u_char *ckexinit, size_t ckexinitlen,
    const u_char *skexinit, size_t skexinitlen,
    const u_char *serverhostkeyblob, size_t sbloblen,
    int min, int wantbits, int max,
    const BIGNUM *prime,
    const BIGNUM *gen,
    const BIGNUM *client_dh_pub,
    const BIGNUM *server_dh_pub,
    const BIGNUM *shared_secret,
    u_char **hash, size_t *hashlen)
{
	struct sshbuf *b;
	static u_char digest[EVP_MAX_MD_SIZE];
	EVP_MD_CTX md;
	int r;

	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_cstring(b, client_version_string)) != 0 ||
	    (r = sshbuf_put_cstring(b, server_version_string)) != 0 ||
	    /* kexinit messages: fake header: len+SSH2_MSG_KEXINIT */
	    (r = sshbuf_put_u32(b, ckexinitlen+1)) != 0 ||
	    (r = sshbuf_put_u8(b, SSH2_MSG_KEXINIT)) != 0 ||
	    (r = sshbuf_put(b, ckexinit, ckexinitlen)) != 0 ||
	    (r = sshbuf_put_u32(b, skexinitlen+1)) != 0 ||
	    (r = sshbuf_put_u8(b, SSH2_MSG_KEXINIT)) != 0 ||
	    (r = sshbuf_put(b, skexinit, skexinitlen)) != 0 ||
	    (r = sshbuf_put_string(b, serverhostkeyblob, sbloblen)) != 0 ||
	    (min != -1 && (r = sshbuf_put_u32(b, min)) != 0) ||
	    (r = sshbuf_put_u32(b, wantbits)) != 0 ||
	    (max != -1 && (r = sshbuf_put_u32(b, max)) != 0) ||
	    (r = sshbuf_put_bignum2(b, prime)) != 0 ||
	    (r = sshbuf_put_bignum2(b, gen)) != 0 ||
	    (r = sshbuf_put_bignum2(b, client_dh_pub)) != 0 ||
	    (r = sshbuf_put_bignum2(b, server_dh_pub)) != 0 ||
	    (r = sshbuf_put_bignum2(b, shared_secret)) != 0) {
		sshbuf_free(b);
		return r;
	}
#ifdef DEBUG_KEXDH
	sshbuf_dump(b, stderr);
#endif
	if (EVP_DigestInit(&md, evp_md) != 1 ||
	    EVP_DigestUpdate(&md, sshbuf_ptr(b), sshbuf_len(b)) != 1 ||
	    EVP_DigestFinal(&md, digest, NULL) != 1) {
		sshbuf_free(b);
		return SSH_ERR_LIBCRYPTO_ERROR;
	}
	sshbuf_free(b);
	*hash = digest;
	*hashlen = EVP_MD_size(evp_md);
#ifdef DEBUG_KEXDH
	dump_digest("hash", digest, *hashlen);
#endif
	return 0;
}
