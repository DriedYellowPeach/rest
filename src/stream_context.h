#ifndef STREAM_CONTEXT_H
#define STREAM_CONTEXT_H

//#include "session.h"
#include "request.h"
#include "response.h"

struct stream_context {
    int stream_id;
    //struct session *ses;
    struct stream_context *prev, *next;
    // TODO: Other fields: request, response, and other.
    char *request_path;
    int fd;
    struct request *req;
    struct resposne *resp; 
};

struct stream_context *stream_context_create(int streamid);

void stream_context_destroy(struct stream_context *strm_ctx);

#endif