
#include "stream_context.h"

struct stream_context *create_stream_context(int streamid)
{
    struct stream_context *stream_ctx;

    stream_ctx = malloc(sizeof(struct stream_context));
    memset(stream_ctx, 0, sizeof(struct stream_context));
    stream_ctx->stream_id = streamid;

    return stream_ctx;
}

void delete_stream_context(struct stream_context* strm_ctx)
{
    free(strm_ctx);
}