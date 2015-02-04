/**
 * @file
 *
 * (c) CossackLabs
 */

#include <themis/secure_session_utils.h>
#include <common/error.h>

#include <soter/soter_container.h>

#include <string.h>

#include <arpa/inet.h>

#define THEMIS_SESSION_CONTEXT_TAG "TSSC"

#define SESSION_CTX_SERIZALIZED_SIZE(_CTX_) (sizeof(_CTX_->session_id) + sizeof(_CTX_->is_client) + sizeof(_CTX_->session_master_key) + sizeof(_CTX_->out_seq) + sizeof(_CTX_->in_seq))

themis_status_t secure_session_save(const secure_session_t *session_ctx, void *out, size_t *out_length)
{
	soter_container_hdr_t *hdr = out;
	uint32_t *curr;

	if ((!session_ctx) || (!out_length))
	{
		return HERMES_INVALID_PARAMETER;
	}

	if (session_ctx->state_handler)
	{
		/* Key agreement is not complete. We cannot save session state at this stage. */
		return HERMES_INVALID_PARAMETER;
	}

	/* | session_id | is_client | master_key | out_seq | in_seq | */

	if ((!out) || (*out_length < (sizeof(soter_container_hdr_t) + SESSION_CTX_SERIZALIZED_SIZE(session_ctx))))
	{
		*out_length = (sizeof(soter_container_hdr_t) + SESSION_CTX_SERIZALIZED_SIZE(session_ctx));
		return HERMES_BUFFER_TOO_SMALL;
	}

	*out_length = (sizeof(soter_container_hdr_t) + SESSION_CTX_SERIZALIZED_SIZE(session_ctx));

	soter_container_set_data_size(hdr, SESSION_CTX_SERIZALIZED_SIZE(session_ctx));
	memcpy(hdr->tag, THEMIS_SESSION_CONTEXT_TAG, SOTER_CONTAINER_TAG_LENGTH);

	curr = (uint32_t*)soter_container_data(hdr);

	/* session_id */
	*curr = htonl(session_ctx->session_id);
	curr++;

	/* is_client */
	*curr = htonl(session_ctx->is_client);
	curr++;

	/* master_key */
	memcpy(curr, session_ctx->session_master_key, sizeof(session_ctx->session_master_key));
	curr += sizeof(session_ctx->session_master_key) / sizeof(uint32_t);

	/* out_seq */
	*curr = htonl(session_ctx->out_seq);
	curr++;

	/* in_seq */
	*curr = htonl(session_ctx->in_seq);

	soter_update_container_checksum(hdr);

	return HERMES_SUCCESS;
}

themis_status_t secure_session_load(secure_session_t *session_ctx, const void *in, size_t in_length)
{
	const soter_container_hdr_t *hdr = in;
	soter_status_t soter_res;
	themis_status_t res;
	const uint32_t *curr;

	if ((!session_ctx) || (!in))
	{
		return HERMES_INVALID_PARAMETER;
	}

	if (in_length < sizeof(soter_container_hdr_t))
	{
		return HERMES_INVALID_PARAMETER;
	}

	if (in_length < (sizeof(soter_container_hdr_t) + soter_container_data_size(hdr)))
	{
		return HERMES_INVALID_PARAMETER;
	}

	soter_res = soter_verify_container_checksum(hdr);
	if (HERMES_SUCCESS != soter_res)
	{
		return (themis_status_t)soter_res;
	}

	memset(session_ctx, 0, sizeof(session_ctx));
	curr = (const uint32_t *)soter_container_const_data(hdr);

	session_ctx->session_id = ntohl(*curr);
	curr++;

	session_ctx->is_client = ntohl(*curr);
	curr++;

	memcpy(session_ctx->session_master_key, curr, sizeof(session_ctx->session_master_key));
	curr += sizeof(session_ctx->session_master_key) / sizeof(uint32_t);

	/* We have to derive session keys before extracting sequence numbers, because this function overwrites them */
	res = secure_session_derive_message_keys(session_ctx);
	if (HERMES_SUCCESS != res)
	{
		return res;
	}

	session_ctx->out_seq = ntohl(*curr);
	curr++;

	session_ctx->in_seq = ntohl(*curr);

	return HERMES_SUCCESS;
}