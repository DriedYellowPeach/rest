#ifndef SESSION_H
#define SESSION_H

#include<nghttp2/nghttp2.h>
#include <event2/bufferevent.h>

#define OUTPUT_WOULDBLOCK_THRESHOLD (1 << 16)

struct session {
    nghttp2_session *ngsession;
    char *client_addr;
    struct bufferevnt *bev;
};

struct session *
create_session(struct server *svr, int fd, struct sockaddr *addr, int addrlen);

void delete_session(struct session *ses);

int hold_stream_context(struct session *ses, int stream_id);

int release_stream_context(struct session *ses, struct stream_context *strm_ctx);

void initialize_nghttp2_session(struct session *ses);

#endif

