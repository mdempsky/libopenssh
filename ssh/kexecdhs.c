/* $OpenBSD: kexecdhs.c,v 1.2 2010/09/22 05:01:29 djm Exp $ */
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2010 Damien Miller.  All rights reserved.
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
#include <string.h>
#include <signal.h>

#include <openssl/ecdh.h>

#include "xmalloc.h"
#include "buffer.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "dispatch.h"
#include "compat.h"
#include "err.h"

static int input_kex_ecdh_init(int, u_int32_t, struct ssh *);

void
kexecdh_server(struct ssh *ssh)
{
	debug("expecting SSH2_MSG_KEX_ECDH_INIT");
	ssh_dispatch_set(ssh, SSH2_MSG_KEX_ECDH_INIT, &input_kex_ecdh_init);
}

static int
input_kex_ecdh_init(int type, u_int32_t seq, struct ssh *ssh)
{
	Kex *kex = ssh->kex;
	EC_POINT *client_public;
	EC_KEY *server_key;
	const EC_GROUP *group;
	BIGNUM *shared_secret;
	struct sshkey *server_host_private, *server_host_public;
	u_char *server_host_key_blob = NULL, *signature = NULL;
	u_char *kbuf, *hash;
	u_int klen, slen, sbloblen, hashlen;
	int curve_nid, r;

	if ((curve_nid = kex_ecdh_name_to_nid(kex->name)) == -1)
		fatal("%s: unsupported ECDH curve \"%s\"", __func__, kex->name);
	if ((server_key = EC_KEY_new_by_curve_name(curve_nid)) == NULL)
		fatal("%s: EC_KEY_new_by_curve_name failed", __func__);
	if (EC_KEY_generate_key(server_key) != 1)
		fatal("%s: EC_KEY_generate_key failed", __func__);
	group = EC_KEY_get0_group(server_key);

#ifdef DEBUG_KEXECDH
	fputs("server private key:\n", stderr);
	sshkey_dump_ec_key(server_key);
#endif

	if (kex->load_host_public_key == NULL ||
	    kex->load_host_private_key == NULL)
		fatal("Cannot load hostkey");
	server_host_public = kex->load_host_public_key(kex->hostkey_type, ssh);
	if (server_host_public == NULL)
		fatal("Unsupported hostkey type %d", kex->hostkey_type);
	server_host_private = kex->load_host_private_key(kex->hostkey_type, ssh);
	if (server_host_private == NULL)
		fatal("Missing private key for hostkey type %d",
		    kex->hostkey_type);

	if ((client_public = EC_POINT_new(group)) == NULL)
		fatal("%s: EC_POINT_new failed", __func__);
	ssh_packet_get_ecpoint(ssh, group, client_public);
	ssh_packet_check_eom(ssh);

	if (sshkey_ec_validate_public(group, client_public) != 0)
		fatal("%s: invalid client public key", __func__);

#ifdef DEBUG_KEXECDH
	fputs("client public key:\n", stderr);
	sshkey_dump_ec_point(group, client_public);
#endif

	/* Calculate shared_secret */
	klen = (EC_GROUP_get_degree(group) + 7) / 8;
	kbuf = xmalloc(klen);
	if (ECDH_compute_key(kbuf, klen, client_public,
	    server_key, NULL) != (int)klen)
		fatal("%s: ECDH_compute_key failed", __func__);

#ifdef DEBUG_KEXDH
	dump_digest("shared secret", kbuf, klen);
#endif
	if ((shared_secret = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);
	if (BN_bin2bn(kbuf, klen, shared_secret) == NULL)
		fatal("%s: BN_bin2bn failed", __func__);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	/* calc H */
	if ((r = sshkey_to_blob(server_host_public, &server_host_key_blob,
	    &sbloblen)) != 0)
		fatal("%s: sshkey_to_blob: %s", __func__, ssh_err(r));
	kex_ecdh_hash(
	    kex->evp_md,
	    group,
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    server_host_key_blob, sbloblen,
	    client_public,
	    EC_KEY_get0_public_key(server_key),
	    shared_secret,
	    &hash, &hashlen
	);
	EC_POINT_clear_free(client_public);

	/* save session id := H */
	if (kex->session_id == NULL) {
		kex->session_id_len = hashlen;
		kex->session_id = xmalloc(kex->session_id_len);
		memcpy(kex->session_id, hash, kex->session_id_len);
	}

	/* sign H */
	if (PRIVSEP(sshkey_sign(server_host_private, &signature, &slen,
	    hash, hashlen, datafellows)) < 0)
		fatal("kexdh_server: sshkey_sign failed");

	/* destroy_sensitive_data(); */

	/* send server hostkey, ECDH pubkey 'Q_S' and signed H */
	ssh_packet_start(ssh, SSH2_MSG_KEX_ECDH_REPLY);
	ssh_packet_put_string(ssh, server_host_key_blob, sbloblen);
	ssh_packet_put_ecpoint(ssh, group, EC_KEY_get0_public_key(server_key));
	ssh_packet_put_string(ssh, signature, slen);
	ssh_packet_send(ssh);

	xfree(signature);
	xfree(server_host_key_blob);
	/* have keys, free server key */
	EC_KEY_free(server_key);

	kex_derive_keys(ssh, hash, hashlen, shared_secret);
	BN_clear_free(shared_secret);
	kex_finish(ssh);
	return 0;
}
