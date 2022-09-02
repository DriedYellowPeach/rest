#ifndef STREAM_CONTEXT_H
#define STREAM_CONTEXT_H

#include "session.h"

struct stream_context {
    int stream_id;
    struct session *ses;
    // TODO: Other fields: request, response, and other.
};

struct stream_context *create_stream_context(int streamid);

void delete_stream_context(struct stream_context *strm_ctx);

#endif