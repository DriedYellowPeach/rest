#include "stream_context.h"
#include <string.h>
#include <stdlib.h>
#include "request.h"
#include "response.h"

struct stream_context *stream_context_create(int streamid)
{
    struct stream_context *stream_ctx;

    stream_ctx = malloc(sizeof(struct stream_context));
    memset(stream_ctx, 0, sizeof(struct stream_context));
    stream_ctx->stream_id = streamid;

    return stream_ctx;
}

void stream_context_destroy(struct stream_context* strm_ctx)
{
    request_destroy(strm_ctx->req);
    response_destroy(strm_ctx->resp);
    free(strm_ctx);
}