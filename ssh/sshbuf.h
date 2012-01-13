/*	$OpenBSD$	*/
/*
 * Copyright (c) 2011 Damien Miller
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _SSHBUF_H

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>

#define SSHBUF_SIZE_MAX		0x20000000	/* Hard maximum size */
#define SSHBUF_MAX_BIGNUM		(8192 / 8)	/* Max bignum *bytes* */
#define SSHBUF_MAX_ECPOINT	((528 * 2 / 8) + 1) /* Max EC point *bytes* */

/*
 * NB. do not depend on the internals of this. It will be made opaque
 * one day.
 */
struct sshbuf {
	u_char *d;		/* Data */
	size_t off;		/* First available byte is buf->d + buf->off */
	size_t size;		/* Last byte is buf->d + buf->size - 1 */
	size_t max_size;	/* Maximum size of buffer */
	size_t alloc;		/* Total bytes allocated to buf->d */
	int freeme;		/* Kludge to support sshbuf_init */
};

#ifndef SSHBUF_NO_DEPREACTED
/*
 * NB. Please do not use sshbuf_init() in new code. Please use sshbuf_new()
 * instead. sshbuf_init() is deprectated and will go away soon (it is
 * only included to allow compat with buffer_* in OpenSSH)
 */
void sshbuf_init(struct sshbuf *buf);
#endif

/*
 * Create a new sshbuf buffer.
 * Returns pointer to buffer on success, or NULL on allocation failure.
 */
struct sshbuf *sshbuf_new(void);

/*
 * Clear and free buf
 */
void	sshbuf_free(struct sshbuf *buf);

/*
 * Reset buf, clearing its contents. NB. max_size is preserved.
 */
void	sshbuf_reset(struct sshbuf *buf);

/*
 * Return the maximum size of buf
 */
size_t	sshbuf_max_size(const struct sshbuf *buf);

/*
 * Set the maximum size of buf
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_set_max_size(struct sshbuf *buf, size_t max_size);

/*
 * Returns the length of data in buf
 */
size_t	sshbuf_len(const struct sshbuf *buf);

/*
 * Returns number of bytes left in buffer before hitting max_size.
 */
size_t	sshbuf_avail(const struct sshbuf *buf);

/*
 * Returns pointer to the start of the the data in buf
 */
u_char *sshbuf_ptr(const struct sshbuf *buf);

/*
 * Check whether a reservation of size len will succeed in buf
 * Safer to use than direct comparisons again sshbuf_avail as it copes
 * with unsigned overflows correctly.
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */

int	sshbuf_check_reserve(const struct sshbuf *buf, size_t len);

/*
 * Reserve len bytes in buf.
 * Returns 0 on success and a pointer to the first reserved byte via the
 * optional dpp parameter or a negative * SSH_ERR_* error code on failure.
 */
int	sshbuf_reserve(struct sshbuf *buf, size_t len, u_char **dpp);

/*
 * Consume len bytes from the start of buf
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_consume(struct sshbuf *buf, size_t len);

/*
 * Consume len bytes from the end of buf
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_consume_end(struct sshbuf *buf, size_t len);

/* Extract or deposit some bytes */
int	sshbuf_get(struct sshbuf *buf, void *v, size_t len);
int	sshbuf_put(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_putb(struct sshbuf *buf, const struct sshbuf *v);

/* Append using a printf(3) format */
int	sshbuf_putf(struct sshbuf *buf, const char *fmt, ...)
	    __attribute__((format(printf, 2, 3)));
int	sshbuf_putfv(struct sshbuf *buf, const char *fmt, va_list ap);

/* Functions to extract or store big-endian words of various sizes */
int	sshbuf_get_u64(struct sshbuf *buf, u_int64_t *valp);
int	sshbuf_get_u32(struct sshbuf *buf, u_int32_t *valp);
int	sshbuf_get_u16(struct sshbuf *buf, u_int16_t *valp);
int	sshbuf_get_u8(struct sshbuf *buf, u_char *valp);
int	sshbuf_put_u64(struct sshbuf *buf, u_int64_t val);
int	sshbuf_put_u32(struct sshbuf *buf, u_int32_t val);
int	sshbuf_put_u16(struct sshbuf *buf, u_int16_t val);
int	sshbuf_put_u8(struct sshbuf *buf, u_char val);

/*
 * Functions to extract or store SSH wire encoded strings (u32 len || data)
 * The "cstring" variants admit no \0 characters in the string contents.
 * Caller must free *valp.
 */
int	sshbuf_get_string(struct sshbuf *buf, u_char **valp, size_t *lenp);
int	sshbuf_get_cstring(struct sshbuf *buf, char **valp, size_t *lenp);
int	sshbuf_put_string(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_put_cstring(struct sshbuf *buf, const char *v);
int	sshbuf_put_stringb(struct sshbuf *buf, const struct sshbuf *v);

/*
 * "Direct" variant of sshbuf_get_string, returns pointer into the sshbuf to
 * avoid an malloc+memcpy. The pointer is guaranteed to be valid until the
 * next sshbuf-modifying function call. Caller does not free.
 */
int	sshbuf_get_string_direct(struct sshbuf *buf, const u_char **valp,
	    size_t *lenp);

/* Another variant: "peeks" into the buffer without modifying it */
int	sshbuf_peek_string_direct(const struct sshbuf *buf, const u_char **valp,
	    size_t *lenp);

/*
 * Functions to extract or store SSH wire encoded bignums and elliptic
 * curve points.
 */
int	sshbuf_get_bignum2(struct sshbuf *buf, BIGNUM *v);
int	sshbuf_get_bignum1(struct sshbuf *buf, BIGNUM *v);
int	sshbuf_get_ec(struct sshbuf *buf, EC_POINT *v, const EC_GROUP *g);
int	sshbuf_get_eckey(struct sshbuf *buf, EC_KEY *v);
int	sshbuf_put_bignum2(struct sshbuf *buf, const BIGNUM *v);
int	sshbuf_put_bignum1(struct sshbuf *buf, const BIGNUM *v);
int	sshbuf_put_ec(struct sshbuf *buf, const EC_POINT *v, const EC_GROUP *g);
int	sshbuf_put_eckey(struct sshbuf *buf, const EC_KEY *v);

/* Dump the contents of the buffer to stderr in a human-readable format */
void	sshbuf_dump(struct sshbuf *buf, FILE *f);

/* Return the hexadecimal representation of the contents of the buffer */
char	*sshbuf_dtob16(struct sshbuf *buf);

/* Encode the contents of the buffer as base64 */
char	*sshbuf_dtob64(struct sshbuf *buf);

/* Decode base64 data and append it to the buffer */
int	sshbuf_b64tod(struct sshbuf *buf, const char *b64);

/* Macros for decoding/encoding integers */
#define PEEK_U64(p) \
	(((u_int64_t)(((u_char *)(p))[0]) << 56) | \
	 ((u_int64_t)(((u_char *)(p))[1]) << 48) | \
	 ((u_int64_t)(((u_char *)(p))[2]) << 40) | \
	 ((u_int64_t)(((u_char *)(p))[3]) << 32) | \
	 ((u_int64_t)(((u_char *)(p))[4]) << 24) | \
	 ((u_int64_t)(((u_char *)(p))[5]) << 16) | \
	 ((u_int64_t)(((u_char *)(p))[6]) << 8) | \
	  (u_int64_t)(((u_char *)(p))[7]))
#define PEEK_U32(p) \
	(((u_int32_t)(((u_char *)(p))[0]) << 24) | \
	 ((u_int32_t)(((u_char *)(p))[1]) << 16) | \
	 ((u_int32_t)(((u_char *)(p))[2]) << 8) | \
	  (u_int32_t)(((u_char *)(p))[3]))
#define PEEK_U16(p) \
	(((u_int16_t)(((u_char *)(p))[0]) << 8) | \
	  (u_int16_t)(((u_char *)(p))[1]))

#define POKE_U64(p, v) \
	do { \
		((u_char *)(p))[0] = (((u_int64_t)(v)) >> 56) & 0xff; \
		((u_char *)(p))[1] = (((u_int64_t)(v)) >> 48) & 0xff; \
		((u_char *)(p))[2] = (((u_int64_t)(v)) >> 40) & 0xff; \
		((u_char *)(p))[3] = (((u_int64_t)(v)) >> 32) & 0xff; \
		((u_char *)(p))[4] = (((u_int64_t)(v)) >> 24) & 0xff; \
		((u_char *)(p))[5] = (((u_int64_t)(v)) >> 16) & 0xff; \
		((u_char *)(p))[6] = (((u_int64_t)(v)) >> 8) & 0xff; \
		((u_char *)(p))[7] = ((u_int64_t)(v)) & 0xff; \
	} while (0)
#define POKE_U32(p, v) \
	do { \
		((u_char *)(p))[0] = (((u_int64_t)(v)) >> 24) & 0xff; \
		((u_char *)(p))[1] = (((u_int64_t)(v)) >> 16) & 0xff; \
		((u_char *)(p))[2] = (((u_int64_t)(v)) >> 8) & 0xff; \
		((u_char *)(p))[3] = ((u_int64_t)(v)) & 0xff; \
	} while (0)
#define POKE_U16(p, v) \
	do { \
		((u_char *)(p))[0] = (((u_int64_t)(v)) >> 8) & 0xff; \
		((u_char *)(p))[1] = ((u_int64_t)(v)) & 0xff; \
	} while (0)

/* Internal definitions follow. Exposed for regress tests */
#ifdef SSHBUF_INTERNAL

# define SSHBUF_SIZE_INIT		256		/* Initial allocation */
# define SSHBUF_SIZE_INC		256		/* Preferred increment length */
# define SSHBUF_PACK_MIN		8192		/* Minimim packable offset */

/* # define SSHBUF_ABORT abort */
/* # define SSHBUF_DEBUG */

# ifndef SSHBUF_ABORT
#  define SSHBUF_ABORT()
# endif

# ifdef SSHBUF_DEBUG
#  define SSHBUF_TELL(what) do { \
		printf("%s:%d %s: %s size %zu alloc %zu off %zu max %zu\n", \
		    __FILE__, __LINE__, __func__, what, \
		    buf->size, buf->alloc, buf->off, buf->max_size); \
		fflush(stdout); \
	} while (0)
#  define SSHBUF_DBG(x) do { \
		printf("%s:%d %s: ", __FILE__, __LINE__, __func__); \
		printf x; \
		printf("\n"); \
		fflush(stdout); \
	} while (0)
# else
#  define SSHBUF_TELL(what)
#  define SSHBUF_DBG(x)
# endif
#endif /* SSHBUF_INTERNAL */

#endif /* _SSHBUF_H */
