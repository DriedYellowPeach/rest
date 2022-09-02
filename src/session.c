#include "session.h"
#include <netinet/tcp.h>
#include <event2/buffer.h>
#include "ioevent.h"
#include "server.h"
#include "log.h"
#include "stream_context.h"

// TODO: error handling when create bev failed!
struct session *
create_session(struct server *svr, int fd, struct sockaddr *addr, int addrlen)
{
  struct session *ses;
  struct bufferevent *bev;
  int val = 1;
  int ret;
  char host[NI_MAXHOST];

  ses = malloc(sizeof(struct session));
  memset(ses, 0, sizeof(struct session));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val));
  bev = apply_bufferevent(svr->eg, fd, readcb, writecb, eventcb, (void *)ses);
  ses->bev = bev;

  ret = getnameinfo(addr, (socklen_t)addrlen, host, sizeof(host), NULL, 0,
                    NI_NUMERICHOST);
  if (ret != 0)
  {
    ses->client_addr = strdup("(unknown)");
  }
  else
  {
    ses->client_addr = strdup(host);
  }
  return ses;
}


// TODO: other callbacks like on_frame_not_send_callback
void initialize_nghttp2_session(struct session *ses)
{
  nghttp2_session_callbacks *callbacks;

  nghttp2_session_callbacks_new(&callbacks);

  nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
  nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers_callback);
  nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);
  nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
}

// TODO: be sure to free all the stream_ctx in the session;
void delete_session(struct session *ses)
{
  // TODO free all the stream_context stored in the map
  free(ses);
}

int hold_stream_context(struct session *ses, int stream_id)
{
  struct stream_context *stream_ctx;
  stream_ctx = create_stream_context(stream_id);
  // TODO a map to put the stream context in

  return 0;
}

int release_stream_context(struct session *ses, struct stream_context *strm_ctx)
{
  list_remove_stream_context(ses, strm_ctx);
  delete_stream_context(strm_ctx);

  return 0;
}

static void list_append_stream_context(struct session *ses, struct stream_context *stream_ctx) {
  // TODO link list add
}

// TODO link list remove
static void list_remove_stream_context(struct session *ses, struct stream_context *stream_ctx) {

}

static int session_recv(struct session *ses)
{
  ssize_t readlen;
  struct evbuffer *input = bufferevent_get_input(ses->bev);
  size_t datalen = evbuffer_get_length(input);
  unsigned char *data = evbuffer_pullup(input, -1);

  readlen = nghttp2_session_mem_recv(ses->ngsession, data, datalen);
  if (readlen < 0)
  {
    log_err("Fatal error: %s", nghttp2_strerror((int)readlen));
    return -1;
  }
  if (evbuffer_drain(input, (size_t)readlen) != 0)
  {
    log_err("Fatal error: evbuffer_drain failed");
    return -1;
  }
  if (session_send(ses) != 0)
  {
    return -1;
  }
  return 0;
}

static int session_send(struct session *ses)
{
  int rv;
  rv = nghttp2_session_send(ses->ngsession);
  if (rv != 0)
  {
    log_err("Fatal error: %s", nghttp2_strerror(rv));
    return -1;
  }
  return 0;
}

static void readcb(struct bufferevent *bev, void *ptr)
{
  struct session *ses = (struct session *)ptr;
  (void)bev;

  if (session_recv(ses) != 0)
  {
    delete_session(ses);
    return;
  }
}

static void writecb(struct bufferevent *bev, void *ptr)
{
  struct session *ses = (struct session *)ptr;
  if (evbuffer_get_length(bufferevent_get_output(bev)) > 0)
  {
    return;
  }
  if (nghttp2_session_want_read(ses->ngsession) == 0 &&
      nghttp2_session_want_write(ses->ngsession) == 0)
  {
    delete_session(ses);
    return;
  }
  if (session_send(ses) != 0)
  {
    delete_sessoin(ses);
    return;
  }
}

static void eventcb(struct bufferevent *bev, short events, void *ptr)
{
  struct session *ses = (struct session *)ptr;
  if (events & BEV_EVENT_CONNECTED)
  {
    initialize_nghttp2_session(ses);
    log_info("%s connected", ses->client_addr);
    if (send_server_connection_header(ses) != 0 ||
        session_send(ses) != 0)
    {
      delete_session(ses);
      return;
    }

    return;
  }

  if (events & BEV_EVENT_EOF)
  {
    log_info("%s EOF", ses->client_addr);
  }
  else if (events & BEV_EVENT_ERROR)
  {
    log_info("%s network error", ses->client_addr);
  }
  else if (events & BEV_EVENT_TIMEOUT)
  {
    log_info("%s timeout", ses->client_addr);
  }

  delete_session(ses);
}

// These are callback needed by nghttps_session

/*
** send_callback will be called when nghttp2_session_send be called, it can be called automatically,
** that is why we need send_callback to send response frame.
** send_callback here do real I/O to bufferevent.
*/
static ssize_t send_callback(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data)
{
  struct session *ses = (struct session *)user_data;
  struct bufferevent *bev = ses->bev;
  (void)session;
  (void)flags;

  /* Avoid excessive buffering in server side. */
  if (evbuffer_get_length(bufferevent_get_output(ses->bev)) >=
      OUTPUT_WOULDBLOCK_THRESHOLD)
  {
    return NGHTTP2_ERR_WOULDBLOCK;
  }
  bufferevent_write(bev, data, length);
  return (ssize_t)length;
}

/*
** on_begin_headers_callback will be called when the first header arrived in ngsession
** this will cause session to create new stream_ctx
*/
static int on_begin_headers_callback(nghttp2_session *ngsession, const nghttp2_frame *frame, void *user_data)
{
  struct session *ses = (struct session *)user_data;
  // TODO: fullfill struct stream_context
  struct stream_context *stream_ctx;

  if (frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_REQUEST)
  {
    return 0;
  }

  // TODO: function add_stream_context

  stream_ctx = add_stream_context(ses, frame->hd.stream_id);
  // set stream_ctx for later callback for stream(id) on this session.
  nghttp2_session_set_stream_user_data(ngsession, frame->hd.stream_id,
                                       stream_ctx);
  return 0;
}

/*
** on_header_callback will be called if a full header received.
** for one kind header, coressponding request field will be set.
*/
static int 
on_header_callback(nghttp2_session *ngsession, const nghttp2_frame *frame, 
                   const uint8_t *name, size_t namelen, 
                   const uint8_t *value, size_t valuelen, 
                   uint8_t flags, void *user_data)
{
  struct stream_context *stream_ctx;

  // TODO: for every kind of header
  // method scheme authority path host...
  // set request's field.
}

/* 
** on_frame_recv_callback will be called if one kind of frame is received
** there are two kinds of frame: NGHTTP2_DATA and NGHTTP2_HEADERS
** NGHTTP2_DATA means data fully received? no
** NGHTTP2_HEADERS means headers fully received? yes
** so we can call mux to get endpoint handler to handle(register cb)
** since http2 has one headers frame and multiple data frame
*/
static int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
  switch (frame->hd.type) {
    case NGHTTP2_DATA:

    break;
    case NGHTTP2_HEADERS: 

    break;
  }

  return 0;
}

/*
** on_data_chunk_recv_callback will be called if a data chunk in data frame be received
** req with registerd callback will be called with fixed length byte array
*/
static int on_data_chunk_recv_callback(nghttp2_session *ses, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data) 
{
  struct stream_context *strm_ctx = nghttp2_session_get_stream_user_data(ses, stream_id);

  // TODO: use stram_ctx to get the request, and call req.call_on_data(data, len);
  return 0;
}


static int on_stream_close_callback(nghttp2_session *ses, int32_t stream_id, uint32_t error_code, void *user_data)
{
  struct stream_context *strm_ctx = nghttp2_session_get_stream_user_data(ses, stream_id);
  // TODO: user strm_ctx to get the response, and call resp.call_on_close(err_code);

  // TODO: unhold the strm_ctx from session, and free the strm_ctx
}

