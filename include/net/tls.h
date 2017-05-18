/* Copyright (c) 2016-2017, Mellanox Technologies All rights reserved.
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *      - Neither the name of the Mellanox Technologies nor the
 *        names of its contributors may be used to endorse or promote
 *        products derived from this software without specific prior written
 *        permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE
 */

#ifndef _TLS_OFFLOAD_H
#define _TLS_OFFLOAD_H

#include <linux/types.h>

#include <uapi/linux/tls.h>


/* Maximum data size carried in a TLS record */
#define TLS_MAX_PAYLOAD_SIZE		((size_t)1 << 14)

#define TLS_HEADER_SIZE			5
#define TLS_NONCE_OFFSET		TLS_HEADER_SIZE

#define TLS_CRYPTO_INFO_READY(info)	((info)->cipher_type)

#define TLS_RECORD_TYPE_DATA		0x17

/* +1 for aad, +1 for tag, +1 for chaining */
#define TLS_SG_DATA_SIZE                (MAX_SKB_FRAGS + 3)
#define ALG_MAX_PAGES 16 /* for skb_to_sgvec */
#define TLS_AAD_SPACE_SIZE		21
#define TLS_AAD_SIZE			13
#define TLS_TAG_SIZE			16

#define TLS_NONCE_SIZE			8
#define TLS_PREPEND_SIZE		(TLS_HEADER_SIZE + TLS_NONCE_SIZE)
#define TLS_OVERHEAD		(TLS_PREPEND_SIZE + TLS_TAG_SIZE)

struct tls_sw_context {
	struct sock *sk;
	void (*sk_write_space)(struct sock *sk);
	struct crypto_aead *aead_send;

	/* Sending context */
	struct scatterlist sg_tx_data[TLS_SG_DATA_SIZE];
	struct scatterlist sg_tx_preenc[ALG_MAX_PAGES + 1];
	char aad_send[TLS_AAD_SPACE_SIZE];
	char tag_send[TLS_TAG_SIZE];
	int wmem_len;
	struct scatterlist sgaad_send[2];
	struct scatterlist sgtag_send[2];
	struct sk_buff_head tx_queue;
	int unsent;
	struct sk_buff *tx_buff;
};

struct tls_context {
	union {
		struct tls_crypto_info crypto_send;
		struct tls12_crypto_info_aes_gcm_128 crypto_send_aes_gcm_128;
	};

	void *priv_ctx;

	u16 prepand_size;
	u16 tag_size;
	u16 iv_size;
	char *iv;
	u16 rec_seq_size;
	char *rec_seq;

	skb_frag_t *pending_frags;
	u16 pending_offset;

	/* This is the number of unpushed frags in the open record.
	 * tls_is_pending_open_record() should be used to determine whether
	 * we already started pushing the record and it remains open
	 * due to backpressure from the TCP layer or whether
	 * it is open due to the use of MSG_MORE OR MSG_SENDPAGE_NOTLAST.
	 */
	u16 open_record_frags;

	void (*sk_write_space)(struct sock *sk);
	void (*sk_destruct)(struct sock *sk);
	void (*sk_close)(struct sock *sk, long timeout);

	int  (*setsockopt)(struct sock *sk, int level,
			   int optname, char __user *optval,
			   unsigned int optlen);
	int  (*getsockopt)(struct sock *sk, int level,
			   int optname, char __user *optval,
			   int __user *optlen);
};

int tls_sk_query(struct sock *sk, int optname, char __user *optval,
		int __user *optlen);
int tls_sk_attach(struct sock *sk, int optname, char __user *optval,
		  unsigned int optlen);


int tls_set_sw_offload(struct sock *sk, struct tls_context *ctx);
void tls_clear_sw_offload(struct sock *sk);
int tls_sw_sendmsg(struct sock *sk, struct msghdr *msg, size_t size);
int tls_sw_sendpage(struct sock *sk, struct page *page,
		    int offset, size_t size, int flags);
void tls_sw_close(struct sock *sk, long timeout);

void tls_sk_destruct(struct sock *sk, struct tls_context *ctx);
void tls_icsk_clean_acked(struct sock *sk);

int tls_push_frags(struct sock *sk, struct tls_context *ctx,
		   skb_frag_t *frag, u16 num_frags, u16 first_offset,
		   int flags);
int tls_push_paritial_record(struct sock *sk, struct tls_context *ctx,
			     int flags);

static inline bool tls_is_pending_open_record(struct tls_context *ctx)
{
	return ctx->pending_frags != NULL;
}

static inline void tls_err_abort(struct sock *sk)
{
	xchg(&sk->sk_err, -EBADMSG);
	sk->sk_error_report(sk);
}

static inline void tls_increment_seqno(unsigned char *seq, struct sock *sk)
{
	int i;

	for (i = 7; i >= 0; i--) {
		++seq[i];
		if (seq[i] != 0)
			break;
	}

	if (i == -1)
		tls_err_abort(sk);
}

static inline void tls_fill_prepend(struct tls_context *ctx,
			     char *buf,
			     size_t plaintext_len,
			     unsigned char record_type)
{
	size_t pkt_len, iv_size = ctx->iv_size;

	pkt_len = plaintext_len + iv_size + ctx->tag_size;

	/* we cover nonce explicit here as well, so buf should be of
	 * size KTLS_DTLS_HEADER_SIZE + KTLS_DTLS_NONCE_EXPLICIT_SIZE
	 */
	buf[0] = record_type;
	buf[1] = TLS_VERSION_MINOR(ctx->crypto_send.version);
	buf[2] = TLS_VERSION_MAJOR(ctx->crypto_send.version);
	/* we can use IV for nonce explicit according to spec */
	buf[3] = pkt_len >> 8;
	buf[4] = pkt_len & 0xFF;
	memcpy(buf + TLS_NONCE_OFFSET, ctx->iv, iv_size);
}

static inline struct tls_context *tls_get_ctx(const struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	return icsk->icsk_ulp_data;
}

static inline struct tls_sw_context *tls_sw_ctx(
		const struct tls_context *tls_ctx)
{
	return (struct tls_sw_context *)tls_ctx->priv_ctx;
}

static inline struct tls_offload_context *tls_offload_ctx(
		const struct tls_context *tls_ctx)
{
	return (struct tls_offload_context *)tls_ctx->priv_ctx;
}

int tls_proccess_cmsg(struct sock *sk, struct msghdr *msg,
		      unsigned char *record_type);

#endif /* _TLS_OFFLOAD_H */
