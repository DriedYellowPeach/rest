#include "session.h"

struct stream_context {
    int stream_id;
    struct session *ses;
    // TODO: Other fields: request, response, and other.
};

struct stream_context *create_stream_context(int streamid);